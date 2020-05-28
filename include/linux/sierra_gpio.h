/* kernel/include/linux/sierra_gpio.h
 *
 * Copyright (C) 2016 Sierra Wireless, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_SIERRA_GPIO_H
#define __LINUX_SIERRA_GPIO_H

extern int gpio_map_name_to_num(const char *buf, int len, bool force, long *gpio_num);
extern const char *gpio_map_num_to_name(long gpio_num, bool force, int *index);
extern void gpio_create_alias_link(const struct gpio_desc *desc, struct device *dev);
extern void gpio_remove_alias_link(const struct gpio_desc *desc);

/* Returns 0 on success to indicate that gpio was populated with a pointer to a gpio descriptor
 */
extern int gpio_alias_lookup(const char *alias, struct gpio_desc **gpio);

/* Define an alias from alias to the number in gpio_num.
 * If allow_overwrite is set, then overwrite existing alias without warning,
 * otherwise fail if the alias already exists.
 */
extern int gpio_alias_define(const char *alias, struct gpio_desc *gpio, bool allow_overwrite);

/* Remove the given alias.  It is an error to remove an alias that doesn’t exist
 */
extern int gpio_alias_undefine(const char *alias);

/* Function to  get the list of aliases that are mapped to a given GPIO.
 * Note that this function differs from the existing gpio_map_num_to_name function in that
 * it populates an array of aliases rather than just returning a single alias.
 * The function should return the error -EOVERFLOW if the in/out parameter num_aliases
 * indicates that the aliases array is too small to contain all of the aliases for the
 * given GPIO.
 */
extern int gpio_find_aliases(struct gpio_desc *gpio, const char **aliases, size_t *num_aliases);

extern int gpiochip_add_export_v2(struct device *dev, struct gpio_chip *chip);

extern void gpiochip_del_unexport_v2(struct device *dev, struct gpio_chip *chip);

#else

static inline int gpio_map_name_to_num(const char *buf, int len, bool force, long *gpio_num)
{
	return -ENOENT;
}

static inline const char *gpio_map_num_to_name(long gpio_num, bool force, int *index)
{
	return NULL;
}

static inline void gpio_create_alias_link(const struct gpio_desc *desc, struct device *dev)
{
}

static inline void gpio_remove_alias_link(const struct gpio_desc *desc);
{
}

static inline int gpio_alias_lookup(const char *alias, struct gpio_desc **gpio)
{
	return -ENOENT;
}

static inline int gpio_alias_define(const char *alias, struct gpio_desc *gpio, bool allow_overwrite)
{
	return -ENOSYS;
}

static inline int gpio_alias_undefine(const char *alias)
{
	return -ENOSYS;
}

static inline int gpio_find_aliases(struct gpio_desc *gpio, const char **aliases, size_t *num_aliases)
{
	return -ENOSYS;
}

static int gpiochip_add_export_v2(struct device *dev, struct gpio_chip *chip)
{
	return -ENOSYS;
}

static void gpiochip_del_unexport_v2(struct device *dev, struct gpio_chip *chip)
{
}
#endif /* __LINUX_SIERRA_GPIO_H */
