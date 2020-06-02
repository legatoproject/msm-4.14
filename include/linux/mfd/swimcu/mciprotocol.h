/************
*
* Filename:  mciprotocol.h
*
* Purpose:   Interface declaration of Micro-Controller Interface Communication Protocol
*
* Note:      For details of the protocol format, please see Chapter 11,
*            KL03 Sub-family reference Manual, Freescale.
*            Application specific protocol items are defined by Sierra.

* Copyright: (c) 2016 Sierra Wireless, Inc.
*            All rights reserved
*
************/

#ifndef MCIPROTOCOL_H
#define MCIPROTOCOL_H

#include <linux/platform_device.h>

/***********************
 * Constants and Enums *
 ***********************/
/* Application target device  */
#define MCI_PROTOCOL_APPL_TARGET                  1

/* Application MCI protocol version numbers */
#define MCI_PROTOCOL_VERSION_0                    0

/* VERSION_1 adds <TARGET> and <OPTION> fields to PING_RESP */
#define MCI_PROTOCOL_VERSION_1                    1

/* Application I2C address   */
#define MCI_PROTOCOL_APPL_I2C_ADDR                0x3A

/* Application's optional functionality represented in a bitmask
*  (avalible with MCI_PROTOCOL_VERSION_1 and later)
*/
#define MCI_PROTOCOL_APPL_OPT_FUNC_NONE           0x0000
#define MCI_PROTOCOL_APPL_OPT_FUNC_WATCHDOG       0x0001

#define MCI_PROTOCOL_APPL_OPT_FUNC_PSM_SYNC_1     0x0002
#define MCI_PROTOCOL_APPL_OPT_FUNC_PSM_SYNC_2     0x0004
#define MCI_PROTOCOL_APPL_OPT_FUNC_PSM_SYNC_3     0x0008
#define MCI_PROTOCOL_APPL_OPT_FUNC_PSM_SYNC_ALL   0x000E

/* All frames start with this special byte */
#define MCI_PROTOCOL_FRAME_START_BYTE             0x5A

/* Index of frame header fields into flat buffer */
#define MCI_PROTOCOL_FRAME_HEADER_START           0
#define MCI_PROTOCOL_FRAME_HEADER_TYPE            1
#define MCI_PROTOCOL_FRAME_HEADER_PAYLOAD_LEN_LO  2
#define MCI_PROTOCOL_FRAME_HEADER_PAYLOAD_LEN_HI  3
#define MCI_PROTOCOL_FRAME_HEADER_CRC16_LO        4
#define MCI_PROTOCOL_FRAME_HEADER_CRC16_HI        5

/* PING response frame has a special format:
*  Followings are the index into encoded flat buffer defined in
*  MCI_PROTOCOL_VERSION_1.
*
* Note: In MCI_PROTOCOL_VERSION_0, same format is used.but with
*       the following bytes reserved (not used):
*         MCI_PROTOCOL_FRAME_PING_RESP_TARGET     5
*         MCI_PROTOCOL_FRAME_PING_RESP_OPT_LO     6
*         MCI_PROTOCOL_FRAME_PING_RESP_OPT_HI     7
        The fIirst two field are identical to other frame's
*         MCI_PROTOCOL_FRAME_HEADER_START         0
*         MCI_PROTOCOL_FRAME_HEADER_TYPE          1
*/
#define MCI_PROTOCOL_FRAME_PING_RESP_PROT_VER     2
#define MCI_PROTOCOL_FRAME_PING_RESP_FWVER_MINOR  3
#define MCI_PROTOCOL_FRAME_PING_RESP_FWVER_MAJOR  4
#define MCI_PROTOCOL_FRAME_PING_RESP_TARGET       5
#define MCI_PROTOCOL_FRAME_PING_RESP_OPT_LO       6
#define MCI_PROTOCOL_FRAME_PING_RESP_OPT_HI       7
#define MCI_PROTOCOL_FRAME_PING_RESP_CRC16_LO     8
#define MCI_PROTOCOL_FRAME_PING_RESP_CRC16_HI     9

#define MCI_PROTOCOL_FRAME_PING_RESP_LEN          10

/* Length of special frames in short format (PING, ACK, NAK, ...) */
#define MCI_PROTOCOL_FRAME_SHORT_LEN              2

/* Length of regular frames (COMMAND, DATA, ...) */
#define MCI_PROTOCOL_FRAME_HEADER_LEN             6
#define MCI_PROTOCOL_FRAME_PAYLOAD_LEN_MAX        32
#define MCI_PROTOCOL_FRAME_LEN_MAX                (MCI_PROTOCOL_FRAME_HEADER_LEN + \
                                                   MCI_PROTOCOL_FRAME_PAYLOAD_LEN_MAX)

#define MCI_PROTOCOL_PACKET_LEN_MAX               MCI_PROTOCOL_FRAME_PAYLOAD_LEN_MAX

/* Protocol packet field index in a flat buffer
 * (All fields of the command packet header are type of uint8_t)
 */
#define MCI_PROTOCOL_PACKET_HEADER_FIELD_TAG      0
#define MCI_PROTOCOL_PACKET_HEADER_FIELD_FLAGS    1
#define MCI_PROTOCOL_PACKET_HEADER_FIELD_RSVD     2
#define MCI_PROTOCOL_PACKET_HEADER_FIELD_COUNT    3
#define MCI_PROTOCOL_PACKET_HEADER_LEN            4

/* Limiteed by max payload size of 32 bytes, a packet data payload max to 28 bytes
*  or 7 parameters of uint32_t type in a COMMAND frame (exclude 4 bytes of header)
*/
#define MCI_PROTOCOL_PACKET_PAYLOAD_LEN_MAX       (MCI_PROTOCOL_PACKET_LEN_MAX - \
                                                   MCI_PROTOCOL_PACKET_HEADER_LEN)
#define MCI_PROTOCOL_CMD_PARAMS_COUNT_MAX          7

/* GENERIC RESP has at least two parameters: status and corresponding */
#define MCI_PROTOCOL_GENERIC_RESP_RESULT_COUNT_MIN  2

/* For MCU applications, GENERIC RESP is extended to optionally include results
 * for the corresponding command frame, if applicable, up to five 32-bit data,
 * limited by max number of results (7 32-bit data) per packet payload and
 * exclude first two 32-bit data for mandatory status and command tag.
 */
#define MCI_PROTOCOL_GENERIC_RESP_RESULT_COUNT_MAX  7
#define MCI_PROTOCOL_GENERIC_RESP_RESULT_PARAMS_MAX 5

/************
*
* Name:     mci_protocol_frame_type_e - type of a protocol frame
*
* Purpose:  Enumerate the MCI protocol frame type
*
* Members:  See below
*
************/
enum mci_protocol_frame_type_e
{
  MCI_PROTOCOL_FRAME_TYPE_INVALID                 = 0x00,

  /* ROM Bootloader and application shares the same PING frame type */
  MCI_PROTOCOL_FRAME_TYPE_PING_REQ                = 0xA6,
  MCI_PROTOCOL_FRAME_TYPE_PING_RESP               = 0xA7,

  /* application specific */
  MCI_PROTOCOL_FRAME_TYPE_APPL_ACK                = 0xB1,
  MCI_PROTOCOL_FRAME_TYPE_APPL_NAK                = 0xB2,
  MCI_PROTOCOL_FRAME_TYPE_APPL_ACK_ABORT          = 0xB3,
  MCI_PROTOCOL_FRAME_TYPE_APPL_COMMAND            = 0xB4,
  MCI_PROTOCOL_FRAME_TYPE_APPL_DATA               = 0xB5,
};

/************
*
* Name:     mci_protocol_command_tag_e - Command tags for protocol command
*
* Purpose:  Enumerate the MCI protocol command tag
*
* Members:  See below
*
* Note:     Bootloader specific tags are defined in Chapter 11,
*           KL03 Sub-family reference Manual, Freescale
*           Application specific command tags are defined by Sierra.
*
************/
enum mci_protocol_command_tag_e
{
  MCI_PROTOCOL_COMMAND_TAG_INVALID                      = 0x00,

  MCI_PROTOCOL_COMMAND_TAG_GENERIC_RESP                 = 0xA0,

  /* Application specific commands */
  MCI_PROTOCOL_COMMAND_TAG_APPL_START_BOOTLOADER        = 0xB8, /* To start boot loader */
  MCI_PROTOCOL_COMMAND_TAG_APPL_TEST                    = 0xB9, /* To perform a test */
  MCI_PROTOCOL_COMMAND_TAG_APPL_EVENT_SERVICE           = 0xC0, /* To retrieve event */
  MCI_PROTOCOL_COMMAND_TAG_APPL_PM_SERVICE              = 0xC1, /* To configure power managment parameters on MCU */
  MCI_PROTOCOL_COMMAND_TAG_APPL_WAKEUP_SOURCE           = 0xC2, /* To configure a wakeup source for MCU to exit from low power mode */
  MCI_PROTOCOL_COMMAND_TAG_APPL_TIMER_SERVICE           = 0xC3, /* To configure a timer service */
  MCI_PROTOCOL_COMMAND_TAG_APPL_RESERVED_0              = 0xC4, /* Reserved for future use */
  MCI_PROTOCOL_COMMAND_TAG_APPL_PIN_SERVICE             = 0xC5, /* To configure settings for a specific pin */
  MCI_PROTOCOL_COMMAND_TAG_APPL_RESERVED_1              = 0xC6, /* Reserved for future use */
  MCI_PROTOCOL_COMMAND_TAG_APPL_RESERVED_2              = 0xC7, /* Reserved for future use */
  MCI_PROTOCOL_COMMAND_TAG_APPL_RESERVED_3              = 0xC8, /* Reserved for future use */
  MCI_PROTOCOL_COMMAND_TAG_APPL_RESERVED_4              = 0xC9, /* Reserved for future use */
  MCI_PROTOCOL_COMMAND_TAG_APPL_ADC_SERVICE             = 0xCA, /* To configure ADC measurement */
  MCI_PROTOCOL_COMMAND_TAG_APPL_CMP_CONFIG              = 0xCB, /* To configure analog input(s) comparison */
  MCI_PROTOCOL_COMMAND_TAG_APPL_TPM_CONFIG              = 0xCC, /* To configure Timer/PWM module */
  MCI_PROTOCOL_COMMAND_TAG_APPL_SPI_CONFIG              = 0xCD, /* Reserved for future use */
};

/************
*
* Name:     mci_protocol_reset_source_e - MCU reset source
*
* Purpose:  Enumerate the source which caused MCU reset
*
* Members:  See below
*
* Note:     MCU reset sources are from MCU Reset Status Registers (SRS0 & SRS1)
*           EMBEDDED_WATCHDOG is a special reset implemented with a hardware timer
*           on MCU.
*
************/
enum mci_protocol_reset_source_e
{
  /* MCU RESETS */
  MCI_PROTOCOL_RESET_SRC_STOP_MODE_ACK      = 0x002000,  /* Stop mode acknowledge failure */
  MCI_PROTOCOL_RESET_SRC_MDM_AP             = 0x000800,  /* host debugger reset request */
  MCI_PROTOCOL_RESET_SRC_SOFTWARE_RESET     = 0x000400,  /* internal software trigger */
  MCI_PROTOCOL_RESET_SRC_CORE_LOCKUP        = 0x000200,  /* ARM core LOCKUP event */
  MCI_PROTOCOL_RESET_SRC_POWER_ON_RESET     = 0x000080,  /* power on */
  MCI_PROTOCOL_RESET_SRC_EXT_PIN_RESET      = 0x000040,  /* external reset pin */
  MCI_PROTOCOL_RESET_SRC_WATCH_DOG_RESET    = 0x000020,  /* watch dog reset */
  MCI_PROTOCOL_RESET_SRC_LOW_VOLTAGE_DETECT = 0x000002,  /* Low voltage detected */
  MCI_PROTOCOL_RESET_SRC_WAKEUP_FROM_STOP   = 0x000001,  /* wakeup from low power mode */

  /* MDM RESETS */
  MCI_PROTOCOL_RESET_SRC_EMBEDDED_WATCHDOG  = 0x010000,  /* Embedded watchdog reset */
};

/************
*
* Name:     mci_protocol_event_service_optype_e - Operation type for event services
*
* Purpose:  Enumerate the event service operation type
*
* Members:  See below
*
* Note:
*
************/
enum mci_protocol_event_service_optype_e
{
  MCI_PROTOCOL_EVENT_SERVICE_OPTYPE_EVENT_NONE  = 0x00,  /* Place holder */
  MCI_PROTOCOL_EVENT_SERVICE_OPTYPE_EVENT_QUERY = 0x01,  /* To query events due to MICRO_IRQ */
  MCI_PROTOCOL_EVENT_SERVICE_OPTYPE_EVENT_LOGS  = 0x02   /* To retrieve logs from MCU (TODO)*/
};

/************
*
* Name:     mci_protocol_event_type_e - type of an event
*
* Purpose:  Enumerate the event type
*
* Members:  See below
*
* Note:
*
************/
enum mci_protocol_event_type_e
{
  MCI_PROTOCOL_EVENT_TYPE_NONE      = 0x00,
  MCI_PROTOCOL_EVENT_TYPE_GPIO      = 0x01,
  MCI_PROTOCOL_EVENT_TYPE_ADC       = 0x02,
  MCI_PROTOCOL_EVENT_TYPE_RESET     = 0x03,
  MCI_PROTOCOL_EVENT_TYPE_MCI_RESET = 0x04,
  MCI_PROTOCOL_EVENT_TYPE_WUSRC     = 0x05,
  MCI_PROTOCOL_EVENT_TYPE_WATCHDOG  = 0x06,
  MCI_PROTOCOL_EVENT_TYPE_CALIBRATE = 0x07,
};

#define MCI_PROTOCOL_EVENT_SERVICE_OPTYPE_MASK    0x000000FF
#define MCI_PROTOCOL_EVENT_SERVICE_OPTYPE_SHIFT   0

/* Each event is encoded in one 32-bit parameter with TYPE in the highest byte:
*  TYPE:       [24~31] enumerated type of event (mci_protocol_event_type_e)
*
*  Other fields are TYPE-dependent:
*  GPIO:       [16~23] 8-bit port number
*              [8~15]  8-bit pin number
*              [1~7]   RESERVED
*              [0]     logic level value
*  ADC:        [16~23] input chaneel identificaiton
*              [0~15]  ADC value
*  RESET       [0~23]  enumerated reset source type
*  MCI_RESET   [16~23] 8-bit read index into TX buffer
*              [8~15]  8-bit write index into TX buffer
*              [0~7]   TX state
*              [0]     logic level value
*  WUSRC       [16~23] 8-bit wakeup source type
*              [0~15]  16-bit wakeup source info depends on WUSRC type
*  WATCHDOG    [0~23]  24-bit max time within which MCU watchdog must be renewed.
*  CALIBRATE   [0~23]  24-bit remaining time if the timer is stopped before expire.
*
*/
#define  MCI_PROTOCOL_EVENT_TYPE_MASK              0xFF000000
#define  MCI_PROTOCOL_EVENT_TYPE_SHIFT             24

/* GPIO interrupt: 16 bits of pin ID and 8 bits of value
* (16 bits of ID is composed of 8-bit port id and 1-bit pin ID)
*/
#define  MCI_PROTOCOL_EVENT_GPIO_NAME_MASK         0x00FFFF00
#define  MCI_PROTOCOL_EVENT_GPIO_NAME_SHIFT        8

#define  MCI_PROTOCOL_EVENT_GPIO_PORT_MASK         0x00FF0000
#define  MCI_PROTOCOL_EVENT_GPIO_PORT_SHIFT        16

#define  MCI_PROTOCOL_EVENT_GPIO_PIN_MASK          0x0000FF00
#define  MCI_PROTOCOL_EVENT_GPIO_PIN_SHIFT         8

#define  MCI_PROTOCOL_EVENT_GPIO_UNUSED_MASK       0x000000FE
#define  MCI_PROTOCOL_EVENT_GPIO_UNUSED_SHIFT      1

#define  MCI_PROTOCOL_EVENT_GPIO_VALUE_MASK        0x00000001
#define  MCI_PROTOCOL_EVENT_GPIO_VALUE_SHIFT       0

/* ADC interrupt: 8 bits ID and 16 bits value */
#define  MCI_PROTOCOL_EVENT_ADCH_ID_MASK           0x00FF0000
#define  MCI_PROTOCOL_EVENT_ADCH_ID_SHIFT          16

#define  MCI_PROTOCOL_EVENT_ADC_VALUE_MASK         0x0000FFFF
#define  MCI_PROTOCOL_EVENT_ADC_VALUE_SHIFT        0

/* Reset source: 24 bits value */
#define  MCI_PROTOCOL_EVENT_RESET_SOURCE_MASK      0x00FFFFFF
#define  MCI_PROTOCOL_EVENT_RESET_SOURCE_SHIFT     0

/* Wakeup from mode: 8-bit value */
#define  MCI_PROTOCOL_EVENT_WAKEUP_FROM_MODE_MASK  0x000000FF
#define  MCI_PROTOCOL_EVENT_WAKEUP_FROM_MODE_SHIFT 0

/* wakeup source */
#define  MCI_PROTOCOL_EVENT_WUSRC_TYPE_MASK        0x00FF0000
#define  MCI_PROTOCOL_EVENT_WUSRC_TYPE_SHIFT       16

#define  MCI_PROTOCOL_EVENT_WUSRC_VALUE_MASK       0x0000FFFF
#define  MCI_PROTOCOL_EVENT_WUSRC_VALUE_SHIFT      0

#define  MCI_PROTOCOL_EVENT_WUSRC_PORT_MASK        0x0000FF00
#define  MCI_PROTOCOL_EVENT_WUSRC_PORT_SHIFT       8

#define  MCI_PROTOCOL_EVENT_WUSRC_PIN_MASK         0x000000FF
#define  MCI_PROTOCOL_EVENT_WUSRC_PIN_SHIFT        0

#define GET_WUSRC_PORT(x) ((x & MCI_PROTOCOL_EVENT_WUSRC_PORT_MASK) >> MCI_PROTOCOL_EVENT_WUSRC_PORT_SHIFT)
#define GET_WUSRC_PIN(x)  (x & MCI_PROTOCOL_EVENT_WUSRC_PIN_MASK)
#define GET_WUSRC_VALUE(port, pin) (((port << MCI_PROTOCOL_EVENT_WUSRC_PORT_SHIFT) & MCI_PROTOCOL_EVENT_WUSRC_PORT_MASK) + (pin & MCI_PROTOCOL_EVENT_WUSRC_PIN_MASK))

/* Time elapsed: 24 bits value */
#define  MCI_PROTOCOL_EVENT_CALIBRATE_TIME_MASK    0x00FFFFFF
#define  MCI_PROTOCOL_EVENT_CALIBRATE_TIME_SHIFT   0

/************
*
* Name:     mci_protocol_pin_service_optype_e - Operation type for pin services
*
* Purpose:  Enumerate the gpio pin service operation type
*
* Members:  See below
*
* Note:
*
************/
enum mci_protocol_pin_service_optype_e
{
  MCI_PROTOCOL_PIN_SERVICE_OPTYPE_ATTR_GET    = 0x00,  /* get value of a single attribute */
  MCI_PROTOCOL_PIN_SERVICE_OPTYPE_ATTR_SET    = 0x01,  /* set value of a single attribute */
  MCI_PROTOCOL_PIN_SERVICE_OPTYPE_STATES_GET  = 0x02,  /* get all states of a pin */
  MCI_PROTOCOL_PIN_SERVICE_OPTYPE_CONFIG_SET  = 0x03   /* configuration of a pin */
};

/* number of arguments and results (32-bit unsigned integer) per operation type */
#define MCI_PROTOCOL_OPERATION_PARAMS_COUNT_MIN         1

#define MCI_PROTOCOL_PIN_ATTR_GET_ARG_COUNT             1
#define MCI_PROTOCOL_PIN_ATTR_GET_RESULT_COUNT          1

#define MCI_PROTOCOL_PIN_ATTR_SET_ARG_COUNT             1
#define MCI_PROTOCOL_PIN_ATTR_SET_RESULT_COUNT          1

#define MCI_PROTOCOL_PIN_STATES_GET_ARG_COUNT           1
#define MCI_PROTOCOL_PIN_STATES_GET_RESULT_COUNT        2

#define MCI_PROTOCOL_PIN_CONFIG_SET_ARG_COUNT           2
#define MCI_PROTOCOL_PIN_CONFIG_SET_RESULT_COUNT        0

/* Bit fields of the first parameter for pin service
*/
#define MCI_PROTOCOL_PIN_SERVICE_PORT_NUM_MASK          0xFF000000
#define MCI_PROTOCOL_PIN_SERVICE_PORT_NUM_SHIFT         24
#define MCI_PROTOCOL_PIN_SERVICE_PIN_NUM_MASK           0x00FF0000
#define MCI_PROTOCOL_PIN_SERVICE_PIN_NUM_SHIFT          16
#define MCI_PROTOCOL_PIN_SERVICE_OPTYPE_MASK            0x0000FF00
#define MCI_PROTOCOL_PIN_SERVICE_OPTYPE_SHIFT           8

/* low byte encode GPIO direction and logic levels if muxed to GPIO funciton */
#define MCI_PROTOCOL_PIN_GPIO_DIRECTION_MASK            0x01
#define MCI_PROTOCOL_PIN_GPIO_DIRECTION_SHIFT           0
#define MCI_PROTOCOL_PIN_GPIO_LEVEL_MASK                0x02
#define MCI_PROTOCOL_PIN_GPIO_LEVEL_SHIFT               1

/* Bit fields of the second parameter for the pin service on KL03
*  The fields are aligned with the definition of the PCR register
*/
#define MCI_PROTOCOL_PIN_CONFIG_PULL_SELECT_SHIFT       0
#define MCI_PROTOCOL_PIN_CONFIG_PULL_SELECT_MASK        0x00000001
#define MCI_PROTOCOL_PIN_CONFIG_PULL_SELECT_SIZE        1

#define MCI_PROTOCOL_PIN_CONFIG_PULL_ENABLE_SHIFT       1
#define MCI_PROTOCOL_PIN_CONFIG_PULL_ENABLE_MASK        0x00000002
#define MCI_PROTOCOL_PIN_CONFIG_PULL_ENABLE_SIZE        1

#define MCI_PROTOCOL_PIN_CONFIG_SLEW_RATE_SHIFT         2
#define MCI_PROTOCOL_PIN_CONFIG_SLEW_RATE_MASK          0x00000004
#define MCI_PROTOCOL_PIN_CONFIG_SLEW_RATE_SIZE          1

#define MCI_PROTOCOL_PIN_CONFIG_PASSIVE_FILTER_SHIFT    4
#define MCI_PROTOCOL_PIN_CONFIG_PASSIVE_FILTER_MASK     0x00000010
#define MCI_PROTOCOL_PIN_CONFIG_PASSIVE_FILTER_SIZE     1

#define MCI_PROTOCOL_PIN_CONFIG_DRIVE_STRENGTH_SHIFT    6
#define MCI_PROTOCOL_PIN_CONFIG_DRIVE_STRENGTH_MASK     0x00000040
#define MCI_PROTOCOL_PIN_CONFIG_DRIVE_STRENGTH_SIZE     1

#define MCI_PROTOCOL_PIN_CONFIG_FUNCTION_MUX_SHIFT      8
#define MCI_PROTOCOL_PIN_CONFIG_FUNCTION_MUX_MASK       0x00000700
#define MCI_PROTOCOL_PIN_CONFIG_FUNCTION_MUX_SIZE       3

#define MCI_PROTOCOL_PIN_CONFIG_INTERRUPT_SHIFT         16
#define MCI_PROTOCOL_PIN_CONFIG_INTERRUPT_MASK          0x000F0000
#define MCI_PROTOCOL_PIN_CONFIG_INTERRUPT_SIZE          4

#define MCI_PROTOCOL_PIN_CONFIG_INTERRUPT_STATUS_SHIFT  24
#define MCI_PROTOCOL_PIN_CONFIG_INTERRUPT_STATUS_MASK   0x01000000
#define MCI_PROTOCOL_PIN_CONFIG_INTERRUPT_STATUS_SIZE   1

/************
*
* Name:     mci_protocol_adc_service_optype_e - ADC service operation types
*
* Purpose:  To enumerate the ADC service operation type
*
* Members:  See below
*
* Note:
*
************/
enum mci_protocol_adc_service_optype_e
{
  MCI_PROTOCOL_ADC_SERVICE_OPTYPE_INVALID  = 0x00, /* Invalid opeartion type */
  MCI_PROTOCOL_ADC_SERVICE_OPTYPE_INIT     = 0x01, /* Initialize the ADC module */
  MCI_PROTOCOL_ADC_SERVICE_OPTYPE_START    = 0x02, /* Start ADC for an input channel */
  MCI_PROTOCOL_ADC_SERVICE_OPTYPE_READ     = 0x03, /* Read value for previously started ADC */
  MCI_PROTOCOL_ADC_SERVICE_OPTYPE_DEINIT   = 0x04, /* De-initalize ADC module */
  MCI_PROTOCOL_ADC_SERVICE_OPTYPE_STOP     = 0x05, /* Stop a previously started ADC */
};

/************
*
* Name:     mci_protocol_adc_channel_e - ADC analog input channel
*
* Purpose:  Enumerate the ADC analog input channel
*
* Members:  See below
*
* Note:
*
************/
enum mci_protocol_adc_channel_e
{
  MCI_PROTOCOL_ADC0_SE0      = 0x00,
  MCI_PROTOCOL_ADC0_SE8      = 0x08,
  MCI_PROTOCOL_ADC0_SE9      = 0x09,
  MCI_PROTOCOL_ADC0_SE15     = 0x0F,
  MCI_PROTOCOL_ADC0_DISABLED = 0x1F,
};

/************
*
* Name:     mci_protocol_adc_low_power_conv_e
*
* Purpose:  Enumerate the power configuration of successive approximation converter
*
* Members:  See below
*
* Note:
*
************/
enum mci_protocol_adc_low_power_conv_e
{
  MCI_PROTOCOL_ADC_LOW_POWER_CONV_DISABLE = 0U,
  MCI_PROTOCOL_ADC_LOW_POWER_CONV_ENABLE  = 1U
};

/************
*
* Name:     mci_protocol_adc_sample_period_config_e
*
* Purpose:  Enumerate the sample period adjustment
*
* Members:  See below
*
* Note:     Longer sample period allows more accurate conversion higher impedance inputs;
*           Shorter sample period maximizes conversion speed for lower impedance inputs.
*
************/
enum mci_protocol_adc_sample_period_adj_e
{
  MCI_PROTOCOL_ADC_SAMPLE_PERIOD_ADJ_NONE = 0,  /* no adjustment */
  MCI_PROTOCOL_ADC_SAMPLE_PERIOD_ADJ_4    = 1,  /* longest sample period */
  MCI_PROTOCOL_ADC_SAMPLE_PERIOD_ADJ_3    = 2,  /* longer sample period */
  MCI_PROTOCOL_ADC_SAMPLE_PERIOD_ADJ_2    = 3,  /* shorter sample period */
  MCI_PROTOCOL_ADC_SAMPLE_PERIOD_ADJ_1    = 4,  /* shortest sample period */
};

/************
*
* Name:     mci_protocol_adc_resolution_e - digital resolution of ADC
*
* Purpose:  Enumerate the digital resolution of ADC
*
* Members:  See below
*
* Note:
*
************/
enum mci_protocol_adc_resolution_mode_e
{
  MCI_PROTOCOL_ADC_RESOLUTION_8_BITS  = 0,  /* 8-bit  for single end sample  */
  MCI_PROTOCOL_ADC_RESOLUTION_12_BITS = 1,  /* 12-bit for single end sample */
  MCI_PROTOCOL_ADC_RESOLUTION_10_BITS = 2,  /* 10-bit for single end sample */
};

/************
*
* Name:     mci_protocol_adc_high_speed_conversion_e
*
* Purpose:  To enable/disable very high speed conversion
*
* Members:  See below
*
* Note:
*
************/
enum mci_protocol_adc_high_speed_conv_e
{
  MCI_PROTOCOL_ADC_HIGH_SPEED_CONV_DISABLE = 0,
  MCI_PROTOCOL_ADC_HIGH_SPEED_CONV_ENABLE  = 1,
};

/************
*
* Name:     mci_protocol_adc_high_speed_conversion_e
*
* Purpose:  To enable/disable very high speed conversion
*
* Members:  See below
*
* Note:
*
************/
enum mci_protocol_adc_trigger_mode_e
{
  MCI_PROTOCOL_ADC_TRIGGER_MODE_SW = 0,
  MCI_PROTOCOL_ADC_TRIGGER_MODE_HW = 1,
};

/************
*
* Name:     mci_protocol_adc_sample_mode_e - ADC sample modes
*
* Purpose:  Enumerate ADC sampling and averaging modes
*
* Members:  See below
*
* Note:
*
************/
enum mci_protocol_adc_sample_mode_e
{
  MCI_PROTOCOL_ADC_SAMPLE_MODE_SOFTWARE   = 0,
  MCI_PROTOCOL_ADC_SAMPLE_MODE_HARDWARE   = 1,
};

/************
*
* Name:     mci_protocol_adc_hw_average_mode_e - ADC HW average modes
*
* Purpose:  Enumerate the HW average modes (number of samples per operation)
*
* Members:  See below
*
* Note:
*
************/
enum mci_protocol_adc_hw_average_count_e
{
  MCI_PROTOCOL_ADC_HW_AVERAGE_DISABLED = 0x0,  /* hardware feature is disabled */
  MCI_PROTOCOL_ADC_HW_AVERAGE_COUNT_4  = 0x4,  /* hardware average with 4 samples. */
  MCI_PROTOCOL_ADC_HW_AVERAGE_COUNT_8  = 0x5,  /* hardware average with 8 samples. */
  MCI_PROTOCOL_ADC_HW_AVERAGE_COUNT_16 = 0x6,  /* hardware average with 16 samples. */
  MCI_PROTOCOL_ADC_HW_AVERAGE_COUNT_32 = 0x7,  /* hardware average with 32 samples. */
};

/************
*
* Name:     mci_protocol_adc_compare_mode_e - ADC comparison method
*
* Purpose:  Enumerate the comparison method of ADC result with specified value(s)
*
* Members:  See below
*
* Note:     Target value are exclusive in all comparisons.
*
************/
enum mci_protocol_adc_compare_mode_e
{
  MCI_PROTOCOL_ADC_COMPARE_MODE_DISABLED = 0x00, /* no trigger */
  MCI_PROTOCOL_ADC_COMPARE_MODE_ABOVE    = 0x01, /* trigger if excede the specified */
  MCI_PROTOCOL_ADC_COMPARE_MODE_BELOW    = 0x02, /* trigger if drop below the specified */
  MCI_PROTOCOL_ADC_COMPARE_MODE_WITHIN   = 0x03, /* trigger if falls into the range */
  MCI_PROTOCOL_ADC_COMPARE_MODE_BEYOND   = 0x04, /* trigger if goes beyond the range */
};

/************
*
* Name:     mci_protocol_adc_trigger_mode_e
*
* Purpose:  To select trigger mode for ADC operation
*
* Members:  See below
*
* Note:     only SOFTWARE trigger is currently supported
*
************/
enum mci_protocol_adc_trigger_e
{
  MCI_PROTOCOL_ADC_TRIGGER_SOFTWARE      = 0xFF,  /* ADC started by SW command */
  MCI_PROTOCOL_ADC_TRIGGER_LPTMR0        = 0x8E,  /* LPTMR timeout event */
};

/* Number of parameters and results per ADC operation type */
#define MCI_PROTOCOL_ADC_INIT_ARGS_COUNT_MIN           2
#define MCI_PROTOCOL_ADC_INIT_RESULTS_COUNT            0

#define MCI_PROTOCOL_ADC_START_ARGS_COUNT              1
#define MCI_PROTOCOL_ADC_START_RESULTS_COUNT           0

/* Number of parameters and results for READ ADC operation */
#define MCI_PROTOCOL_ADC_READ_ARGS_COUNT               1
#define MCI_PROTOCOL_ADC_READ_RESULTS_COUNT            1

/* Number of parameters and results for DEINIT ADC operation */
#define MCI_PROTOCOL_ADC_DEINIT_ARGS_COUNT             1
#define MCI_PROTOCOL_ADC_DEINIT_RESULTS_COUNT          0

/* Bit fields of the first parameter: ADC module ID and operation type
*  (KL03 has a single ADC module: ID=0)
*/
#define MCI_PROTOCOL_ADC_SERVICE_OPTYPE_MASK           0x000000FF
#define MCI_PROTOCOL_ADC_SERVICE_OPTYPE_SHIFT          0

#define MCI_PROTOCOL_ADC_CHANNEL_MASK                  0x00001F00
#define MCI_PROTOCOL_ADC_CHANNEL_SHIFT                 8

#define MCI_PROTOCOL_ADC_SAMPLE_COUNT_MASK             0x00FF0000
#define MCI_PROTOCOL_ADC_SAMPLE_COUNT_SHIFT            16

#define MCI_PROTOCOL_ADC_TRIGGER_MASK                  0xFF000000
#define MCI_PROTOCOL_ADC_TRIGGER_SHIFT                 24

/* Bit fields of the second parameter for adc configuration operation
*/
#define MCI_PROTOCOL_ADC_CFG1_MASK                     0x000000FF
#define MCI_PROTOCOL_ADC_CFG1_SHIFT                    0

#define MCI_PROTOCOL_ADC_CFG2_MASK                     0x0000FF00
#define MCI_PROTOCOL_ADC_CFG2_SHIFT                    8

#define MCI_PROTOCOL_ADC_SC2_MASK                      0x00FF0000
#define MCI_PROTOCOL_ADC_SC2_SHIFT                     16

#define MCI_PROTOCOL_ADC_SC3_MASK                      0xFF000000
#define MCI_PROTOCOL_ADC_SC3_SHIFT                     24

/* LSB byte contains CFG1 content */
#define MCI_PROTOCOL_ADC_RESOLUTION_MODE_MASK          0x0000000C
#define MCI_PROTOCOL_ADC_RESOLUTION_MODE_SHIFT         2

#define MCI_PROTOCOL_ADC_SAMPLE_PERIOD_ENABLE_MASK     0x00000010
#define MCI_PROTOCOL_ADC_SAMPLE_PERIOD_ENABLE_SHIFT    4

#define MCI_PROTOCOL_ADC_LOW_POWER_CONFIG_MASK         0x00000080
#define MCI_PROTOCOL_ADC_LOW_POWER_CONFIG_SHIFT        7

/* Second LSB byte contains CFG2 content */
#define MCI_PROTOCOL_ADC_SAMPLE_PERIOD_MASK            0x00000300
#define MCI_PROTOCOL_ADC_SAMPLE_PERIOD_SHIFT           8

#define MCI_PROTOCOL_ADC_HIGH_SPEED_CONVERSION_MASK    0x00000400
#define MCI_PROTOCOL_ADC_HIGH_SPEED_CONVERSION_SHIFT   10

/* third byte contains SC2 register content */
#define MCI_PROTOCOL_ADC_TRIGGER_MODE_MASK             0x00400000
#define MCI_PROTOCOL_ADC_TRIGGER_MODE_SHIFT            22

#define MCI_PROTOCOL_ADC_HW_COMPARE_ENABLE_MASK        0x00200000
#define MCI_PROTOCOL_ADC_HW_COMPARE_ENABLE_SHIFT       21

#define MCI_PROTOCOL_ADC_HW_COMPARE_GTEQ_MASK          0x00100000
#define MCI_PROTOCOL_ADC_HW_COMPARE_GTEQ_SHIFT         20

#define MCI_PROTOCOL_ADC_HW_COMPARE_RANGE_MASK         0x00080000
#define MCI_PROTOCOL_ADC_HW_COMPARE_RANGE_SHIFT        19

/* MSB byte contains SC3 register content: HW average */
#define MCI_PROTOCOL_ADC_HW_AVERAGE_SELECT_MASK        0x07000000
#define MCI_PROTOCOL_ADC_HW_AVERAGE_SELECT_SHIFT       24

/* Bit fields of optional third parameter: ADC thresholds for HW comparison
 * [31-24]  8 bits compare mode
 * [23-12]  12 bits 2nd ADC value (low limit of the range comparison; ignored in simple comparison)
 * [11- 0]  12 bits 1st ADC value (high limit of the range comparison; or threshold in simple comparison)
 */
#define MCI_PROTOCOL_ADC_COMPARE_MODE_MASK             0xFF000000
#define MCI_PROTOCOL_ADC_COMPARE_MODE_SHIFT            24

#define MCI_PROTOCOL_ADC_COMPARE_V2_MASK               0x00FFF000
#define MCI_PROTOCOL_ADC_COMPARE_V2_SHIFT              12

#define MCI_PROTOCOL_ADC_COMPARE_V1_MASK               0x00000FFF
#define MCI_PROTOCOL_ADC_COMPARE_V1_SHIFT              0

/* ADC start */
#define MCI_PROTOCOL_ADC_SAMPLE_COUNT_MASK             0x00FF0000
#define MCI_PROTOCOL_ADC_SAMPLE_COUNT_SHIFT            16

#define MCI_PROTOCOL_ADC_HW_SAMPLE_MODE_MASK           0x01000000
#define MCI_PROTOCOL_ADC_HW_SAMPLE_MODE_SHIFT          24

/* read */
#define MCI_PROTOCOL_ADC_VALUE_MASK                    0xFFFF0000
#define MCI_PROTOCOL_ADC_VALUE_SHIFT                   16

/************
*
* Name:     mci_protocol_wakeup_source_optype_e - Operation type for wakeup source settings
*
* Purpose:  Enumerate the wakeup source operation type
*
* Members:  See below
*
* Note:
*
************/
enum mci_protocol_wakeup_source_optype_e
{
  MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_INVALID = 0x00,
  MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_SET     = 0x01,
  MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_GET     = 0x02,
  MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_CLEAR   = 0x03,
};

/************
*
* Name:     mci_protocol_wakeup_source_type_e - wakeup source type for power saving mode
*
* Purpose:  Enumerate the wakeup source used to wake the device up from power saving mode
*
* Members:  See below
*
* Note:
*
************/
enum mci_protocol_wakeup_source_type_e
{
  MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_NONE      = 0x0000,
  MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_EXT_PINS  = 0x0001,  /* External pins interrupt */
  MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_TIMER     = 0x0002,  /* RTC alarm timeout event */
  MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_ADC       = 0x0004,  /* ADC result from an analog input pin */
};

/* Max 16 pins for each port */
#define MCI_PROTCOL_PINS_PER_PORT                         16

/* bitmask representation of the external wakeup pins */
#define MCI_PROTCOL_WAKEUP_SOURCE_EXT_PIN_BITMASK_NONE    0x0
#define MCI_PROTCOL_WAKEUP_SOURCE_EXT_PIN_BITMASK_PTA0    0x00000001
#define MCI_PROTCOL_WAKEUP_SOURCE_EXT_PIN_BITMASK_PTB0    0x00010000
#define MCI_PROTCOL_WAKEUP_SOURCE_EXT_PIN_BITMASK_ALL     \
                             (MCI_PROTCOL_WAKEUP_SOURCE_EXT_PIN_BITMASK_PTA0 | \
                              MCI_PROTCOL_WAKEUP_SOURCE_EXT_PIN_BITMASK_PTB0)

#define MCI_PROTOCOL_WAKEUP_SOURCE_ADC_PIN_BITMASK_NONE   0x00
#define MCI_PROTOCOL_WAKEUP_SOURCE_ADC_PIN_BITMASK_PTA12  0x00001000
#define MCI_PROTOCOL_WAKEUP_SOURCE_ADC_PIN_BITMASK_PTB1   0x00020000


/* Number of parameters and results per PM operation type */
#define MCI_PROTOCOL_WAKEUP_SOURCE_SET_PARAMS_COUNT      2
#define MCI_PROTOCOL_WAKEUP_SOURCE_SET_RESULT_COUNT      0

#define MCI_PROTOCOL_WAKEUP_SOURCE_CLEAR_PARAMS_COUNT    2
#define MCI_PROTOCOL_WAKEUP_SOURCE_CLEAR_RESULT_COUNT    0

#define MCI_PROTOCOL_WAKEUP_SOURCE_GET_PARAMS_COUNT      1
#define MCI_PROTOCOL_WAKEUP_SOURCE_GET_RESULT_COUNT      2

/* Bit fields of the first parameter: wakeup source type and operation type
*  OPTYPE:   bits [ 0, 7] - 8 bits of operation type
*  TYPE:     bits [ 8,15] - 8 bits of wakeup source type
*  ADCH:     bits [16,23] - 8 bits of ADC input channel identifier (ADC type only).
*/
#define MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_MASK           0x000000FF
#define MCI_PROTOCOL_WAKEUP_SOURCE_OPTYPE_SHIFT          0

#define MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_MASK             0x0000FF00
#define MCI_PROTOCOL_WAKEUP_SOURCE_TYPE_SHIFT            8

#define MCI_PROTOCOL_WAKEUP_SOURCE_ADCH_MASK             0x00FF0000
#define MCI_PROTOCOL_WAKEUP_SOURCE_ADCH_SHIFT            16

/* the second paramter encode the args for the wakeup source type
*  EXT_PIN: Bit mask representation (32 bits) of the external pins
*  TIMER:   32-bit unsigned integer (ms)
*  ADC:       thresholds of ADC range to trigger wakeup event
*             bits [ 0, 7] - compare method
*             bits [ 8,19] - HIGH threshold
*             bits [20,31] - LOW threshold
*/

/************
*
* Name:     mci_protocol_power_mode_e
*
* Purpose:  To enumerate MCU power mode
*
* Member:   See below.
*
* Notes:
*
************/
enum mci_protocol_power_mode_e
{
  MCI_PROTOCOL_POWER_MODE_RUN    = 0,          /* Normal Run mode*/
  MCI_PROTOCOL_POWER_MODE_WAIT   = 1,          /* Core clock is gated off */

  MCI_PROTOCOL_POWER_MODE_VLPR   = 2,          /* Restricted clock: core and bus clocks 2~4MHz, Flash 0.8~1MHz */
  MCI_PROTOCOL_POWER_MODE_VLPW   = 3,          /* Restricted clock: core clock off, other clocks may runs if enabled */
  MCI_PROTOCOL_POWER_MODE_VLPS   = 4,          /* Restricted clock: core clock off; system and bus clock are gated off */

  MCI_PROTOCOL_POWER_MODE_STOP   = 5,          /* Core clock gated off; system and bus clock gated off */
  MCI_PROTOCOL_POWER_MODE_STOP_1 = 6,          /* Partial Stop with both system and bus clocks disabled */
  MCI_PROTOCOL_POWER_MODE_STOP_2 = 7,          /* Partial Stop with system clock disabled and bus clock enabled */

  MCI_PROTOCOL_POWER_MODE_VLLS3  = 8,          /* 1. core/system and bus/flash clocks are gated off
                                                * 2. The MCU is placed in a low leakage mode .
                                                * 3. I/O states are held. */
  MCI_PROTOCOL_POWER_MODE_VLLS1  = 9,          /* 4. Further powering down all system RAM */
  MCI_PROTOCOL_POWER_MODE_VLLS0  = 10,         /* 5. Further disable the 1kHz LPO clock */
  MCI_PROTOCOL_POWER_MODE_VLLS0_POR_OFF = 11,  /* 6. Disable the power on reset (POR) circuit */
  MCI_PROTOCOL_POWER_MODE_MAX
};

/************
*
* Name:     mci_protocol_mdm_state_e
*
* Purpose:  To enumerate MDM state
*
* Member:   See below.
*
* Notes:    MDM is up running only the power supply is switched on and
*           PMIC is powered on.
*
************/
enum mci_protocol_mdm_state_e
{
  MCI_PROTOCOL_MDM_STATE_OFF = 0x0,
  MCI_PROTOCOL_MDM_STATE_ON  = 0x1,
};

/************
*
* Name:     mci_protocol_pm_optype_e
*
* Purpose:  To enumerate power management operation type
*
* Member:   See below.
*
* Notes:    none
*
************/
enum mci_protocol_pm_optype_e
{
  MCI_PROTOCOL_PM_OPTYPE_INVALID               = 0x0,  /* placeholder */
  MCI_PROTOCOL_PM_OPTYPE_SET                   = 0x1,  /* to config the power management profile */
  MCI_PROTOCOL_PM_OPTYPE_GET                   = 0x2,  /* to get the configed power management profile */
  MCI_PROTOCOL_PM_OPTYPE_POWER_OFF_TIME_CONFIG = 0x3,  /* to config the wait times for power off sequence */
  MCI_PROTOCOL_PM_OPTYPE_POWER_OFF_SYNC        = 0x4,  /* to synchronize the completion of power off sequence */
  MCI_PROTOCOL_PM_OPTYPE_PSM_SYNC_CONFIG       = 0x5,  /* to configure the PSM/ULPM synchronization option */
};

#define MCI_PROTOCOL_PM_SET_PARAMS_COUNT              3
#define MCI_PROTOCOL_PM_SET_OK_RESULTS_COUNT          0
#define MCI_PROTOCOL_PM_SET_ERR_RESULTS_COUNT         1

#define MCI_PROTOCOL_PM_GET_PARAMS_COUNT              1
#define MCI_PROTOCOL_PM_GET_RESULTS_COUNT             3

/* Bit fields of the first parameter: wakeup source type and operation type
*  PM_OPTYPE:      bits [ 0, 7] - operation type
*  ACTIVE_MODE:    bits [ 8,15] - MCU power mode of the next active state
*  WAIT_TIME:      bits [16]    - time (ms) before MCU's transition from active
*                                 state to standby state after the last external
*                                 event is processed.
*/
#define MCI_PROTOCOL_PM_OPTYPE_MASK                   0x000000FF
#define MCI_PROTOCOL_PM_OPTYPE_SHIFT                  0

#define MCI_PROTOCOL_PM_ACTIVE_MCU_MODE_MASK          0x0000FF00
#define MCI_PROTOCOL_PM_ACTIVE_MCU_MODE_SHIFT         8

#define MCI_PROTOCOL_PM_ACTIVE_IDLE_TIME_MASK         0xFFFF0000
#define MCI_PROTOCOL_PM_ACTIVE_IDLE_TIME_SHIFT        16

/* the second paramter encode the STANDBY state configuration
*  STANDBY_MODE: bits [ 0, 7] - MCU power mode in STANDBY state
*  MDM_STATE:    bits [ 8,15] - MDM state in STANDBY state
*  WAKEUP_SOURCE bits [16,31] - wakeup source to be activated in STANDBY state
*/
#define MCI_PROTOCOL_PM_STANDBY_MCU_MODE_MASK         0x000000FF
#define MCI_PROTOCOL_PM_STANDBY_MCU_MODE_SHIFT        0

#define MCI_PROTOCOL_PM_STANDBY_MDM_STATE_MASK        0x0000FF00
#define MCI_PROTOCOL_PM_STANDBY_MDM_STATE_SHIFT       8

#define MCI_PROTOCOL_PM_STANDBY_WAKEUP_SOURCES_MASK    0xFFFF0000
#define MCI_PROTOCOL_PM_STANDBY_WAKEUP_SOURCES_SHIFT   16

/* the third paramter encodes the conditions to power on MDM */
#define MCI_PROTOCOL_MDM_ON_CONDITIONS_ANY_MASK       0x0000FFFF
#define MCI_PROTOCOL_MDM_ON_CONDITIONS_ANY_SHIFT      0

#define MCI_PROTOCOL_MDM_ON_CONDITIONS_ALL_MASK       0xFFFF0000
#define MCI_PROTOCOL_MDM_ON_CONDITIONS_ALL_SHIFT      16

/* For the MCI_PROTOCOL_PM_OPTYPE_POWER_OFF_TIME_CONFIG request, in addition to the OPTYPE,
*  the second parameter encodes the max wait time for possible arrival of the SYNC message.
*  the third parameter encodes the  wait time before switching off the MDM power supply.
*/
#define MCI_PROTOCOL_PM_POWER_OFF_TIME_CONFIG_PARAMS_COUNT  3
#define MCI_PROTOCOL_PM_POWER_OFF_TIME_CONFIG_RESULT_COUNT  0

/* No additional parameters for the MCI_PROTOCOL_PM_OPTYPE_POWER_OFF_SYNC indication */
#define MCI_PROTOCOL_PM_POWER_OFF_SYNC_PARAMS_COUNT         1
#define MCI_PROTOCOL_PM_POWER_OFF_SYNC_RESULT_COUNT         0

/* For the MCI_PROTOCOL_PM_OPTYPE_PSM_SYNC_CONFIG request
*  Bit fields of the first parameter: wakeup source type and operation type
*  PM_OPTYPE:      bits [ 0, 7] - operation type
*  SYNC_OPTTION:   bits [ 8,15] - PSM synchronization option
*  the second parameter encodes the optional timeout value for option 2 and 3
*  (ignored for synchronization option 1).
*/
#define MCI_PROTOCOL_PM_PSM_SYNC_CONFIG_PARAMS_COUNT        3
#define MCI_PROTOCOL_PM_PSM_SYNC_CONFIG_RESULT_COUNT        0

#define MCI_PROTOCOL_PM_PSM_SYNC_CONFIG_OPTION_MASK        0x0000FF00
#define MCI_PROTOCOL_PM_PSM_SYNC_CONFIG_OPTION_SHIFT        8

/************
*
* Name:     mci_protocol_pm_psm_sync_option_e
*
* Purpose:  To enumerate PSM-ULPM synchronization option
*
* Member:   See below.
*
* Notes:
*
************/
enum mci_protocol_pm_psm_sync_option_e
{
  MCI_PROTOCOL_PM_PSM_SYNC_OPTION_NONE = 0, /* Placeholder */
  MCI_PROTOCOL_PM_PSM_SYNC_OPTION_A    = 1, /* I2C on to wait MDM power on w/ PMIC RTC */
  MCI_PROTOCOL_PM_PSM_SYNC_OPTION_B    = 2, /* RTC timer sync w/ PMIC RTC timer */
  MCI_PROTOCOL_PM_PSM_SYNC_OPTION_C    = 3, /* RTC timer for PSM; MMD power off */
  MCI_PROTOCOL_PM_PSM_SYNC_OPTION_MAX  = MCI_PROTOCOL_PM_PSM_SYNC_OPTION_C
};

/************
*
* Name:     mci_protocol_timer_optype_e - Operation type for timer service
*
* Purpose:  Enumerate the timer operation type
*
* Members:  See below
*
* Note:
*
************/
enum mci_protocol_timer_optype_e
{
  MCI_PROTOCOL_TIMER_OPTYPE_IDLE       = 0x00,  /* To set a timer to idle */
  MCI_PROTOCOL_TIMER_OPTYPE_WATCHDOG   = 0x01,  /* To start watchdog service */
  MCI_PROTOCOL_TIMER_OPTYPE_READ_INPUT = 0x02,  /* To start input read service */
  MCI_PROTOCOL_TIMER_OPTYPE_CALIBRATE  = 0x03,  /* To start a calibrate timer */
};

/************
*
* Name:     mci_protocol_timer_e - the timer used for timer service
*
* Purpose:  Enumerate the timer type
*
* Members:  See below
*
* Note:     Only RTC_ALARM timer (default) is currently available.
*
************/
enum mci_protocol_timer_e
{
  MCI_PROTOCOL_TIMER_RTC_ALARM = 0x00,
  MCI_PROTOCOL_TIMER_LPTMR     = 0x01,
};

/************
*
* Name:     mci_protocol_timer_read_input_type_e
*
* Purpose:  Enumerate types of the input to be read
*
* Members:  See below
*
* Note:     Bit flags to indicate the type(s) of inputs in the request
*
************/
enum mci_protocol_timer_read_input_type_e
{
  MCI_PROTOCOL_TIMER_READ_INPUT_NONE   = 0x00,
  MCI_PROTOCOL_TIMER_READ_INPUT_GPIO   = 0x01,   /* digital input */
  MCI_PROTOCOL_TIMER_READ_INPUT_ADC    = 0x02,   /* analog input  */
};

/* No additional parameters for MCI_PROTOCOL_TIMER_OPTYPE_IDLE besides the OPTYPE.
*  Results returned for successful MCI_PROTOCOL_TIMER_OPTYPE_IDLE command
*  The 1st parameter encodes the usage of the stopped timer.
*  The 2nd.parameter encodes the remainder of the timeout value when the timer is stopped.
*/
#define MCI_PROTOCOL_TIMER_IDLE_PARAMS_COUNT           1
#define MCI_PROTOCOL_TIMER_IDLE_RESULT_COUNT           2

/* Bit fields of the first parameter for all TIMER_SERVICE requests:
*  TIMER_OPTYPE:   bits [ 0, 7] - timer operation type
*  TIMER:          bits [ 8,15] - timer type
*/
#define MCI_PROTOCOL_TIMER_OPTYPE_MASK       0x000000FF
#define MCI_PROTOCOL_TIMER_OPTYPE_SHIFT      0

#define MCI_PROTOCOL_TIMER_MASK              0x0000FF00
#define MCI_PROTOCOL_TIMER_SHIFT             8

/* For MCI_PROTOCOL_TIMER_OPTYPE_WATCHDOG:
*  the 2nd paramter encodes the timeout value (ms) for watchdog warning event.
*  the 3rd paramter encodes the time delay (ms) before MCU resetting the device
*                           after issuing a watchdog warning event.
*/
#define MCI_PROTOCOL_TIMER_WATCHDOG_PARAMS_COUNT       3

/* For MCI_PROTOCOL_TIMER_OPTYPE_READ_INPUT request:
*  the 2nd paramter encodes the inital timeout value (ms)
*  the 3rd paramter encodes the subsequent timeout value (ms)
*  the 4th paramter encodes the bitbask contain all digital input pins
*     (valid only if MCI_PROTOCOL_TIMER_READ_INPUT_GPIO is specificed)
*  the 5th paramter encodes the bitbask contain all analog input pins
*     (valid only if MCI_PROTOCOL_TIMER_READ_INPUT_ADC is specificed)
*/
#define MCI_PROTOCOL_TIMER_INPUT_READ_PARAMS_COUNT      5

/* For MCI_PROTOCOL_TIMER_OPTYPE_CALIBRATE request:
*  the 2nd paramter encodes the timeout value (ms)
*  No subsequent the 3rd paramter encodes the subsequent timeout value (ms)
*/
#define MCI_PROTOCOL_TIMER_CALIBRATE_PARAMS_COUNT       2

/* no parameter returned in a response to a TIMER_SERVICE request */
#define MCI_PROTOCOL_TIMER_SERVICE_RESULT_COUNT         0

#define MCI_PROTOCOL_APPL_OPTYPE_MASK                   0x000000FF
#define MCI_PROTOCOL_APPL_OPTYPE_SHIFT                  0

/************
 *
 * Name:     mci_protocol_status_code_e  - Protocol status error codes
 *
 * Purpose:  Define the MCI communication protocol status code
 *
 * Members:  See below
 *
 ************/
enum mci_protocol_status_code_e
{
  /* status code generated internally on MDM side */
  MCI_PROTOCOL_STATUS_CODE_NOT_ENOUGH_BUFFER           = -107,
  MCI_PROTOCOL_STATUS_CODE_RX_ERROR                    = -106,
  MCI_PROTOCOL_STATUS_CODE_TX_ERROR                    = -105,
  MCI_PROTOCOL_STATUS_CODE_ENCODE_ERROR                = -104,
  MCI_PROTOCOL_STATUS_CODE_WRONG_MODE                  = -103,
  MCI_PROTOCOL_STATUS_CODE_NOT_CONNECTED               = -102,

  /* status code from MCU */
  MCI_PROTOCOL_STATUS_CODE_NOT_READY                   = -3,
  MCI_PROTOCOL_STATUS_CODE_CODING_ERROR                = -2,
  MCI_PROTOCOL_STATUS_CODE_NOT_IMPLEMENTED             = -1,

  MCI_PROTOCOL_STATUS_CODE_SUCCESS                     = 0,
  MCI_PROTOCOL_STATUS_CODE_FAIL                        = 1,    /* general error */
  MCI_PROTOCOL_STATUS_CODE_READ_ONLY                   = 2,    /* the property is read-only */
  MCI_PROTOCOL_STATUS_CODE_OUT_OF_RANGE                = 3,    /* given parameters out of range */
  MCI_PROTOCOL_STATUS_CODE_INVALID_ARGUMENT            = 4,    /* invalid parameters for the command */
  MCI_PROTOCOL_STATUS_CODE_WUSRC_NOT_CONFIGURED        = 5,    /* wakeup source not configured for PM profile */

  MCI_PROTOCOL_STATUS_CODE_UNKNOWN_COMMAND             = 10000,
};

/*******************
 * Data structures *
 *******************/
/************
*
* Name:      mci_protocol_packet_s
*
* Purpose:   Define C-Structure for protocol packet data
*
* Members:   See below
*
* Notes:
*
************/
struct mci_protocol_packet_s
{
  enum mci_protocol_command_tag_e tag;    /* command tag */
  uint8_t                         flags;  /* flags to indicate whether there is data follow this command */
  void                           *datap;  /* generic pointer: uint32_t command parameter list or data bytes */
  uint8_t                         count;  /* number of parameters in CMD packet; number of bytes in DATA packet */
};

/************
*
* Name:     mci_protocol_frame_s
*
* Purpose:  Define C-structure for protocol frame data
*
* Members:  See below
*
* Notes:
*
************/
struct mci_protocol_frame_s
{
  enum mci_protocol_frame_type_e  type;         /* the type of the frame */
  uint16_t                        crc;          /* CRC16 value of the frame exclude CRC field itself */
  uint16_t                        payload_len;  /* the entire command or data packet size in bytes */
  void                           *payloadp;     /* generic pointer to the frame payload data
                                                 * CMD:  struct mci_protocol_packet_s*
                                                 * DATA: uint8_t*
                                                 */
};

#endif /* MCIPROTOCOL_H */

