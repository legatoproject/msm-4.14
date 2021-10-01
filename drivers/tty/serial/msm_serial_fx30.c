/*
 * RS485 extension for msm7k serial driver on FX30S platform
 *
 * Copyright (C) 2021 Sierra Wireless, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * Sierra Wireless FX30S device has SN65HVD72DR and SN65C3238 TI devices that
 * work in conjuction with UART1 serial interface on WP modules to achieve
 * RS485 functionality. This interface is implemented as half-duplex where
 * RXD and DCD lines of the RS232 DB9 are reused as D+ and D- lines of RS485.
 * Communication is half-duplex, so there needs to be an optional time delay
 * when switching between Rx and Tx modes.
 *
 * RXD and TXD signals of the UART are routed both to SN65C3238 and SN65HVD72DR
 * devices. The interface comes up as RS232 but it can be switched into RS485
 * mode by toggling the FORCEOFF_RS232_N GPIO GPIO, which would decouple the
 * RS232 function in SN65C3238 and enable RS485 function in SN65HVD72DR.
 *
 * Once RS485 is turned on, the interface would be in Rx state. Whenever there
 * is data to transmit, it switches to Tx state by toggling RS485_IN_EN and
 * RS485_OUT_EN_N GPIOs. It then remains in Tx state while data is transmitted
 * and switched back to Rx when data is exhausted. If FX30S is terminating a
 * multi-drop line, a 120ohm termination resistor can be enabled by toggling
 * RS485_TERM_N GPIO.
 *
 * Delays are imposed whenever RS485 is switched between Rx and Tx states:
 * Rx and Tx control delays to stabilize the signals as per SN65HVD72DR data
 * sheet, as well as optional delays for flushing data at various baud rates.
 *
 * This driver implements the following functionality:
 * - Toggling between RS232 and RS485 modes by writing to rs_mode sysfs entry
 * - Enabling RS485 mode from modem's AT!MAPUART setting
 * - Enabling 120ohm termination resistor by writing to rs485_term sysfs entry
 * - Toggling between RS485 Tx/Rx modes in driver's start_tx()/stop_tx()
 *   routines
 * - Device tree entries for GPIOs and delays
 *
 * TODO: Implement TIOCSRS485 and TIOCGRS485 ioctl() callbacks.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/bitfield.h>

struct rs485conf {
	int txonb;			/* RS485_OUT_EN_N GPIO */
	int rxon;			/* RS485_IN_EN GPIO */
	int forceoff_rs232b;		/* FORCEOFF_RS232_N GPIO */
	int rs485_termb;		/* RS485_TERM_N GPIO */
	unsigned int tx_udelay;		/* TX delay in usec */
	unsigned int rx_udelay;		/* RX delay in usec */
};

/* Use padding area in struct serial_rs485 to store custom data */
#define conf padding[0]

static inline void __uart_irq_disable(struct uart_port *up)
{
	unsigned long irqflags;

	spin_lock_irqsave(&up->lock, irqflags);
	msm_write(up, 0, UART_IMR); /* disable interrupt */
	spin_unlock_irqrestore(&up->lock, irqflags);
}

static inline void __uart_irq_enable(struct uart_port *up)
{
	unsigned long irqflags;
	struct msm_port *mp = UART_TO_MSM(up);

	spin_lock_irqsave(&up->lock, irqflags);
	msm_write(up, mp->imr, UART_IMR); /* re-enable interrupt */
	spin_unlock_irqrestore(&up->lock, irqflags);
}

/* Called inside start_tx() driver routine before transmission starts */
static void msm_rs485_txon(struct uart_port *up)
{
	struct rs485conf *rs485 = (struct rs485conf *)up->rs485.conf;

	if (!rs485)
		/* Not configured yet */
		return;

	if (!up->rs485.flags & SER_RS485_ENABLED)
		/* Not in RS485 mode */
		return;

	/* Disable rx */
	gpio_set_value(rs485->rxon, 0);
	udelay(rs485->rx_udelay);

	/* Turn on tx */
	gpio_set_value(rs485->txonb, 0);
	udelay(rs485->tx_udelay);

	/* Optional delay before transmitting */
	if (up->rs485.flags & SER_RS485_RTS_ON_SEND)
		mdelay(up->rs485.delay_rts_before_send);

	return;
}

/* Called inside stop_tx() driver routine when no more data is available */
static void msm_rs485_txoff(struct uart_port *up)
{
	struct rs485conf *rs485 = (struct rs485conf *)up->rs485.conf;
	unsigned int timeout = 500000;

	if (!rs485)
		/* Not configured yet */
		return;

	if (!up->rs485.flags & SER_RS485_ENABLED)
		/* Not in RS485 mode */
		return;

	/* Flush Tx FIFO */
	while (!msm_tx_empty(up)) {
		udelay(1);
		cpu_relax();
		if (!timeout--)
			BUG();
	}
	msm_write(up, UART_CR_CMD_RESET_TX_READY, UART_CR);

	/* Optional delay before transmit is disabled */
	if (up->rs485.flags & SER_RS485_RTS_AFTER_SEND)
		mdelay(up->rs485.delay_rts_after_send);

	/* Turn off tx */
	gpio_set_value(rs485->txonb, 1);
	udelay(rs485->tx_udelay);

	/* Enable rx */
	gpio_set_value(rs485->rxon, 1);
	udelay(rs485->rx_udelay);

	return;
}

#define SER_RS485_ENABLE_MASK  (SER_RS485_ENABLED | \
				SER_RS485_RTS_ON_SEND |\
				SER_RS485_RTS_AFTER_SEND)
/* Configure GPIOs for RS485 mode */
static void __setup_rs485(struct device *dev)
{
	struct uart_port *up = dev_get_drvdata(dev);
	struct rs485conf *rs485 = (struct rs485conf *)up->rs485.conf;

	/* Called with UART interrupts disabled */
	gpio_set_value_cansleep(rs485->forceoff_rs232b, 0);

	gpio_set_value(rs485->txonb, 1);
	udelay(rs485->tx_udelay);
	gpio_set_value(rs485->rxon, 1);
	udelay(rs485->rx_udelay);
	up->rs485.flags |= SER_RS485_ENABLE_MASK;
}

/* Configure GPIOs for RS232 mode */
static void __setup_rs232(struct device *dev)
{
	struct uart_port *up = dev_get_drvdata(dev);
	struct rs485conf *rs485 = (struct rs485conf *)up->rs485.conf;

	/* Called with UART interrupts disabled */
	up->rs485.flags &= ~SER_RS485_ENABLE_MASK;
	gpio_set_value(rs485->txonb, 1);
	gpio_set_value(rs485->rxon, 0);
	udelay(rs485->rx_udelay);

	gpio_set_value_cansleep(rs485->forceoff_rs232b, 1);
	gpio_set_value_cansleep(rs485->rs485_termb, 1);
}

enum serial_rs_mode {
	SERIAL_RS232,			/* Default mode RS232*/
	SERIAL_RS485,			/* RS485 mode */
};

static ssize_t rs_mode_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct uart_port *up = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", (up->rs485.flags & SER_RS485_ENABLED ?
			"RS485" : "RS232"));
}

static ssize_t rs_mode_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct uart_port *up = dev_get_drvdata(dev);
	unsigned int val;
	bool enable;

	val = simple_strtoul(buf, NULL, 0);

	/* Protect from invalid input */
	switch (val) {
	case SERIAL_RS232:
		enable = false;
		break;

	case SERIAL_RS485:
		enable = true;
		break;

	default:
		return -EINVAL;
	}

	if (!!FIELD_GET(SER_RS485_ENABLED, up->rs485.flags) == enable)
		/* Already set, nothing to do */
		return count;

	__uart_irq_disable(up);
	if (enable)
		__setup_rs485(dev);
	else
		__setup_rs232(dev);
	__uart_irq_enable(up);

	return count;
}

DEVICE_ATTR(rs_mode, S_IWUSR | S_IRUGO, rs_mode_show, rs_mode_store);

static ssize_t rs485_term_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int val, ret;
	struct uart_port *up = dev_get_drvdata(dev);
	struct rs485conf *rs485 = (struct rs485conf *)up->rs485.conf;

	if (!rs485)
		/* Not configured yet */
		return -EAGAIN;

	val = gpio_get_value_cansleep(rs485->rs485_termb);
	if (val < 0)
		return val;

	ret = sprintf(buf, (!val ? "ENABLED\n" : "DISABLED\n"));
	return ret;
}

static ssize_t rs485_term_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int val;
	struct uart_port *up = dev_get_drvdata(dev);
	struct rs485conf *rs485 = (struct rs485conf *)up->rs485.conf;

	if (!rs485)
		/* Not configured yet */
		return -EAGAIN;

	val = simple_strtoul(buf, NULL, 0);
	if (val > 1)
		return -EINVAL;

	__uart_irq_disable(up);
	gpio_set_value_cansleep(rs485->rs485_termb, !val);
	__uart_irq_enable(up);

	return count;
}

DEVICE_ATTR(rs485_term, S_IWUSR | S_IRUGO, rs485_term_show, rs485_term_store);

static int msm_probe_rs485_gpios(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct uart_port *up = platform_get_drvdata(pdev);
	struct rs485conf rs485, *conf;
	u32 rs485_udelay[2], rs485_mdelay[2];
	int ret = -ENODEV;

	/* Acquire GPIOs and configure them for RS232 as default */
	rs485.forceoff_rs232b = of_get_named_gpio(np,
					"sierra,forceoff-rs232b-gpio",0);
	if (!gpio_is_valid(rs485.forceoff_rs232b))
		return ret;
	ret = devm_gpio_request_one(&pdev->dev, rs485.forceoff_rs232b,
					GPIOF_OUT_INIT_HIGH, "forceoff-rs232b");
	if (unlikely(ret < 0))
		return ret;

	rs485.rs485_termb = of_get_named_gpio(np, "sierra,rs485-termb-gpio",0);
	if (!gpio_is_valid(rs485.rs485_termb))
		goto bail_forceoff_rs232b;
	ret = devm_gpio_request_one(&pdev->dev, rs485.rs485_termb,
					GPIOF_OUT_INIT_HIGH, "rs485-termb");
	if (unlikely(ret < 0))
		goto bail_forceoff_rs232b;

	rs485.txonb = of_get_named_gpio(np, "sierra,rs485-txonb-gpio", 0);
	if (!gpio_is_valid(rs485.txonb))
		goto bail_rs485_termb;
	ret = devm_gpio_request_one(&pdev->dev, rs485.txonb,
					GPIOF_OUT_INIT_HIGH, "rs485-txonb");
	if (unlikely(ret < 0))
		goto bail_rs485_termb;

	rs485.rxon = of_get_named_gpio(np, "sierra,rs485-rxon-gpio", 0);
	if (!gpio_is_valid(rs485.rxon))
		goto bail_txonb;
	ret = devm_gpio_request_one(&pdev->dev, rs485.rxon,
					GPIOF_OUT_INIT_LOW, "rs485-rxon");
	if (unlikely(ret < 0))
		goto bail_txonb;

	/*
	 * If low-speed UART probe function has not assigned a port, it
	 * means the UART is configured as high speed. We still need
	 * GPIOs left in their default state to facilitate RS232 communication,
	 * so exit here.
	 */
	if (!up) {
		/* GPIOs configured for high-speed UART */
		dev_info(&pdev->dev, "high-speed UART configured, no RS485\n");
		return 0;
	}

	/* Continue with setting up RS485-capable low-speed UART */
	conf = (struct rs485conf *)kzalloc(sizeof(rs485), GFP_KERNEL);
	if (!conf) {
		ret = -ENOMEM;
		goto bail_gpios;
	}

	/* Signal stabilization delays as per SN65HVD72DR data sheet */
	if (of_property_read_u32_array(np, "sierra,rs485-udelay",
					rs485_udelay, 2) == 0) {
		conf->tx_udelay = rs485_udelay[0];
		conf->rx_udelay = rs485_udelay[1];
	}

	conf->forceoff_rs232b = rs485.forceoff_rs232b;
	conf->rs485_termb = rs485.rs485_termb;
	conf->txonb = rs485.txonb;
	conf->rxon = rs485.rxon;
	up->rs485.conf = (unsigned int)conf;

	/* Optional delays when toggling Rx/Tx modes */
	if (of_property_read_u32_array(np, "rs485-rts-delay",
					rs485_mdelay, 2) == 0) {
		up->rs485.delay_rts_before_send = rs485_mdelay[0];
		up->rs485.delay_rts_after_send = rs485_mdelay[1];
	}

	/* Sysfs entries */
	ret = device_create_file(&pdev->dev, &dev_attr_rs_mode);
	if (unlikely(ret))
		goto bail_free;

	ret = device_create_file(&pdev->dev, &dev_attr_rs485_term);
	if (unlikely(ret))
		goto bail_free;

	/* UART is already up, let's disable interrupts*/
	__uart_irq_disable(up);

	/* Switch to RS485 mode if configured with AT!MAPUART */
	if (uart_is_function_rs485(&pdev->dev))
		__setup_rs485(&pdev->dev);
	else
		__setup_rs232(&pdev->dev);

	/* Done, it's safe to re-enable interrupts now */
	__uart_irq_enable(up);

	dev_info(&pdev->dev, "RS485 GPIOs: Tx:%d Rx:%d RS232OFF:%d TERM:%d\n",
			conf->txonb, conf->rxon,
			conf->forceoff_rs232b,
			conf->rs485_termb);
	dev_info(&pdev->dev, "RS485 delays: %u (Tx), %u (Rx)\n",
		conf->tx_udelay, conf->rx_udelay);

	return ret;

bail_free:
	kfree(conf);
	up->rs485.conf = 0;

/* Unwind GPIOs */
bail_gpios:
	devm_gpio_free(&pdev->dev, rs485.rxon);
bail_txonb:
	devm_gpio_free(&pdev->dev, rs485.txonb);
bail_rs485_termb:
	devm_gpio_free(&pdev->dev, rs485.rs485_termb);
bail_forceoff_rs232b:
	devm_gpio_free(&pdev->dev, rs485.forceoff_rs232b);

	return ret;
}

/* Bus number and address of tca6424 GPIO expander on FX30S */
#define TCA6424_BUSNUM  (4)
#define TCA6424_ADDRESS (0x22)

static int msm_probe_rs485(struct platform_device *pdev)
{
        struct i2c_adapter *adap;

	/* Check if this is FX30S: detect tca6424 on I2C bus 4 */
        adap = i2c_get_adapter(TCA6424_BUSNUM);
        if (!adap)
                return -ENODEV;

        if (!i2c_probe_func_quick_read(adap, TCA6424_ADDRESS))
                return -ENODEV;

        i2c_put_adapter(adap);

	return msm_probe_rs485_gpios(pdev);
}

#define RS485_UART "78b0000.serial"
static int __init msm_serial_rs485_init(void)
{
	struct device *dev;

	dev = bus_find_device_by_name(&platform_bus_type, NULL, RS485_UART);
	if (dev)
		msm_probe_rs485(to_platform_device(dev));

	return 0;
}

device_initcall_sync(msm_serial_rs485_init);
