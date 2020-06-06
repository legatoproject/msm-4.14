/************
 *
 * $Id$
 *
 * Filename:  sierra_gpio_wake_n.c
 *
 * Purpose:
 *
 * Copyright: (c) 2016 Sierra Wireless, Inc.
 *            All rights reserved
 *
 ************/
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/sierra_gpio.h>

struct wake_n_pdata {
	int gpio;
	char name[64];
	int irq;
	struct wakeup_source ws;
	struct platform_device *pdev;
	struct work_struct check_work;
	spinlock_t lock;
} wake_n_pdata;

static RAW_NOTIFIER_HEAD(wake_chain);

/************
 *
 * Name:     sierra_gpio_wake_notifier_register
 *
 * Purpose:  register notifier block pointer
 *
 * Parms:    nb - notifier block pointer
 *
 * Return:   0 if success
 *           -ERRNO otherwise
 *
 * Abort:    none
 *
 ************/
int __ref sierra_gpio_wake_notifier_register(struct notifier_block *nb)
{
	int ret = 0;

	pr_info("%s", __func__);
	spin_lock(&wake_n_pdata.lock);
	ret = raw_notifier_chain_register(&wake_chain, nb);
	spin_unlock(&wake_n_pdata.lock);
	return ret;
}
EXPORT_SYMBOL(sierra_gpio_wake_notifier_register);

/************
 *
 * Name:     sierra_gpio_wake_notifier_unregister
 *
 * Purpose:  unregister the notifier block pointer
 *
 * Parms:    nb - notifier block pointer
 *
 * Return:   none
 *
 * Abort:    none
 *
 ************/
void __ref sierra_gpio_wake_notifier_unregister(struct notifier_block *nb)
{
	spin_lock(&wake_n_pdata.lock);
	raw_notifier_chain_unregister(&wake_chain, nb);
	spin_unlock(&wake_n_pdata.lock);
}
EXPORT_SYMBOL(sierra_gpio_wake_notifier_unregister);

/************
 *
 * Name:     wake_notify
 *
 * Purpose:  notify the registered module when wake
 *
 * Parms:    none
 *
 * Return:   none
 *
 * Abort:    none
 *
 ************/
static int wake_notify(void)
{
	int ret;

	ret = raw_notifier_call_chain(&wake_chain, 0, NULL);
	return notifier_to_errno(ret);
}

/************
 *
 * Name:     gpio_check_and_wake
 *
 * Purpose:  check specfic gpio and trigger the wake notifier action
 *
 * Parms:    work - work queue pointer
 *
 * Return:   none
 *
 * Abort:    none
 *
 ************/
static void gpio_check_and_wake(struct work_struct *work)
{
	int err, gpioval;
	struct wake_n_pdata *w;
	char event[16], *envp[2];

	w = container_of(work, struct wake_n_pdata, check_work);
	gpioval = gpio_get_value(w->gpio);
	sprintf(event, "STATE=%s", (gpioval ? "SLEEP" : "WAKEUP"));
	pr_info("%s: %s %s\n", __func__, w->name, event);

	if (wake_chain.head != NULL) {
		if (0 == gpioval)
		{
			wake_notify();
		}
	} else {
		envp[0] = event;
		envp[1] = NULL;
		kobject_get(&w->pdev->dev.kobj);
		if ((err = kobject_uevent_env(&w->pdev->dev.kobj, KOBJ_CHANGE, envp)))
		{
			pr_err("%s: error %d signaling uevent\n", __func__, err);
		}
		kobject_put(&w->pdev->dev.kobj);
	}
	if (gpioval)
		__pm_relax(&w->ws);
}

/************
 *
 * Name:     gpio_wake_input_irq_handler
 *
 * Purpose:  gpio intterrupt handler
 *
 * Parms:    irq - interrupt id
 *           dev_id - device pointer
 *
 * Return:   IRQ_HANDLED
 *
 * Abort:    none
 *
 ************/
static irqreturn_t gpio_wake_input_irq_handler(int irq, void *dev_id)
{
	struct wake_n_pdata *w = (struct wake_n_pdata*)dev_id;

	/*
	 * The gpio_check_and_wake routine calls kobject_uevent_env(),
	 * which might sleep, so cannot call it from interrupt context.
	 */
	__pm_stay_awake(&w->ws);
	schedule_work(&w->check_work);
	return IRQ_HANDLED;
}

static int wake_n_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np;

	if (!pdev) {
		pr_err("%s: NULL input parameter\n", __func__);
		return -EINVAL;
	}
	dev_info(&pdev->dev, "wake_n probe\n");

	np = pdev->dev.of_node;
	wake_n_pdata.gpio = of_get_named_gpio(np, "wake-n-gpio", 0);
	if (!gpio_is_valid(wake_n_pdata.gpio)) {
		pr_err("%s: invalid wake-n-gpio value %d\n", __func__,
			wake_n_pdata.gpio);
		return -EINVAL;
	}

	ret = gpio_request(wake_n_pdata.gpio, "WAKE_N_GPIO");
	if (ret) {
		pr_err("%s: failed to get GPIO%d, error code is %d\n", __func__,
			wake_n_pdata.gpio,ret);
		return ret;
	}

	snprintf(wake_n_pdata.name, sizeof(wake_n_pdata.name), "wake-n_gpio%d",
		wake_n_pdata.gpio);
	if (gpio_direction_input(wake_n_pdata.gpio)) {
		pr_err("%s: failed to set GPIO%d to input\n", __func__,
			wake_n_pdata.gpio);
		goto release_gpio;
	}
	if (gpio_pull_up(gpio_to_desc(wake_n_pdata.gpio))) {
		pr_err("%s: failed pulling up GPIO%d\n", __func__,
			wake_n_pdata.gpio);
		goto release_gpio;
	}

	wake_n_pdata.irq = gpio_to_irq(wake_n_pdata.gpio);
	if(wake_n_pdata.irq < 0){
		pr_err("%s: no IRQ associated with GPIO%d\n", __func__,
			wake_n_pdata.gpio);
		goto release_gpio;
	}
	spin_lock_init(&wake_n_pdata.lock);
	wakeup_source_init(&wake_n_pdata.ws, "wake-n_GPIO");
	wake_n_pdata.pdev = pdev;
	INIT_WORK(&wake_n_pdata.check_work, gpio_check_and_wake);

	ret = request_irq(wake_n_pdata.irq, gpio_wake_input_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			wake_n_pdata.name, &wake_n_pdata);

	if (ret) {
		pr_err("%s: request_irq failed for GPIO%d (IRQ%d)\n", __func__,
			wake_n_pdata.gpio, wake_n_pdata.irq);
		goto release_gpio;
	}

	ret = enable_irq_wake(wake_n_pdata.irq);
	if (ret) {
		pr_err("%s: enable_irq failed for GPIO%d\n", __func__,
			wake_n_pdata.gpio);
		goto free_irq;
	}

	__pm_stay_awake(&wake_n_pdata.ws);
	schedule_work(&wake_n_pdata.check_work);

	return 0;

free_irq:
	free_irq(wake_n_pdata.irq, NULL);
release_gpio:
	gpio_free(wake_n_pdata.gpio);
	return ret;
}

static int wake_n_remove(struct platform_device *pdev)
{
	pr_info("wake_n_remove");
	gpio_free(wake_n_pdata.gpio);
	wakeup_source_trash(&wake_n_pdata.ws);
	return 0;
}

static const struct of_device_id sierra_gpio_wake_n_table[] = {
	{ .compatible = "sierra,gpio_wake_n" },
	{},
};
MODULE_DEVICE_TABLE(of, sierra_gpio_wake_n);

static struct platform_driver wake_n_driver = {
	.driver = {
		.name = "sierra_gpio_wake_n",
		.owner = THIS_MODULE,
		.of_match_table = sierra_gpio_wake_n_table,
		},
	.probe = wake_n_probe,
	.remove = wake_n_remove,
};

static int __init wake_n_init(void)
{
	return platform_driver_register(&wake_n_driver);
}

/* init early so consumer modules can complete system boot */
subsys_initcall(wake_n_init);

static void __exit wake_n_exit(void)
{
	platform_driver_unregister(&wake_n_driver);
}

module_exit(wake_n_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("GPIO wake_n pin driver");
