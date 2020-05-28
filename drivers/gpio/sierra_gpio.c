/************
 *
 * $Id$
 *
 * Filename:  sierra_gpio.c
 *
 * Purpose:   Name gpio and convert to pin num and function
 *
 * Copyright: (c) 2016 Sierra Wireless, Inc.
 *            All rights reserved
 *
 ************/
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/sierra_gpio.h>
#include <linux/sierra_bsudefs.h>
#include <linux/kdev_t.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include "../base/base.h"

#include "gpiolib.h"

DEFINE_SPINLOCK(alias_lock);
DEFINE_SPINLOCK(gpiochip_lock);

static struct kset *gpio_v2_kset;		/* /sys/class/gpio/v2/ */
static struct kset *gpiochip1_kset;		/* /sys/class/gpio/gpiochip1/ */
static struct kset *gpio_aliases_kset;		/* /sys/class/gpio/v2/aliases/ */
static struct kset *gpio_aliases_exported_kset;	/* /sys/class/gpio/v2/aliases_exported/ */
static struct class *gpio_class;		/* parent directory of /sys/class/gpio/v2/ */

static LIST_HEAD(gpiochip_list);
struct gpiochip_list {
	struct list_head	list;
	struct gpio_chip	*chip;
	struct device		*dev;
};

#define DRIVER_NAME		"sierra_gpio"
#define DT_COMPATIBLE		"sierra,gpio"
#define GPIO_ALIAS_PROPERTY	"alias-"

#define MAX_NB_GPIOS	100

struct gpio_alias_map {
	struct device_attribute	attr;		// attribute for the alias populated to /sys/class/gpio/aliases/
	char			*gpio_name;	// alias name
	int			gpio_num;	// gpio number
};

static u32			gpio_alias_count = 0;
static u32			gpio_alias_dt_count = 0;
static struct gpio_alias_map	gpio_alias_map[MAX_NB_GPIOS];

/* for RI PIN owner flag*/
#define RI_OWNER_MODEM      0
#define RI_OWNER_APP        1

static int	gpio_ri = -1;

static int _gpio_alias_define(const char *alias, struct gpio_desc *gpio, bool allow_overwrite);
static int _gpio_alias_undefine(const char *alias);

static struct gpio_desc *gpio_to_valid_desc(int gpio)
{
	return gpio_is_valid(gpio) ? gpio_to_desc(gpio) : NULL;
}

/**
 * gpio_sync_ri() - sync gpio RI function with riowner
 * Context: After ext_gpio and gpio_ri have been set.
 *
 * Returns 1 if apps, 0 if modem, or -1 if RI not found.
 */
static int gpio_sync_ri(void)
{
	int			ri_owner = -1;
	struct gpio_desc	*desc;

	if ((gpio_ri >= 0) && (desc = gpio_to_desc(gpio_ri))) {
		unsigned long flags;

		/* Check if RI gpio is owned by APP core
		 * In this case, set that gpio for RI management
		 * RI owner: 1 APP , 0 Modem. See AT!RIOWNER
		 */
		ri_owner = bsgetriowner();
		spin_lock_irqsave(&alias_lock, flags);
		if (RI_OWNER_APP == ri_owner) {
			if (!desc->owned_by_app_proc) {
				pr_debug("%s: RI owner is APP\n", __func__);
				desc->owned_by_app_proc = true;
			}
		} else {
			if (desc->owned_by_app_proc) {
				pr_debug("%s: RI owner is Modem\n", __func__);
				desc->owned_by_app_proc = false;
			}
		}
		spin_unlock_irqrestore(&alias_lock, flags);
	}

	return ri_owner;
}

/**
 * gpio_map_name_to_num() - Return the internal GPIO number for an
 *                         external GPIO name
 * @*buf: The external GPIO name (may include a trailing <lf>)
 * @len: The length of character into buf
 * @force: true if a check of function must be bypassed (useful to find anyway)
 * @*gpio_num: The GPIO number for this alias
 * Context: After gpiolib_sysfs_init has setup the gpio device
 *
 * Returns 0 if success, -ENOENT if the GPIO name is not mapped to a number
 * or -EPERM if the access to the GPIO is prohibited (except if force = true)
 *
 */
int gpio_map_name_to_num(const char *buf, int len, bool force, long *gpio_num)
{
	int	i;
	int	cmplen = len;

	if ((len > 0) && ((isspace(buf[len - 1]) || !buf[len - 1])))
		cmplen--; /* strip trailing <0x0a> from buf for compare ops */

	gpio_sync_ri();
	for (i = 0; i < gpio_alias_count; i++) {
		struct gpio_desc	*desc;
		if (gpio_alias_map[i].gpio_name &&
			(strlen(gpio_alias_map[i].gpio_name) == cmplen) &&
			(strncmp(buf, gpio_alias_map[i].gpio_name, cmplen) == 0)) {
			desc = gpio_to_valid_desc(gpio_alias_map[i].gpio_num);
			/* The multi-function GPIO is used as another feature, cannot export */
			if (!desc)
				return -EINVAL;
			if (!force && !desc->owned_by_app_proc)
				return -EPERM;

			*gpio_num = gpio_alias_map[i].gpio_num;
			pr_debug("%s: find GPIO %ld\n", __func__, *gpio_num);
			return 0;
		}
	}

	pr_debug("%s: Can not find GPIO %s\n", __func__, buf);
	return -ENOENT;
}
EXPORT_SYMBOL(gpio_map_name_to_num);

/**
 * gpio_map_num_to_name() - Return the external GPIO name for an
 *                         internal GPIO number
 * @gpio_num: The internal (i.e. MDM) GPIO pin number
 * @force: true if a check of function must be bypassed (useful to find anyway)
 * @*index: The next index n GPIO table after the matching number
 * Context: After gpiolib_sysfs_init has setup the gpio device
 *
 * Returns NULL if the GPIO number is not mapped to a name
 * or if the access to the GPIO is prohibited (except if force = true)
 *
 */
const char *gpio_map_num_to_name(long gpio_num, bool force, int *index)
{
	int	i;

	gpio_sync_ri();
	for (i = (*index > 0 ? *index : 0); i < gpio_alias_count; i++) {
		struct gpio_desc	*desc;
		if (gpio_alias_map[i].gpio_name && (gpio_num == gpio_alias_map[i].gpio_num)) {
			*index = (i + 1);
			desc = gpio_to_valid_desc(gpio_alias_map[i].gpio_num);
			if (!desc)
				return NULL;
			if (!force && !desc->owned_by_app_proc) {
				return NULL;
			}
			return gpio_alias_map[i].gpio_name;
		}
	}

	if (!(*index))
		pr_debug("%s: Can not find GPIO %ld\n", __func__, gpio_num);
	return NULL;
}
EXPORT_SYMBOL(gpio_map_num_to_name);

void gpio_create_alias_link(const struct gpio_desc *desc, struct device *dev)
{
	int		index = 0;
	const char	*ioname;

	if (desc) {
		char	gpioname[16]; /* "gpioNNNN\0" */

		snprintf(gpioname, sizeof(gpioname), "gpio%d", desc_to_gpio(desc));
		if (-1 == sysfs_create_link(&gpio_v2_kset->kobj, &dev->kobj, gpioname))
			pr_err("%s: Create link '%s' failed: %m\n", __func__, ioname);
	}

	while ((ioname = gpio_map_num_to_name(desc_to_gpio(desc), false, &index))) {
		if (-1 == sysfs_create_link(&gpio_aliases_exported_kset->kobj, &dev->kobj, ioname))
			pr_err("%s: Create link '%s' failed: %m\n", __func__, ioname);
	}
}
EXPORT_SYMBOL(gpio_create_alias_link);

void gpio_remove_alias_link(const struct gpio_desc *desc)
{
	int		index = 0;
	const char	*ioname;

	if (desc) {
		char	gpioname[16]; /* "gpioNNNN\0" */

		snprintf(gpioname, sizeof(gpioname), "gpio%d", desc_to_gpio(desc));
		sysfs_remove_link(&gpio_v2_kset->kobj, gpioname);
	}

	while ((ioname = gpio_map_num_to_name(desc_to_gpio(desc), true, &index))) {
		sysfs_remove_link(&gpio_aliases_exported_kset->kobj, ioname);
	}
}
EXPORT_SYMBOL(gpio_remove_alias_link);

/*
 * /sys/class/gpio/aliases/<alias_name> ... read-only
 *	gpiochip base,gpio offset related to this <alias_name>
 */
static ssize_t alias_gpio_num_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gpio_alias_map	*ext = (struct gpio_alias_map *)attr;
	struct gpio_desc	*desc;

	desc = gpio_to_valid_desc(ext->gpio_num);
	if (desc && desc->gdev && desc->gdev->chip)
		return sprintf(buf, "%d,%d\n", desc->gdev->chip->base,
				ext->gpio_num - desc->gdev->chip->base);
	else
		return sprintf(buf, "%d\n", ext->gpio_num);
	return 0;
}

static int gpio_create_alias_name_file(struct gpio_alias_map *ext)
{
	ext->attr.attr.name = ext->gpio_name;
	ext->attr.attr.mode = 0444;
	ext->attr.show = alias_gpio_num_show;
	ext->attr.store = NULL;
	return sysfs_create_file(&gpio_aliases_kset->kobj, &ext->attr.attr);
}

/*
 * /sys/class/gpio/v2/export ... write-only
 *	raw number of GPIO to export (full access)
 * /sys/class/gpio/v2/unexport ... write-only
 *	raw number of GPIO to unexport
 */
static ssize_t export_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t len)
{
	long			gpio;
	struct gpio_desc	*desc;
	int			status;

	status = kstrtol(buf, 0, &gpio);
	if (status < 0)
		goto done;

	desc = gpio_to_valid_desc(gpio);
	/* reject invalid GPIOs */
	if (!desc) {
		pr_err("%s: invalid GPIO %ld\n", __func__, gpio);
		return -EINVAL;
	}

	/* No extra locking here; FLAG_SYSFS just signifies that the
	 * request and export were done by on behalf of userspace, so
	 * they may be undone on its behalf too.
	 */

	status = gpiod_request(desc, "sysfs");
	if (status < 0) {
		if (status == -EPROBE_DEFER)
			status = -ENODEV;
		goto done;
	}
	status = gpiod_export(desc, true);
	if (status < 0)
		gpiod_free(desc);
	else
		set_bit(FLAG_SYSFS, &desc->flags);

done:
	if (status)
		pr_debug("%s: status %d\n", __func__, status);
	return status < 0 ? status : len;
}
static DEVICE_ATTR_WO(export);

static ssize_t unexport_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t len)
{
	long			gpio;
	struct gpio_desc	*desc;
	int			status;
	const char		*ioname;
	char			ioname_buf[128];
	int			index = 0;

	status = kstrtol(buf, 0, &gpio);
	if (status < 0)
		goto done;

	desc = gpio_to_valid_desc(gpio);
	/* reject bogus commands (gpio_unexport ignores them) */
	if (!desc) {
		pr_warn("%s: invalid GPIO %ld\n", __func__, gpio);
		return -EINVAL;
	}

	status = -EINVAL;

	/* No extra locking here; FLAG_SYSFS just signifies that the
	 * request and export were done by on behalf of userspace, so
	 * they may be undone on its behalf too.
	 */
	if (test_and_clear_bit(FLAG_SYSFS, &desc->flags)) {
		status = 0;
		gpiod_free(desc);
	}

	gpio_remove_alias_link(desc);

	while ((ioname = gpio_map_num_to_name(desc_to_gpio(desc), true, &index))) {
		snprintf(ioname_buf, sizeof(ioname_buf), "gpio%s", ioname);
		sysfs_remove_link(&gpio_class->p->subsys.kobj, ioname_buf);
	}

done:
	if (status)
		pr_debug("%s: status %d\n", __func__, status);
	return status < 0 ? status : len;
}
static DEVICE_ATTR_WO(unexport);

/*
 * /sys/class/gpio/alias_export ... write-only
 *	string name of GPIO to export (full access)
 * /sys/class/gpio/alias_unexport ... write-only
 *	string name of GPIO to unexport
 */
static ssize_t alias_export_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t len)
{
	long			gpio;
	struct gpio_desc	*desc;
	int			status;

	status = gpio_map_name_to_num(buf, len, false, &gpio);
	if (status < 0)
		goto done;

	desc = gpio_to_valid_desc(gpio);
	/* reject invalid GPIOs */
	if (!desc) {
		pr_warn("%s: invalid GPIO %ld\n", __func__, gpio);
		return -EINVAL;
	}

	/* No extra locking here; FLAG_SYSFS just signifies that the
	 * request and export were done by on behalf of userspace, so
	 * they may be undone on its behalf too.
	 */

	status = gpiod_request(desc, "sysfs");
	if (status < 0) {
		if (status == -EPROBE_DEFER)
			status = -ENODEV;
		goto done;
	}
	status = gpiod_export(desc, true);
	if (status < 0)
		gpiod_free(desc);
	else
		set_bit(FLAG_SYSFS, &desc->flags);

done:
	if (status)
		pr_debug("%s: status %d\n", __func__, status);
	return status < 0 ? status : len;
}
static DEVICE_ATTR_WO(alias_export);

static ssize_t alias_unexport_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t len)
{
	long			gpio;
	struct gpio_desc	*desc;
	int			status;
	const char		*ioname;
	char			ioname_buf[128];
	int			index = 0;

	status = gpio_map_name_to_num(buf, len, true, &gpio);
	if (status < 0)
		goto done;

	desc = gpio_to_valid_desc(gpio);
	/* reject bogus commands (gpio_unexport ignores them) */
	if (!desc) {
		pr_warn("%s: invalid GPIO %ld\n", __func__, gpio);
		return -EINVAL;
	}

	status = -EINVAL;

	/* No extra locking here; FLAG_SYSFS just signifies that the
	 * request and export were done by on behalf of userspace, so
	 * they may be undone on its behalf too.
	 */
	if (test_and_clear_bit(FLAG_SYSFS, &desc->flags)) {
		status = 0;
		gpiod_free(desc);
	}

	gpio_remove_alias_link(desc);

	while ((ioname = gpio_map_num_to_name(desc_to_gpio(desc), true, &index))) {
		snprintf(ioname_buf, sizeof(ioname_buf), "gpio%s", ioname);
		sysfs_remove_link(&gpio_class->p->subsys.kobj, ioname_buf);
	}

done:
	if (status)
		pr_debug("%s: status %d\n", __func__, status);
	return status < 0 ? status : len;
}
static DEVICE_ATTR_WO(alias_unexport);

/*
 * /sys/class/gpio/alias_define ... write-only: name:<num> or name:<base>,<offset>
 *	string name of GPIO to create/define and its GPIO number
 * /sys/class/gpio/alias_undefine ... write-only
 *	string name of GPIO to destroy/undefine
 */
static ssize_t alias_define_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t len)
{
	int			status;
	unsigned int		base = 0;
	unsigned int		gpio;
	struct gpio_desc	*desc;
	char			*p;
	char			*pp;
	char			*ioname = kmemdup(buf, len + 1, GFP_KERNEL);

	if (!ioname) {
		return -ENOMEM;
	}
	p = strchr(ioname, ':');
	if (!p || (p == ioname)) {
		pr_err("%s: incorrect syntax name:num\n", __func__);
		status = -EINVAL;
		goto done_free;
	}
	*(p++) = '\0';
	pp = strchr(p, ',');
	if (pp) {
		*pp = '\0';
		status = kstrtouint(p, 10, &base);
		if (status < 0)
			goto done_free;
		p = pp + 1;
	}
	status = kstrtouint(p, 10, &gpio);
	if (status < 0)
		goto done_free;
	gpio += base;

	desc = gpio_to_valid_desc(gpio);
	/* reject invalid GPIOs */
	if (!desc) {
		pr_err("%s: invalid GPIO %u\n", __func__, gpio);
		status = -EINVAL;
		goto done_free;
	}

	status = _gpio_alias_define(ioname, desc, false);

done_free:
	kfree(ioname);
	return status < 0 ? status : len;
}
static DEVICE_ATTR_WO(alias_define);

static ssize_t alias_undefine_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t len)
{
	int	status;
	char	*ioname = kmemdup(buf, len + 1, GFP_KERNEL);

	if (!ioname)
		return -ENOMEM;
	if (ioname[len - 1] < 0x20)
		ioname[len - 1] = '\0';

	status = _gpio_alias_undefine(ioname);
	kfree(ioname);
	return status < 0 ? status : len;
}
static DEVICE_ATTR_WO(alias_undefine);

/**
 * alias_map_show() - Display the GPIO aliases/pin/owned_by_app_proc map
 * @dev: The device
 * @buf: The buffer (1 page) to put the GPIO map to display
 *
 * Returns the number of characters to display
 */
static ssize_t alias_map_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int	i;
	int	status = 0;

	gpio_sync_ri();
	for (i = 0; i < gpio_alias_count; i++) {
		if (gpio_alias_map[i].gpio_name) {
			struct gpio_desc *desc;
			desc = gpio_to_desc(gpio_alias_map[i].gpio_num);
			if (desc && desc->gdev && desc->gdev->chip)
				status += snprintf(buf + status, PAGE_SIZE - status, "%4d,%-4d %c \"%s\"\n",
							desc->gdev->chip->base,
							gpio_alias_map[i].gpio_num - desc->gdev->chip->base,
							desc->owned_by_app_proc ? 'A' : 'M',
							gpio_alias_map[i].gpio_name);
			else
				status += snprintf(buf + status, PAGE_SIZE - status, "%9d %c \"%s\"\n",
							gpio_alias_map[i].gpio_num,
							'A',
							gpio_alias_map[i].gpio_name);
		}
	}
	return status;
}
static DEVICE_ATTR_RO(alias_map);

/**
 * mask_show() - Display the GPIO chip 1 mask
 * @dev: The device
 * @buf: The buffer (1 page) to put the GPIO chip 1 mask
 *
 * Returns the number of characters to display
 */
static ssize_t mask_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct gpio_chip  *chip = gpio_to_desc(1)->gdev->chip;

	return sprintf(buf, "0x%08x%08x\n", (u32)(chip->mask[0] >> 32), (u32)chip->mask[0]);
}
static DEVICE_ATTR_RO(mask);

/**
 * mask_v2_show() - Display the GPIO chip 1 mask
 * @dev: The device
 * @buf: The buffer (1 page) to put the GPIO chip 1 mask
 *
 * Returns the number of characters to display
 */
static ssize_t mask_v2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	const struct gpio_chip	*chip = gpio_to_desc(1)->gdev->chip;
	int			len = 0;
	int			i;
	size_t			nbit = sizeof(chip->mask[0]) * 8;

	for (i = 0; i < chip->ngpio; i += 8) {
		len += sprintf(buf + len, "%02llx ",
				(chip->bitmask_valid ?
					(chip->mask[i / nbit] >> (i % nbit)) & 0xff : 0xff));
	}
	return len + sprintf(buf + len, "\n");
}
static DEVICE_ATTR_RO(mask_v2);

static void gpiochip_export_v2(struct gpiochip_list *chip)
{
	int			status = 0;
	char			gpiochipname[16]; /* "gpiochipNNNN\0" */

	snprintf(gpiochipname, sizeof(gpiochipname), "gpiochip%u", chip->chip->base);
	pr_info("%s: Export gpiochip %s [%u,%u] to v2\n",
		__func__, chip->chip->label, chip->chip->base, chip->chip->ngpio);
	status = sysfs_create_link(&gpio_v2_kset->kobj, &chip->dev->kobj, gpiochipname);
	if (status)
		pr_err("%s: Failed to create link while exporting gpiochip %s to v2: err=%d\n",
			 __func__, chip->chip->label, status);
}

int gpiochip_add_export_v2(struct device *dev, struct gpio_chip *chip)
{
	struct gpiochip_list	*thischip = NULL;
	struct gpiochip_list	*newchip = NULL;
	struct list_head	*list;
	unsigned long		flags;

	newchip = kmalloc(sizeof(struct gpiochip_list), GFP_KERNEL);
	if (!newchip) {
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&newchip->list);
	newchip->chip = chip;
	newchip->dev = dev;
	spin_lock_irqsave(&gpiochip_lock, flags);
	list_for_each(list, &gpiochip_list) {
		thischip = list_entry(list, struct gpiochip_list, list);
		if ((thischip->chip->base == chip->base) && (thischip->chip->ngpio == chip->ngpio) &&
			(0 == strcmp(thischip->chip->label, chip->label))) {
			break;
		}
		thischip = NULL;
	}
	if (!thischip) {
		list_add_tail(&newchip->list, &gpiochip_list);
	}
	spin_unlock_irqrestore(&gpiochip_lock, flags);
	if (thischip) {
		kfree(newchip);
		newchip = thischip;
	}
	if (gpio_v2_kset)
		gpiochip_export_v2(newchip);
	return 0;
}

static void gpiochip_unexport_v2(struct gpiochip_list *chip)
{
	char	gpiochipname[16]; //"gpiochipNNNN\0"

	snprintf(gpiochipname, sizeof(gpiochipname), "gpiochip%u", chip->chip->base);
	pr_info("%s: Unxport gpiochip %s [%u,%u] to v2\n",
		 __func__, chip->chip->label, chip->chip->base, chip->chip->ngpio);
	sysfs_remove_link(&gpio_v2_kset->kobj, gpiochipname);
}

void gpiochip_del_unexport_v2(struct device *dev, struct gpio_chip *chip)
{
	struct gpiochip_list	*thischip = NULL;
	struct list_head	*list;
	unsigned long		flags;

	spin_lock_irqsave(&gpiochip_lock, flags);
	list_for_each(list, &gpiochip_list) {
		thischip = list_entry(list, struct gpiochip_list, list);
		if ((thischip->chip->base == chip->base) && (thischip->chip->ngpio == chip->ngpio) &&
			(0 == strcmp(thischip->chip->label, chip->label))) {
			list_del(&thischip->list);
			break;
		}
		thischip = NULL;
	}
	spin_unlock_irqrestore(&gpiochip_lock, flags);
	if (thischip) {
		if (gpio_v2_kset)
			gpiochip_unexport_v2(thischip);
		kfree(thischip);
	}
}

static int sierra_gpio_probe(struct platform_device *pdev)
{
	struct device_node	*np;
	int			ngpios = 0;
	int			status;
	int			gpio;
	struct property		*pp;
	int			dummy;
	struct gpiochip_list	*chip;
	struct list_head	*list;
	unsigned long		flags;

	if (!pdev) {
		pr_err("%s: NULL input parameter\n", __func__);
		return -EINVAL;
	}

	gpio_class = gpio_class_get();
	if (!gpio_class) {
		dev_err(&pdev->dev, "NO class for gpio\n");
		return -ENOENT;
	}

	gpiochip1_kset = kset_create_and_add("gpiochip1", NULL, &gpio_class->p->subsys.kobj);
	if (!gpiochip1_kset) {
		dev_err(&pdev->dev, "No more memory to create gpiochip1 kset\n");
		return -ENOMEM;
	}

	status = sysfs_create_file(&gpiochip1_kset->kobj, &dev_attr_mask.attr);
	if (status < 0)
		dev_err(&pdev->dev, "Cannot create file gpiochip1/mask: %d\n", status);
	status = sysfs_create_file(&gpiochip1_kset->kobj, &dev_attr_mask_v2.attr);
	if (status < 0)
		dev_err(&pdev->dev, "Cannot create file gpiochip1/mask_v2: %d\n", status);

	gpio_v2_kset = kset_create_and_add("v2", NULL, &gpio_class->p->subsys.kobj);
	if (!gpio_v2_kset) {
		dev_err(&pdev->dev, "No more memory to create v2 kset\n");
		return -ENOMEM;
	}

	gpio_aliases_kset = kset_create_and_add("aliases", NULL, &gpio_v2_kset->kobj);
	if (!gpio_aliases_kset) {
		dev_err(&pdev->dev, "No more memory to create aliases kset\n");
		return -ENOMEM;
	}

	gpio_aliases_exported_kset = kset_create_and_add("aliases_exported", NULL, &gpio_v2_kset->kobj);
	if (!gpio_aliases_exported_kset) {
		dev_err(&pdev->dev, "No more memory to create aliases_exported kset\n");
		return -ENOMEM;
	}

	status = sysfs_create_link(&gpio_v2_kset->kobj, &gpiochip1_kset->kobj, "gpiochip1");
	if (status < 0)
		dev_err(&pdev->dev, "Cannot create v2/gpiochip1: %d\n", status);
	status = sysfs_create_file(&gpio_v2_kset->kobj, &dev_attr_export.attr);
	if (status < 0)
		dev_err(&pdev->dev, "Cannot create file v2/export: %d\n", status);
	status = sysfs_create_file(&gpio_v2_kset->kobj, &dev_attr_unexport.attr);
	if (status < 0)
		dev_err(&pdev->dev, "Cannot create file v2/unexport: %d\n", status);
	status = sysfs_create_file(&gpio_v2_kset->kobj, &dev_attr_alias_export.attr);
	if (status < 0)
		dev_err(&pdev->dev, "Cannot create file v2/alias_export: %d\n", status);
	status = sysfs_create_file(&gpio_v2_kset->kobj, &dev_attr_alias_unexport.attr);
	if (status < 0)
		dev_err(&pdev->dev, "Cannot create file v2/alias_unexport: %d\n", status);
	status = sysfs_create_file(&gpio_v2_kset->kobj, &dev_attr_alias_define.attr);
	if (status < 0)
		dev_err(&pdev->dev, "Cannot create file v2/alias_define: %d\n", status);
	status = sysfs_create_file(&gpio_v2_kset->kobj, &dev_attr_alias_undefine.attr);
	if (status < 0)
		dev_err(&pdev->dev, "Cannot create file v2/alias_undefine: %d\n", status);
	status = sysfs_create_file(&gpio_v2_kset->kobj, &dev_attr_alias_map.attr);
	if (status < 0)
		dev_err(&pdev->dev, "Cannot create file v2/alias_map: %d\n", status);

	np = pdev->dev.of_node;
	for_each_property_of_node(np, pp) {
		struct gpio_desc	*desc;

		dev_err(&pdev->dev, "property \"%s\": length %d\n", pp->name, pp->length);
		if (strcmp(pp->name, "compatible") == 0)
			continue;
		if (strncmp(pp->name, GPIO_ALIAS_PROPERTY, sizeof(GPIO_ALIAS_PROPERTY) - 1)) {
			dev_err(&pdev->dev, "Skipping unknown property \"%s\"\n", pp->name);
			continue;
		}
		if (pp->length == (sizeof(u32)*2))
		{
			gpio = -1;
			of_property_read_u32(np, pp->name, &gpio);
		}
		else
		{
			gpio = of_get_named_gpio_flags(np, pp->name, 0, (enum of_gpio_flags *)&dummy);
		}
		if (gpio >= 0) {
			if (ngpios == MAX_NB_GPIOS) {
				dev_err(&pdev->dev, "Too many aliases\n");
				break;
			}
			desc = gpio_to_desc(gpio);
			/* skip "alias-" from DT name */
			gpio_alias_map[ngpios].gpio_name = pp->name + sizeof(GPIO_ALIAS_PROPERTY) - 1;
			gpio_alias_map[ngpios].gpio_num = gpio;
			if (desc)
				desc->owned_by_app_proc = !(-1 == gpio_alias_map[ngpios].gpio_num);
			if (desc && desc->gdev && desc->gdev->chip && (desc->gdev->chip->bitmask_valid)
				&& (desc->bit_in_mask >= 0) && (desc->bit_in_mask < desc->gdev->chip->ngpio)) {
				int nmask = desc->bit_in_mask / (sizeof(desc->gdev->chip->mask[0]) * 8);
				int nbit = desc->bit_in_mask % (sizeof(desc->gdev->chip->mask[0]) * 8);
				desc->owned_by_app_proc = (desc->gdev->chip->mask[nmask] >> nbit) & 1ULL;
				if (desc->gdev->chip->max_bit < desc->bit_in_mask)
					desc->gdev->chip->max_bit = desc->bit_in_mask;
			}
			if (test_bit(FLAG_RING_INDIC, &desc->flags) && gpio_ri == -1) {
				gpio_ri = gpio_alias_map[ngpios].gpio_num;
				gpio_sync_ri();
			}
			gpio_create_alias_name_file(&gpio_alias_map[ngpios]);
			dev_info(&pdev->dev, "%d PIN %d FUNC %d NAME \"%s\"\n",
				ngpios, gpio_alias_map[ngpios].gpio_num,
				desc ? desc->owned_by_app_proc : 1,
				gpio_alias_map[ngpios].gpio_name);
			ngpios++;
		}
	}
	gpio_alias_dt_count = gpio_alias_count = ngpios;

	for (ngpios = 0; ngpios < gpio_alias_count; ngpios++) {
		struct gpio_desc	*desc;

		desc = gpio_to_desc(gpio_alias_map[ngpios].gpio_num);
		if (desc && desc->gdev && desc->gdev->chip) {
			if (desc->gdev->chip->bitmask_valid && desc->gdev->chip->max_bit != -1) {
				int mask_array_size = (desc->gdev->chip->ngpio +
					8 * sizeof(desc->gdev->chip->mask[0]) - 1) /
					(8 * sizeof(desc->gdev->chip->mask[0]));
				u64 mask = ((1ULL << (desc->gdev->chip->max_bit %
                                                         (sizeof(desc->gdev->chip->mask[0]) * 8) + 1)) - 1ULL);
				int nmask = desc->gdev->chip->max_bit / (sizeof(desc->gdev->chip->mask[0]) * 8);
				desc->gdev->chip->mask[nmask++] &= mask;
				while (nmask < mask_array_size)
					desc->gdev->chip->mask[nmask++] = 0;
			}
			else
				desc->gdev->chip->max_bit = -1;
		}
	}

	spin_lock_irqsave(&gpiochip_lock, flags);
	list_for_each(list, &gpiochip_list) {
		chip = list_entry(list, struct gpiochip_list, list);
		dev_info(&pdev->dev, "Export to v2 gpiochip %s\n", chip->chip->label);
		spin_unlock_irqrestore(&gpiochip_lock, flags);
		gpiochip_export_v2(chip);
		spin_lock_irqsave(&gpiochip_lock, flags);
	}
	spin_unlock_irqrestore(&gpiochip_lock, flags);

	return status;
}

/**
 * Returns 0 on success to indicate that gpio was populated with a pointer to a gpio descriptor
 */
int gpio_alias_lookup(const char *alias, struct gpio_desc **gpio)
{
	int	i;

	gpio_sync_ri();
	for (i = 0; i < gpio_alias_count; i++) {
		if (gpio_alias_map[i].gpio_name &&
			(strcmp(alias, gpio_alias_map[i].gpio_name) == 0)) {
			*gpio = gpio_to_desc(gpio_alias_map[i].gpio_num);
			pr_debug("%s: alias %s, find GPIO %d\n", __func__, alias, gpio_alias_map[i].gpio_num);
			return 0;
		}
	}

	pr_debug("%s: Can not find GPIO %s\n", __func__, alias);
	return -ENOENT;
}
EXPORT_SYMBOL(gpio_alias_lookup);

/**
 * Define an alias from alias to the number in gpio_num.
 * If allow_overwrite is set, then overwrite existing alias without warning,
 * otherwise fail if the alias already exists.
 */
static int _gpio_alias_define(const char *alias, struct gpio_desc *gpio, bool allow_overwrite)
{
	long			gpio_num = desc_to_gpio(gpio);
	int			status;
	char			*ioname = NULL;
	int			i;
	struct gpio_alias_map	*ext = NULL;
	unsigned long		flags;

	/* reject invalid GPIOs */
	if (!gpio_to_desc(gpio_num)) {
		pr_warn("%s: invalid GPIO %ld\n", __func__, gpio_num);
		status = -EINVAL;
		goto done;
	}

	ioname = kstrdup(alias, GFP_KERNEL);
	if (!ioname) {
		pr_err("%s: No space to allocate GPIO alias name\n", __func__);
		status = -ENOMEM;
		goto done;
	}
	spin_lock_irqsave(&alias_lock, flags);
	for (i = 0; i <gpio_alias_count; i++) {
		if (gpio_alias_map[i].gpio_name &&
			(strcmp(alias, gpio_alias_map[i].gpio_name) == 0)) {
			if (!allow_overwrite) {
				pr_err("%s: GPIO aliases already exists\n", __func__);
				status = -EEXIST;
				goto done_restore;
			}
			ext = &gpio_alias_map[i];
			kfree(ext->gpio_name);
			break;
		}
		if (!ext && !gpio_alias_map[i].gpio_name)
			ext = &gpio_alias_map[i];
	}

	if (!ext && gpio_alias_count == MAX_NB_GPIOS) {
		pr_err("%s: too many GPIO aliases\n", __func__);
		status = -ENOMEM;
		goto done_restore;
	}
	if (!ext)
		ext = &gpio_alias_map[gpio_alias_count];

	ext->gpio_name = ioname;
	ext->gpio_num = gpio_num;
	gpio->owned_by_app_proc = true;
	spin_unlock_irqrestore(&alias_lock, flags);
	status = gpio_create_alias_name_file(ext);

done:
	if (status) {
		if (ioname)
			kfree(ioname);
		if (ext)
			memset(ext, 0, sizeof(struct gpio_alias_map));
		pr_debug("%s: status %d\n", __func__, status);
	}
	else if (ext == &gpio_alias_map[gpio_alias_count])
		/* New GPIO aliases is registered */
		gpio_alias_count++;
	return status;

done_restore:
	spin_unlock_irqrestore(&alias_lock, flags);
	goto done;
}

int gpio_alias_define(const char *alias, struct gpio_desc *gpio, bool allow_overwrite)
{
	return _gpio_alias_define(alias, gpio, allow_overwrite);
}
EXPORT_SYMBOL(gpio_alias_define);

/**
 * Remove the given alias.  It is an error to remove an alias that doesn’t exist
 */
static int _gpio_alias_undefine(const char *alias)
{
	int			status;
	int			i;
	unsigned long		flags;
	struct device_attribute	thisattr;
	char			*thisname;

	status = -ENOENT;
	spin_lock_irqsave(&alias_lock, flags);
	for (i = 0; i <gpio_alias_count; i++) {
		if (strcmp(alias, gpio_alias_map[i].gpio_name) == 0) {
			status = 0;
			break;
		}
	}
	if (status)
		goto done_restore;

	if (i < gpio_alias_dt_count) {
		pr_err("%s: Cannot destroy GPIO alias %s created by device tree\n",
			 __func__, gpio_alias_map[i].gpio_name);
		status = -EPERM;
		goto done_restore;
	}

	/* No extra locking here; FLAG_SYSFS just signifies that the
	 * request and export were done by on behalf of userspace, so
	 * they may be undone on its behalf too.
	 */
	if (test_bit(FLAG_SYSFS, &gpio_to_desc(gpio_alias_map[i].gpio_num)->flags)) {
		pr_err("%s: Cannot destroy GPIO alias currently exported\n", __func__);
		status = -EBUSY;
		goto done_restore;
	}

	thisname = gpio_alias_map[i].gpio_name;
	thisattr = gpio_alias_map[i].attr;
	if (i < (gpio_alias_count - 1))
		memmove(&gpio_alias_map[i], &gpio_alias_map[i + 1],
			sizeof(gpio_alias_map[0]) * (gpio_alias_count - i - 1));
	gpio_alias_count--;
	spin_unlock_irqrestore(&alias_lock, flags);
	sysfs_remove_file(&gpio_aliases_kset->kobj, &thisattr.attr);
	kfree(thisname);

done:
	if (status) {
		pr_debug("%s: status %d\n", __func__, status);
	}
	return status;

done_restore:
	spin_unlock_irqrestore(&alias_lock, flags);
	goto done;
}

int gpio_alias_undefine(const char *alias)
{
	return _gpio_alias_undefine(alias);
}
EXPORT_SYMBOL(gpio_alias_undefine);

/**
 * Function to  get the list of aliases that are mapped to a given GPIO.
 * Note that this function differs from the existing gpio_map_num_to_name function in that
 * it populates an array of aliases rather than just returning a single alias.
 * The function should return the error -EOVERFLOW if the in/out parameter num_aliases
 * indicates that the aliases array is too small to contain all of the aliases for the
 * given GPIO.
 */
int gpio_find_aliases(struct gpio_desc *desc, const char **aliases, size_t *num_aliases)
{
	int	gpio = desc_to_gpio(desc);
	int	naliases = 0;

	if (gpio_to_desc(gpio)) {
		int	i;

		for (i = 0; i < gpio_alias_count; i++) {
			if (gpio_alias_map[i].gpio_num == gpio) {
				if (naliases < *num_aliases)
					aliases[naliases++] = gpio_alias_map[i].gpio_name;
				else
					return -ENOMEM;
			}
		}
		*num_aliases = naliases;
		return naliases;
	}
	return -EINVAL;
}
EXPORT_SYMBOL(gpio_find_aliases);

static int sierra_gpio_remove(struct platform_device *pdev)
{
	pr_info("sierra_gpio_remove");
	gpio_alias_dt_count = gpio_alias_count = 0;
	return 0;
}

static const struct of_device_id sierra_gpio[] = {
	{ .compatible = DT_COMPATIBLE },
	{},
};
MODULE_DEVICE_TABLE(of, sierra_gpio);

static struct platform_driver sierra_gpio_driver = {
	.driver = {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(sierra_gpio),
		},
	.probe = sierra_gpio_probe,
	.remove = sierra_gpio_remove,
};

static int __init sierra_gpio_init(void)
{
	return platform_driver_register(&sierra_gpio_driver);
}

static void __exit sierra_gpio_exit(void)
{
	platform_driver_unregister(&sierra_gpio_driver);
}

module_init(sierra_gpio_init);
module_exit(sierra_gpio_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Sierra GPIO driver");
