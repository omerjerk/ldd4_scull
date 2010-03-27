/*
 * A virtual bus for LDD sample code devices to plug into.  This
 * code is heavily borrowed from drivers/base/sys.c
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */
/* $Id: lddbus.c,v 1.9 2004/09/26 08:12:27 gregkh Exp $ */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/version.h>
#include "lddbus.h"

MODULE_AUTHOR("Jonathan Corbet");
MODULE_LICENSE("Dual BSD/GPL");
static char *Version = "$Revision: 1.9 $";

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
/*
 * Respond to hotplug events.
 */
static int ldd_hotplug(struct device *dev, char **envp, int num_envp,
		char *buffer, int buffer_size)
{
	envp[0] = buffer;
	if (snprintf(buffer, buffer_size, "LDDBUS_VERSION=%s",
			    Version) >= buffer_size)
		return -ENOMEM;
	envp[1] = NULL;
	return 0;
}
#else
/*
 * Respond to uevents.
 */
static int ldd_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct ldd_device *ldddev;

	if (!dev)
		return -ENODEV;

	ldddev = to_ldd_device(dev);
	if (!ldddev)
		return -ENODEV;

	if (add_uevent_var(env, "LDDBUS_VERSION=%s", dev_name(&ldddev->dev)))
		return -ENOMEM;

	return 0;
}
#endif

/*
 * Match LDD devices to drivers.  Just do a simple name test.
 */
static int ldd_match(struct device *dev, struct device_driver *driver)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
	return !strncmp(dev->bus_id, driver->name, strlen(driver->name));
#else
	struct ldd_device *ldddev = to_ldd_device(dev);
	return !strncmp(dev_name(&ldddev->dev), driver->name, strlen(driver->name));
#endif
}


/*
 * The LDD bus device.
 */
static void ldd_bus_release(struct device *dev)
{
	printk(KERN_DEBUG "lddbus release\n");
}

struct device ldd_bus = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
	.bus_id	    = "ldd0",
#else
	.init_name  = "ldd0",
#endif
	.release    = ldd_bus_release
};


/*
 * And the bus type.
 */
struct bus_type ldd_bus_type = {
	.name = "ldd",
	.match = ldd_match,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
	.hotplug  = ldd_hotplug,
#else
	.uevent = ldd_uevent,
#endif
};

/*
 * Export a simple attribute.
 */
static ssize_t show_bus_version(struct bus_type *bus, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", Version);
}

static BUS_ATTR(version, S_IRUGO, show_bus_version, NULL);



/*
 * LDD devices.
 */

/*
 * For now, no references to LDDbus devices go out which are not
 * tracked via the module reference count, so we use a no-op
 * release function.
 */
static void ldd_dev_release(struct device *dev)
{ }

int register_ldd_device(struct ldd_device *ldddev)
{
	ldddev->dev.bus = &ldd_bus_type;
	ldddev->dev.parent = &ldd_bus;
	ldddev->dev.release = ldd_dev_release;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
	strncpy(ldddev->dev.bus_id, ldddev->name, BUS_ID_SIZE);
#else
	ldddev->name = kasprintf(GFP_KERNEL, "%s", dev_name(&ldddev->dev));
#endif
	return device_register(&ldddev->dev);
}
EXPORT_SYMBOL(register_ldd_device);

void unregister_ldd_device(struct ldd_device *ldddev)
{
	device_unregister(&ldddev->dev);
}
EXPORT_SYMBOL(unregister_ldd_device);



/*
 * Crude driver interface.
 */


static ssize_t show_version(struct device_driver *driver, char *buf)
{
	struct ldd_driver *ldriver = to_ldd_driver(driver);

	sprintf(buf, "%s\n", ldriver->version);
	return strlen(buf);
}


int register_ldd_driver(struct ldd_driver *driver)
{
	int ret;

	driver->driver.bus = &ldd_bus_type;
	ret = driver_register(&driver->driver);
	if (ret)
		return ret;
	driver->version_attr.attr.name = "version";
	driver->version_attr.attr.owner = driver->module;
	driver->version_attr.attr.mode = S_IRUGO;
	driver->version_attr.show = show_version;
	driver->version_attr.store = NULL;
	return driver_create_file(&driver->driver, &driver->version_attr);
}

void unregister_ldd_driver(struct ldd_driver *driver)
{
	driver_unregister(&driver->driver);
}
EXPORT_SYMBOL(register_ldd_driver);
EXPORT_SYMBOL(unregister_ldd_driver);



static int __init ldd_bus_init(void)
{
	int ret;

	ret = bus_register(&ldd_bus_type);
	if (ret)
		return ret;
	if (bus_create_file(&ldd_bus_type, &bus_attr_version))
		printk(KERN_NOTICE "Unable to create version attribute\n");
	ret = device_register(&ldd_bus);
	if (ret)
		printk(KERN_NOTICE "Unable to register ldd0\n");
	return ret;
}

static void ldd_bus_exit(void)
{
	device_unregister(&ldd_bus);
	bus_unregister(&ldd_bus_type);
}

module_init(ldd_bus_init);
module_exit(ldd_bus_exit);
