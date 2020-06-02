/************
*
* Filename:  mcidefs.h
*
* Purpose:   Internal definitions for Micro-Controller Interface module
*
* Copyright: (c) 2016 Sierra Wireless
*            All rights reserved
*
************/

#ifndef MCIDEFS_H
#define MCIDEFS_H

#include <linux/mfd/swimcu/core.h>
#include <linux/mfd/swimcu/mciprotocol.h>

#define MCI_EVENT_LIST_SIZE_MAX      \
  (MCI_PROTOCOL_GENERIC_RESP_RESULT_COUNT_MAX - MCI_PROTOCOL_GENERIC_RESP_RESULT_COUNT_MIN)

/************
 *
 * Name:     mci_adc_state_e - MCU pin ADC operation state
 *
 * Purpose:  Define the state of the ADC operation state
 *
 * Members:  See below
 *
 * Note:     An ADC operation can be started by calling init() with user-defined
 *           configuration or start() with exiting configuration.
 *           ADC must not be started while the previous one not completed yet.
 *           The state of an ADC operation state is not known until a read is
 *           performed. Hence, a read() or uninit() must be performed after
 *           start an ADC operation before next init() or start().
 *
 ************/
enum mci_adc_state_e
{
  MCI_ADC_STATE_NONE,     /* ADC powered down */
  MCI_ADC_STATE_STARTED,  /* ADC started */
  MCI_ADC_STATE_SUCCESS,  /* ADC completed successfully */
  MCI_ADC_STATE_ERROR,    /* ADC failed */
};

/************
*
* Name:     mci_mcu_pin_function_e
*
* Purpose:  Enumerate MCU pin function
*
* Members:  See below
*
* Note:     none
*
************/
enum mci_mcu_pin_function_e {

  MCI_MCU_PIN_FUNCTION_DISABLED = 0,   /* Disabled (as analog signal) */
  MCI_MCU_PIN_FUNCTION_GPIO     = 1,   /* Configured as GPIO.*/
  MCI_MCU_PIN_FUNCTION_Alt2     = 2,   /* pin-specific */
  MCI_MCU_PIN_FUNCTION_Alt3     = 3,   /* pin-specific */
  MCI_MCU_PIN_FUNCTION_Alt4     = 4,   /* pin-specific */
  MCI_MCU_PIN_FUNCTION_Alt5     = 5,   /* pin-specific */
  MCI_MCU_PIN_FUNCTION_Alt6     = 6,   /* pin-specific */
  MCI_MCU_PIN_FUNCTION_Alt7     = 7    /* pin-specific */
};

/************
*
* Name:     mci_mcu_pin_direction_e
*
* Purpose:  Enumerate MCU pin direction
*
* Members:  See below
*
* Note:     none
*
************/
enum mci_mcu_pin_direction_e {

  MCI_MCU_PIN_DIRECTION_INPUT  = 0,
  MCI_MCU_PIN_DIRECTION_OUTPUT = 1
};

/************
*
* Name:     mci_mcu_pin_pull_e
*
* Purpose:  Enumerate MCU pull select
*
* Members:  See below
*
* Note:     none
*
************/
enum mci_mcu_pin_pull_select_e {
    MCI_MCU_PIN_PULL_DOWN = 0,  /* internal pull-down resistor is enabled. */
    MCI_MCU_PIN_PULL_UP   = 1   /* internal pull-up resistor is enabled.  */
};

/************
*
* Name:     mci_pin_irqc_type_e
*
* Purpose:  Enumerate MCU pin interrupt type.
*
* Members:  See below
*
* Note:     none
*
************/
enum mci_pin_irqc_type_e {

  MCI_PIN_IRQ_DISABLED     = 0x0,  /* Interrupt/DMA request is disabled.*/
  MCI_PIN_IRQ_LOGIC_ZERO   = 0x8,  /* Interrupt when logic zero. */
  MCI_PIN_IRQ_RISING_EDGE  = 0x9,  /* Interrupt on rising edge. */
  MCI_PIN_IRQ_FALLING_EDGE = 0xA,  /* Interrupt on falling edge. */
  MCI_PIN_IRQ_EITHER_EDGE  = 0xB,  /* Interrupt on either edge. */
  MCI_PIN_IRQ_LOGIC_ONE    = 0xC   /* Interrupt when logic one. */
};

/************
*
* Name:     mci_mcu_pin_slew_rate_e
*
* Purpose:  Enumerate MCU output pin slew rate.
*
* Members:  See below
*
* Note:     none
*
************/
enum mci_mcu_pin_slew_rate_e {

    MCI_MCU_PIN_SLEW_RATE_FAST = 0,    /* fast slew rate is configured */
    MCI_MCU_PIN_SLEW_RATE_SLOW = 1     /* slow slew rate is configured */
};

/************
*
* Name:     mci_mcu_pin_level_e
*
* Purpose:  Enumerate logic levls of an MCU pin.
*
* Members:  See below
*
* Note:     none
*
************/
enum mci_mcu_pin_level_e
{
  MCI_MCU_PIN_LEVEL_LOW  = 0,
  MCI_MCU_PIN_LEVEL_HIGH = 1,
};

/************
*
* Name:     mci_mcu_pin_drive_strength_e
*
* Purpose:  Enumerate MCU output pin drive strength.
*
* Members:  See below
*
* Note:     none
*
************/
enum mci_mcu_pin_drive_strength_e {

    MCI_MCU_PIN_DRIVE_STRENGTH_LOW  = 0, /* low drive strength is configured. */
    MCI_MCU_PIN_DRIVE_STRENGTH_HIGH = 1  /* high drive strength is configured.*/
};


/************
*
* Name:     mci_adc_resolution_e - digital resolution of ADC
*
* Purpose:  Enumerate the digital resolution of ADC
*
* Members:  See below
*
* Note:
*
************/
enum mci_adc_resolution_e
{
  MCI_ADC_RESOLUTION_8_BITS  = 8,   /* 8-bit for single end sample  */
  MCI_ADC_RESOLUTION_10_BITS = 10,  /* 10-bit for single end sample */
  MCI_ADC_RESOLUTION_12_BITS = 12,  /* 12-bit for single end sample */
  MCI_ADC_RESOLUTION_DEFAULT = MCI_ADC_RESOLUTION_8_BITS
};

/* max ADC values for different resolution modes */
#define MCI_ADC_RESOLUTION_8_VALUE_MAX        255
#define MCI_ADC_RESOLUTION_10_VALUE_MAX       1023
#define MCI_ADC_RESOLUTION_12_VALUE_MAX       4095

/* The delta between the low and high thresholds of the range
 * TO be consulted with HW team and tuned to sensible values
 */
#define MCI_ADC_RESOLUTION_8_RANGE_SIZE_MIN   5
#define MCI_ADC_RESOLUTION_10_RANGE_SIZE_MIN  10
#define MCI_ADC_RESOLUTION_12_RANGE_SIZE_MIN  20

/************
*
* Name:     mci_adc_average_mode_e - ADC average operation mode
*
* Purpose:  Enumerate average mmode of ADC operation
*
* Members:  See below
*
* Note:     In SW average mode, upon completion of each sample, software is
*           interrupted to the collect the conversion result of the sample
*           and to start next sampling until all samples are collected. The
*           number of samples are collected are 1 ~255
*           Two sample collection and average modes are available on MCU:
*           In HW average mode, samples is collected and averaged by HW
*           without SW intervention until completion but the number of
*           samples collected are limited to a few finite numbers: 4, 8, 16 and 32
*
************/
enum mci_adc_avarage_mode_e
{
  MCI_ADC_AVERAGE_MODE_SOFTWARE = 0,
  MCI_ADC_AVERAGE_MODE_HARDWARE = 1
};

#define MCI_ADC_SW_AVERAGE_SAMPLES_MIN    1
#define MCI_ADC_SW_AVERAGE_SAMPLES_MAX    255

/* In HW average mode, The number of samples collected and averaged are discrete */
#define MCI_ADC_HW_AVERAGE_SAMPLES_4      4
#define MCI_ADC_HW_AVERAGE_SAMPLES_8      8
#define MCI_ADC_HW_AVERAGE_SAMPLES_16     16
#define MCI_ADC_HW_AVERAGE_SAMPLES_32     32

/************
*
* Name:     mci_adc_speed_e
*
* Purpose:  Enumerate conversion speed.
*
* Members:  See below
*
* Note:     Configure slower conversion for less power consumption
*           or for better accuracy.
*
************/
enum mci_adc_conversion_speed_e
{
    MCI_ADC_CONVERSION_SPEED_SLOW   = -1,
    MCI_ADC_CONVERSION_SPEED_NORMAL =  0,
    MCI_ADC_CONVERSION_SPEED_FAST   = +1,
};

/************
*
*
* Name:     mci_adc_sample_period_e
* Purpose:  Enumerate ADC sample period extension.
*
* Members:  See below
*
* Note:     Extended sample period may improve accuracy
*           for high impedance analog inputs
*
************/
enum mci_adc_sample_period_e
{
  MCI_ADC_SAMPLE_PERIOD_NORMAL = 0,    /* no extension */
  MCI_ADC_SAMPLE_PERIOD_EXTENDED_1,    /* longest extended period */
  MCI_ADC_SAMPLE_PERIOD_EXTENDED_2,    /* longer extended period */
  MCI_ADC_SAMPLE_PERIOD_EXTENDED_3,    /* long extended period */
  MCI_ADC_SAMPLE_PERIOD_EXTENDED_4,    /* short extended period */
};

/************
*
* Name:     mci_adc_trigger_mode_e - ADC operation trigger
*
* Purpose:  Enumerate how samples are collected and averaged to get the value
*
* Members:  See below
*
* Note:     Only SOFTWARE trigger is currently supported
*
************/
enum mci_adc_trigger_mode_e
{
  MCI_ADC_TRIGGER_MODE_SOFTWARE    = 0, /* ADC triggered by SW */
  MCI_ADC_TRIGGER_MODE_RTC_ALARM   = 1, /* ADC triggered by a RTC alarm event */
  MCI_ADC_TRIGGER_MODE_TIMER_EVENT = 2, /* ADC triggered by timeout event */
};

/************
*
* Name:     mci_adc_compare_e - how ADC is compared
*
* Purpose:  Enumerate the way to compare ADC result with specified value(s)
*
* Members:  See below
*
* Note:     The specified value(s) are inclusive in all comparisons.
*
************/
enum mci_adc_compare_e
{
  MCI_ADC_COMPARE_ABOVE = 1, /* trigger event if excede the specified value */
  MCI_ADC_COMPARE_BELOW,     /* trigger event if drop below the specified value */
  MCI_ADC_COMPARE_BEYOND,    /* trigger event if goes beyond the specified range */
  MCI_ADC_COMPARE_WITHIN,    /* trigger event if falls within the specifiedrange */
};

/************
*
* Name:     mci_timer_tick_e - mci timer tick length
*
* Purpose:  Enumerate the duration of a timer tick for MCI_WAKEUP_SOURCE_TYPE_TIMER_2
*
* Members:  See below
*
* Note:     For MCI_WAKEUP_SOURCE_TYPE_TIMER_1, the tick width is fixed at ONE milli-second.
*
************/
enum mci_timer_tick_e
{
  MCI_TIMER_TICK_MS_1 = 1, /* 1 tick = 1 millisecond */
  MCI_TIMER_TICK_MS_2,     /* 1 tick = 2 millisecond */
  MCI_TIMER_TICK_MS_4,     /* 1 tick = 4 millisecond */
  MCI_TIMER_TICK_MS_8,     /* 1 tick = 8 millisecond */
  MCI_TIMER_TICK_MS_16,    /* 1 tick = 16 millisecond */
  MCI_TIMER_TICK_MS_32,    /* 1 tick = 32 millisecond */
  MCI_TIMER_TICK_MS_64,    /* 1 tick = 64 millisecond */
  MCI_TIMER_TICK_MS_128,   /* 1 tick = 128 millisecond */
  MCI_TIMER_TICK_MS_256,   /* 1 tick = 256 millisecond */
  MCI_TIMER_TICK_MS_512,   /* 1 tick = 512 millisecond */
  MCI_TIMER_TICK_MS_1024,  /* 1 tick = 1024 millisecond */
  MCI_TIMER_TICK_MS_2048,  /* 1 tick = 2048 millisecond */
  MCI_TIMER_TICK_MS_4096,  /* 1 tick = 4096 millisecond */
  MCI_TIMER_TICK_MS_8192,  /* 1 tick = 8192 millisecond */
  MCI_TIMER_TICK_MS_16384, /* 1 tick = 16384 millisecond */
  MCI_TIMER_TICK_MS_32768, /* 1 tick = 32768 millisecond */
  MCI_TIMER_TICK_MS_65536, /* 1 tick = 65536 millisecond */
  MCI_TIMER_TICK_MS_MAX = MCI_TIMER_TICK_MS_65536
};

/* maximum tiemout values (in milli seconds) for low power timer */
#define MCI_TIMER2_COUNT_MAX  65535

/************
*
* Name:     mci_mcu_power_mode_e - mci MCU power mode
*
* Purpose:  TO enumerate the powermode available for MCU
*
* Members:  See below
*
* Note:
*
************/
enum mci_mcu_power_mode_e
{
  MCI_MCU_POWER_MODE_RUN  = 0,       /* Normal Run mode*/
  MCI_MCU_POWER_MODE_WAIT,           /* Core clock is gated off */

  MCI_MCU_POWER_MODE_STOP = 5,       /* Core clock gated off; system and bus clock gated off */
  MCI_MCU_POWER_MODE_STOP_1,         /* Partial Stop with both system and bus clocks disabled */
  MCI_MCU_POWER_MODE_STOP_2,         /* Partial Stop with system clock disabled and bus clock enabled */
  MCI_MCU_POWER_MODE_VLLS3,          /* 1. core/system and bus/flash clocks are gated off
                                      * 2. The MCU is placed in a low leakage mode .
                                      * 3. I/O states are held. */
  MCI_MCU_POWER_MODE_VLLS1,          /* 4. Further powering down all system RAM */
  MCI_MCU_POWER_MODE_VLLS0,          /* 5. Further disable the 1kHz LPO clock */
  MCI_MCU_POWER_MODE_VLLS0_POR_OFF,  /* 6. Further Disable the power on reset (POR) circuit */
  MCI_MCU_POWER_MODE_MAX,
  MCI_MCU_POWER_MODE_INVALID = MCI_MCU_POWER_MODE_MAX
};

/* The following macros define the bit mask represents the conditions for
 * powering on MDM when MCU wakeup from a low power mode, in case the MDM was
 * powered down when MCU went into the low power mode.
 */
#define MCI_PM_MDM_PWRUP_CONDS_BIT_MASK_ADC      0X1000
#define MCI_PM_MDM_PWRUP_CONDS_BIT_MASK_TIMER1   0X0100
#define MCI_PM_MDM_PWRUP_CONDS_BIT_MASK_TIMER2   0X0200

#define MCI_PM_MDM_PWRUP_CONDS_BIT_MASK_GPIOS    0X00FF

/************
*
* Name:     mci_adc_compare_scheme_s
*
* Purpose:  Define data structure for ADC range configuration
*
* Members:  See below
*
* Note:     none
*/
struct mci_adc_compare_scheme_s
{
  enum mci_protocol_adc_compare_mode_e mode;
  uint16_t value1;
  uint16_t value2;
};

/************
*
* Name:     mci_adc_callback_config_s
*
* Purpose:  To define user callback configuration data structure .
*
 * Members: See below; for decription of the members of enum type, see their definitions.
 *
 * Note:
 *
 ************/
typedef void (*mci_adc_callback_t) (void* user_data, uint16_t adc);

struct mci_adc_callback_config_s
{
	mci_adc_callback_t  callback;     /* user callback for ADC completion */
	void               *user_data;    /* optional accompanied user data */
};

/************
*
* Name:     mci_external_wakeup_pins_s
*
* Purpose:  Define data structure for external pin as wakeup sources
*
* Members:  See below
*
* Note:     none
*/
struct mci_external_wakeup_pins_s
{
  char** names; /* pointer to an array of GPIO input pin names */
  int    count; /* number of GPIO input pins in the array */
};

/************
*
* Name:     mci_adc_config_s
*
* Purpose:  Define data structure for ADC configuration
*
* Members:  See below
*
* Note:     none
*/
struct mci_adc_config_s
{
  enum mci_protocol_adc_channel_e           channel;          /* ADC analog input channel ID */
  enum mci_protocol_adc_low_power_conv_e    low_power_conv;   /* lower power conversion mode */
  enum mci_protocol_adc_sample_period_adj_e sample_period;    /* extended sample period adjustment */
  enum mci_protocol_adc_resolution_mode_e   resolution_mode;  /* conversion result resolution */
  enum mci_protocol_adc_high_speed_conv_e   high_speed_conv;  /* faster conversion mode */
  enum mci_protocol_adc_trigger_mode_e      trigger_mode;     /* conversion trigger mode */
  enum mci_protocol_adc_trigger_e           trigger_type;
  uint16_t                                  trigger_interval;
  uint8_t                                   sample_count;
  bool                                      hw_average;
  struct mci_adc_compare_scheme_s           hw_compare;
  mci_adc_callback_t                        callback;
  void*                                     user_data;
  enum mci_adc_state_e                      state;
};

/************
*
* Name:     mci_adc_compare_config_s
*
* Purpose:  Define data structure for ADC range configuration
*
* Members:  See below
*
* Note:     none
*/
struct mci_adc_compare_config_s
{
  enum mci_protocol_adc_channel_e               channel;
  struct mci_adc_compare_scheme_s               compare;
  unsigned int                                  interval; /* ADC sampling interval */
};

/************
*
* Name:     mci_wakeup_source_config_s
*
* Purpose:  Define internal data structure of a wakeup source
*
* Members:  See below
*
* Note:     none
*/
struct mci_wakeup_source_config_s
{
  enum mci_protocol_wakeup_source_type_e source_type; /* source type */
  union
  {
    uint32_t                           pins;     /* external pin (in bitmask) */
    uint32_t                           timeout;  /* timer wakeup  */
    uint32_t                           channel;   /* ADC wakeup */
  } args;
};

/************
*
* Name:     mci_pm_profile_config_s
*
* Purpose:  Define internal data structure for power management profile
*
* Members:  See below
*
* Note:     none
*/
struct mci_pm_profile_config_s
{
  enum mci_protocol_power_mode_e active_power_mode;       /* power mode of MCU while running */
  enum mci_protocol_power_mode_e standby_power_mode;      /* power mode of MCU in standby */
  enum mci_protocol_mdm_state_e  standby_mdm_state;       /* MDM on/off in low power mode */
  uint16_t                       standby_wakeup_sources;  /* conditions to wake from low power */
  uint16_t                       mdm_on_conds_bitset_any; /* reserved */
  uint16_t                       mdm_on_conds_bitset_all; /* reserved */
  uint16_t                       active_idle_time;        /* time before MCU drops to standby */
};

/************
*
* Name:     mci_event_s - MCU event data structure
*
* Purpose:  Define C structure for MCU event
*
* Members:  See below
*
* Note:    none
*
************/
struct mci_event_s
{
  enum mci_protocol_event_type_e type;  /* event type */

  union {
    struct
    {
      enum mci_mcu_pin_level_e level;   /* logic level of the GPIO */
      uint8_t port;                     /* port number of the GPIO */
      uint8_t pin;                      /* pin number of the GPIO */
    } gpio_irq;                         /* GPIO pin IRQ event data */

    struct
    {
      enum mci_protocol_adc_channel_e adch;       /* ADC channel ID */
      uint16_t                        value;      /* ADC value */
    } adc;                                        /* ADC event data */

    struct
    {
      enum mci_protocol_reset_source_e source;
    } reset;

    struct
    {
      enum mci_protocol_wakeup_source_type_e type;
      uint16_t                               value;
    } wusrc;
  } data;                               /* event data */
};

/************
*
* Name:     mci_pin_irq_config_s - MCU GPIO pin interrupt request configuration
*
* Purpose:  Define data structure for user to configure GPIO pin interrupt request
*
* Members:  See below.
*
* Note:     A limited number of GPIO pins support IRQ: PTA0, PTA7 and PTB0
*           If the interrupt type control 'irqc' is not DISABLED, the handler
*           'irq_callback' must be a valid pointer to user provided callback.
*
************/
typedef void (*mci_pin_irq_callback_t)(void*, enum mci_mcu_pin_level_e);
struct mci_pin_irq_config_s
{
  enum mci_pin_irqc_type_e type_control;  /* interrupt type control */
  mci_pin_irq_callback_t   user_callback; /* user provided callback to handle IRQ */
  void*                    user_data;     /* user data accompanied with the callback */
};

/************
*
* Name:     mci_mcu_pin_state_s
*
* Purpose:  Define data structure for an MCU pin state
*
* Members:  See below
*
* Note:     none
*/
struct mci_mcu_pin_state_s
{
  enum mci_mcu_pin_function_e    mux;   /* fucntion mux */
  enum mci_mcu_pin_direction_e   dir;   /* signal direction  */
  enum mci_mcu_pin_level_e       level; /* logic level */

  union
  {
    struct _input
    {
      bool                            pe;   /* enable or disable internal pull */
      enum mci_mcu_pin_pull_select_e  ps;   /* pull resistor select (up/down) */
      bool                            pfe;  /* passive filter enable */
      enum mci_pin_irqc_type_e        irqc_type; /* IRQ control type */
    } input;

    struct _output
    {
      enum mci_mcu_pin_slew_rate_e      sre;   /* fast slew rate enable */
      enum mci_mcu_pin_drive_strength_e dse;   /* high drive strenth enable */

    } output;

  } params;
};

/* mciprotocol.c */
extern enum mci_protocol_status_code_e
            mci_protocol_command(struct swimcu *swimcu, enum mci_protocol_command_tag_e cmd,
                                 uint32_t params[], uint8_t max_count,
                                 uint8_t *countp, uint8_t flags);

extern enum mci_protocol_status_code_e
	    swimcu_ping(struct swimcu *swimcu);

extern enum mci_protocol_status_code_e
            swimcu_to_boot_transit(struct swimcu *swimcu);

extern enum mci_protocol_status_code_e
            swimcu_pin_states_get(struct swimcu *swimcu, uint8_t port_number, uint8_t pin_number,
                                    struct mci_mcu_pin_state_s * statep);
extern enum mci_protocol_status_code_e
            swimcu_pin_config_set(struct swimcu *swimcu, uint8_t port_number, uint8_t pin_number,
                                   struct mci_mcu_pin_state_s * configp);

extern enum mci_protocol_status_code_e
            swimcu_adc_init(struct swimcu *swimcu, struct mci_adc_config_s* configp);

extern enum mci_protocol_status_code_e
            swimcu_adc_deinit(struct swimcu *swimcu);

extern enum mci_protocol_status_code_e
            swimcu_adc_get(struct swimcu *swimcu, enum mci_protocol_adc_channel_e   channel,
                              uint16_t *valuep);

extern enum mci_protocol_status_code_e swimcu_wakeup_source_config(
                                struct swimcu *swimcu,
                                struct mci_wakeup_source_config_s          *configp,
                                enum   mci_protocol_wakeup_source_optype_e  optype);

extern enum mci_protocol_status_code_e swimcu_adc_restart(
                                  struct swimcu *swimcu,
                                  enum mci_protocol_adc_channel_e   channel);

extern enum mci_protocol_status_code_e swimcu_pm_profile_config(
                                struct swimcu *swimcu,
                                struct mci_pm_profile_config_s  *configp,
                                enum   mci_protocol_pm_optype_e  optype);

extern enum mci_protocol_status_code_e swimcu_event_query(
                                struct swimcu *swimcu,
                                struct mci_event_s *eventp,
                                int                *countp);

extern enum mci_protocol_status_code_e swimcu_pm_wait_time_config(
				struct swimcu* swimcu,
				uint32_t wait_sync_time,
				uint32_t wait_pwr_off_time);

enum mci_protocol_status_code_e swimcu_psm_sync_config(
				struct swimcu *swimcu,
				uint32_t sync_option,
				uint32_t wait_time,
				uint32_t mdm_off_time);

extern enum mci_protocol_status_code_e swimcu_pm_pwr_off(
				struct swimcu* swimcu);

/*
 * MCU GPIO set/get IRQ trigger value.
 */
extern int swimcu_gpio_set_trigger(int gpio, enum mci_pin_irqc_type_e type);

extern enum mci_pin_irqc_type_e swimcu_gpio_get_trigger(int gpio);

#endif  /* MCIDEFS_H */
