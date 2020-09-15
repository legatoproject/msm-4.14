/************
 *
 * $Id$
 *
 * Filename:  sierra_ududefs - user definitions USB gadget driver
 *
 * Copyright: (c) 2012-2020 Sierra Wireless, Inc.
 *            All rights reserved
 *
 ************/
#ifndef SIERRA_UDUDEFS_H
#define SIERRA_UDUDEFS_H

#define UD_VENDOR_ID_QCT	0x05C6

#define UD_PID_68B1 0x68B1

/* Sierra USB Interface Information */
struct ud_usb_interface {
	unsigned number;
	char * name;
};

/*
  Define Interface assignments for each possible interface
  Interfaces may be exposed or not based on entry in "Start_USB" shell script
  Supports interface types that require more than one interface #
  Ordering is important for those interface types requiring more than one #
  Interface #'s can be listed multiple times, however, They will be assigned in
  first-come-first-served fashion.
  NOTE: To use interface #'s larger than 15 increase define MAX_CONFIG_INTERFACES
*/
static const struct ud_usb_interface ud_interface_generic[] = {
	/* Interface #      Name   */
	{	0,		"diag"				},
	{	1,		"adb"				},
	{	1,		"Function FS Gadget"		},
	{	2,		"nmea"				},
	{	3,		"modem"				},
	{	4,		"at" 				},
	{	5,		"raw_data"			},
	{	6,		"osa"				},
	{	8,		"rmnet"				},	/* Muxed rmnet interface */
	{	9,		"mass_storage"			},
	{	12,		"usb_mbim"			},
	{	13,		"usb_mbim"			},
	{	14,		"rndis"				},
	{	15,		"rndis"				},
	{	16,		"g_audio"			},
	{	17,		"g_audio"			},
	{	18,		"g_audio"			},
	{	19,		"cdc_ethernet"			},
	{	20,		"cdc_ethernet"			},
	{	21,		"cdc_network"			},	/* NCM */
	{	22,		"cdc_network"			},	/* NCM */
};

/* Special PID 68B1 for MBIM support */
static const struct ud_usb_interface ud_interface_68B1[] = {
	/* Interface #      Name   */
	{	0,		"usb_mbim"		},
	{	1,		"usb_mbim"		},
	{	2,		"diag"			},
	{	3,		"modem"			},
	{	4,		"nmea"			},
	{	5,		"mass_storage"		},
	{	6,		"adb"			},
	{	8,		"rmnet"			},
	{	14,		"rndis"			},
	{	15,		"rndis"			},
};

#define UD_INVALID_INTERFACE 255

static bool interface_reserved[MAX_CONFIG_INTERFACES];

static inline unsigned ud_get_interface_number( const char *interface_name, struct usb_configuration *config )
{
	unsigned interface_number = UD_INVALID_INTERFACE;
	unsigned i;
	unsigned max = ARRAY_SIZE(ud_interface_generic);
	const struct ud_usb_interface * ud_interface = &ud_interface_generic[0];

	/* Return invalid interface for non-Sierra products so that it falls
	 * back to default gadget behavior. Fixed/incontinuous interface
	 * number is only required for Sierra products
	 */
	if (UD_VENDOR_ID_QCT == config->cdev->desc.idVendor)
		return UD_INVALID_INTERFACE;

	if(config->cdev->desc.idProduct == UD_PID_68B1){
		ud_interface = &ud_interface_68B1[0];
		max = ARRAY_SIZE(ud_interface_68B1);
	}

	for (i = 0 ; i < max ; i++ ){
		interface_reserved[ud_interface->number] = true;
		if ( (strncmp(interface_name, ud_interface->name, strlen(ud_interface->name)) == 0) &&
			(config->interface[ud_interface->number] == NULL) ){
			/* Strings match */
			interface_number = ud_interface->number;
			break;
		}
		ud_interface++;
	}

	if( interface_number == UD_INVALID_INTERFACE ){
		/* Find next available */
		for(i=0 ; i<MAX_CONFIG_INTERFACES ; i++){
			if( (interface_reserved[i] == false) &&
				(config->interface[i] == NULL) ){
				interface_number = i;
				break;
			}
		}
		pr_info("No Match for Function Name: %s, Int #%d\n", interface_name, interface_number);
	}
	else
	{
		pr_info("Match for Function Name: %s, Int #%d\n", interface_name, interface_number);
	}

	return (interface_number);
}

#endif
