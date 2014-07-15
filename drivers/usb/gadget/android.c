/*
 * Gadget Driver for Android
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 * Author: Mike Lockwood <lockwood@android.com>
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

/* #define DEBUG */
/* #define VERBOSE_DEBUG */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/debugfs.h>
#include <linux/types.h>

#include <linux/usb/android_composite.h>
#include <linux/usb/ch9.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>

#include "gadget_chips.h"
#include "u_serial.h"
#include <linux/miscdevice.h>
#include <linux/wakelock.h>

/*
 * Kbuild is not very cooperative with respect to linking separately
 * compiled library objects into one module.  So for now we won't use
 * separate compilation ... ensuring init/exit sections work to shrink
 * the runtime footprint, and giving us at least some parts of what
 * a "gcc --combine ... part1.c part2.c part3.c ... " build would.
 */
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"
#include "composite.c"

MODULE_AUTHOR("Mike Lockwood");
MODULE_DESCRIPTION("Android Composite USB Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

/*xingbeilei-20110825-begin*/
static u16 global_product_id;
static int android_set_pid(const char *val, struct kernel_param *kp);
static int android_get_pid(char *buffer, struct kernel_param *kp);
module_param_call(product_id, android_set_pid, android_get_pid,
                  &global_product_id, 0664);
MODULE_PARM_DESC(product_id, "USB device product id");
/*xingbeilei-20110825-end*/

static const char longname[] = "Gadget Android";

/* Default vendor and product IDs, overridden by platform data */
#define VENDOR_ID		0x18D1
#define PRODUCT_ID		0x0001

#if 1
/*COMMON*/
#define PRODUCT_ID_DIAG_MODEM_NMEA_MS_ADB          		0x1350
#define   PRODUCT_ID_ALL_INTERFACE    PRODUCT_ID_DIAG_MODEM_NMEA_MS_ADB

#define PRODUCT_ID_MS_ADB                      			0x1351
#define PRODUCT_ID_ADB                             			0x1352
#define PRODUCT_ID_MS                               			0x1353
#define PRODUCT_ID_MODEM_MS_ADB         		0x1354
#define PRODUCT_ID_MODEM_MS                 			0x1355

#define PRODUCT_ID_MS_CDROM                 			0x0083

#define PRODUCT_ID_DIAG_NMEA_MODEM   		0x0111
#define PRODUCT_ID_DIAG                           			0x0112	

/*froyo RNDIS*/
#define PRODUCT_ID_RNDIS_MS                 			0x1364
#define PRODUCT_ID_RNDIS_MS_ADB             		0x1364
#define PRODUCT_ID_RNDIS             				0x1365
#define PRODUCT_ID_RNDIS_ADB					0x1373

/*n600 pcui*/
#define PRODUCT_ID_DIAG_MODEM_NEMA_MS_ADB_AT   	0x1366
#define PRODUCT_ID_DIAG_MODEM_NEMA_MS_AT   		0x1367

/*DELL project*/
#define PRODUCT_ID_DIAG_MODEM_NMEA_MS_ADB_DELL          	0x1368
#define PRODUCT_ID_MS_ADB_DELL                      	0x1369
#define PRODUCT_ID_MS_DELL                               		0x1370
#define PRODUCT_ID_MODEM_MS_ADB_DELL         	0x1371
#define PRODUCT_ID_MODEM_MS_DELL                 	0x1372
#endif
//ruanmeisi_20100712 for cdrom auto install driver
static unsigned short g_enable_cdrom = 0;
struct usb_ex_work
{
	struct workqueue_struct *workqueue;
	int    enable_switch;
	int    enable_linux_switch;
	int switch_pid;
	int has_switch;
	int cur_pid;
	int linux_pid;
	struct delayed_work switch_work;
	struct delayed_work linux_switch_work;
	struct delayed_work plug_work;
	spinlock_t lock;
	struct wake_lock	wlock;
};

struct usb_ex_work global_usbwork = {0};
static int create_usb_work_queue(void);
static int destroy_usb_work_queue(void);
static void usb_plug_work(struct work_struct *w);
static void usb_switch_work(struct work_struct *w);
static void usb_switch_os_work(struct work_struct *w);
static void clear_switch_flag(void);
static int usb_cdrom_is_enable(void);
static int is_cdrom_enabled_after_switch(void);
struct android_usb_product *android_validate_product_id(unsigned short pid);
//end
struct android_dev {
	struct usb_composite_dev *cdev;
	struct usb_configuration *config;
	int num_products;
	struct android_usb_product *products;
	int num_functions;
	char **functions;

	//xingbeilei begin
	char **bind_functions;
	int bind_num_functions;
	struct mutex lock;
	//end

	int product_id;
	int version;
};

static struct android_dev *_android_dev;

/*xingbeilei-20110827 begin*/
static atomic_t adb_enable_excl;

int is_adb_enabled(void)
{
        return 1 == atomic_read(&adb_enable_excl);
}

static int current_pid(void)
{
	return _android_dev?_android_dev->product_id:0;
}

int support_assoc_desc(void)
{
	return current_pid() == PRODUCT_ID_RNDIS ? 0 : 1;
}
/*xingbeilei-20110827 end*/

#define MAX_STR_LEN		64
/* string IDs are assigned dynamically */

#define STRING_MANUFACTURER_IDX		0
#define STRING_PRODUCT_IDX		1
#define STRING_SERIAL_IDX		2

static char serial_number[MAX_STR_LEN];
//xingbeilei begin
static struct kparam_string kps = {
        .string                 = serial_number,
        .maxlen                 = MAX_STR_LEN,
};

static int android_set_sn(const char *kmessage, struct kernel_param *kp);
module_param_call(serial_number, android_set_sn, param_get_string,
		  &kps, 0664);
MODULE_PARM_DESC(serial_number, "SerialNumber string");
//end

/* String Table */
static struct usb_string strings_dev[] = {
	/* These dummy values should be overridden by platform data */
	[STRING_MANUFACTURER_IDX].s = "Android",
	[STRING_PRODUCT_IDX].s = "Android",
	[STRING_SERIAL_IDX].s = "0123456789ABCDEF",
	{  }			/* end of list */
};

static struct usb_gadget_strings stringtab_dev = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_dev,
};

static struct usb_gadget_strings *dev_strings[] = {
	&stringtab_dev,
	NULL,
};

static struct usb_device_descriptor device_desc = {
	.bLength              = sizeof(device_desc),
	.bDescriptorType      = USB_DT_DEVICE,
	.bcdUSB               = __constant_cpu_to_le16(0x0200),
	.bDeviceClass         = USB_CLASS_PER_INTERFACE,
	.idVendor             = __constant_cpu_to_le16(VENDOR_ID),
	.idProduct            = __constant_cpu_to_le16(PRODUCT_ID),
	.bcdDevice            = __constant_cpu_to_le16(0xffff),
	.bNumConfigurations   = 1,
};

static struct usb_otg_descriptor otg_descriptor = {
	.bLength =		sizeof otg_descriptor,
	.bDescriptorType =	USB_DT_OTG,
	.bmAttributes =		USB_OTG_SRP | USB_OTG_HNP,
	.bcdOTG               = __constant_cpu_to_le16(0x0200),
};

static const struct usb_descriptor_header *otg_desc[] = {
	(struct usb_descriptor_header *) &otg_descriptor,
	NULL,
};

static struct list_head _functions = LIST_HEAD_INIT(_functions);
static bool _are_functions_bound;

//xingbeilei 20110829 begin
enum usb_opt_nv_item
{
	NV_BACK_LIGHT_I=77,//nv77 used for config/store usb mode
	NV_FTM_MODE_I = 453// FTM mode
};
enum usb_opt_nv_type
{
	NV_READ=0,
        NV_WRITE
};
int
msm_hsusb_get_set_usb_conf_nv_value(uint32_t nv_item,uint32_t value,uint32_t is_write);
int get_ftm_from_tag(void);

#define NV_WRITE_SUCCESS 10

static int ftm_mode = 0;
static int pid_from_nv = 0;

static int is_ftm_mode(void)
{
	return !!ftm_mode;
}
static void set_ftm_mode(int i)
{
	ftm_mode = i;
	return ;
}
static int is_pid_configed_from_nv(void)
{
	return !!pid_from_nv;
}
static void set_pid_from_nv(int i)
{
	pid_from_nv = i;
	return ;
}

static int get_nv(void)
{
	return msm_hsusb_get_set_usb_conf_nv_value(NV_BACK_LIGHT_I,0,NV_READ);
}
static int set_nv(int nv)
{
	int r = msm_hsusb_get_set_usb_conf_nv_value(NV_BACK_LIGHT_I,nv,NV_WRITE);
	return (r == NV_WRITE_SUCCESS)? 0:-1;
}
static int config_pid_from_nv(void)
{
	int i = 0;
	printk("usb: %s, %d\n", __FUNCTION__, __LINE__);
	if (is_ftm_mode()) {
		return 0;
	}
	i = get_nv();
	//if nv77 == 4,  config usb pid from nv
	set_pid_from_nv(i == 4? 1:0);
	if (is_pid_configed_from_nv()) {
		printk(KERN_ERR"config pid from nv\n");
	}
	return is_pid_configed_from_nv();
}
static int config_ftm_from_tag(void)
{
	printk("usb: %s, %d\n", __FUNCTION__, __LINE__);
	if (is_ftm_mode()) {
		return 0;
	}

	set_ftm_mode(get_ftm_from_tag());

	printk("usb: %s, %d: ftm_mode %s\n",
	       __FUNCTION__, __LINE__,
	       is_ftm_mode()?"enable":"disable");
	if (is_ftm_mode()) {
		//product_id = PRODUCT_ID_DIAG;
	}
	return 0;
}
static ssize_t msm_hsusb_set_pidnv(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t size)
{
	int value;
	sscanf(buf, "%d", &value);
	set_nv(value);
	return size;
}

static ssize_t msm_hsusb_show_pidnv(struct device *dev,
                                    struct device_attribute *attr,
                                    char *buf)
{
	int i = 0;
	i = scnprintf(buf, PAGE_SIZE, "nv %d\n", get_nv());

	return i;
}

static ssize_t msm_hsusb_show_ftm(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	int i = 0;
	i = scnprintf(buf, PAGE_SIZE, "%s\n", is_ftm_mode()?"enable":"disable");
	return i;
}

static ssize_t msm_hsusb_show_pid_from_nv(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	int i = 0;
	config_pid_from_nv();
	i = scnprintf(buf, PAGE_SIZE, "%s\n", is_pid_configed_from_nv()?"enable":"disable");
	return i;
}
struct usb_parameters {
	char p_mass_vendor_name[64];
	char p_manufacturer_name[64];
	char p_product_name[64];
	char p_serialnumber[64];
	char cdrom_enable_after_switch;
};
struct usb_parameters zte_usb_parameters = {
	.p_mass_vendor_name = {"ZTE"},
	.p_manufacturer_name = {"ZTE Incorporated"},
	.p_product_name	   = {"ZTE HSUSB Device"},
	.p_serialnumber = {"ZTE_andorid"},
	.cdrom_enable_after_switch = 0,
};
static int is_cdrom_enabled_after_switch(void)
{
	return zte_usb_parameters.cdrom_enable_after_switch;
}
static void enable_cdrom_after_switch(int enable)
{
	zte_usb_parameters.cdrom_enable_after_switch = !!enable;
}

	
#define android_parameter_attr(parameter)                                                \
static void set_android_usb_##parameter(char * param)                                    \
{                                                                                        \
	strncpy(zte_usb_parameters.p_##parameter,			\
		param,                                                                   \
		sizeof(zte_usb_parameters.p_##parameter));                               \
    zte_usb_parameters.p_##parameter[sizeof(zte_usb_parameters.p_##parameter) - 1] = 0;  \
	return ;                                                                         \
}                                                                                        \
static char* get_android_usb_##parameter(void)                                           \
{                                                                                        \
	return zte_usb_parameters.p_##parameter;                                         \
}                                                                                        \
static ssize_t  show_android_usb_##parameter(struct device *dev,                         \
				struct device_attribute *attr, char *buf)                \
{                                                                                        \
	return sprintf(buf, "%s\n", get_android_usb_##parameter());                      \
}                                                                                        \
static ssize_t store_android_usb_##parameter(struct device *dev,                         \
					       struct device_attribute *attr,            \
					       const char* buf, size_t size)             \
{                                                                                        \
	set_android_usb_##parameter((char*)buf);		                         \
        return size;                                                                     \
}                                                                                        \
static DEVICE_ATTR(parameter, S_IRUGO | S_IWUSR, show_android_usb_##parameter,           \
		   store_android_usb_##parameter);                                      

android_parameter_attr(mass_vendor_name);
android_parameter_attr(manufacturer_name);
android_parameter_attr(product_name);
android_parameter_attr(serialnumber);

//xingbeilei
char* get_mass_vendor_name(void)
{
	return get_android_usb_mass_vendor_name();
}
EXPORT_SYMBOL(get_mass_vendor_name);
//end

static int android_set_sn(const char *kmessage, struct kernel_param *kp)
{
        int len = strlen(kmessage);

        if (len >= MAX_STR_LEN) {
                pr_err("serial number string too long\n");
                return -ENOSPC;
        }

        strlcpy(serial_number, kmessage, MAX_STR_LEN);
        /* Chop out \n char as a result of echo */
        if (serial_number[len - 1] == '\n')
                serial_number[len - 1] = '\0';
	set_android_usb_serialnumber(serial_number);

        return 0;
}

static ssize_t msm_hsusb_show_exwork(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int i = 0;
	struct usb_ex_work *p = &global_usbwork;
	i = scnprintf(buf, PAGE_SIZE,
				"auto switch: %s\nlinux switch: %s\nswitch_pid: 0x%x\ncur_pid: 0x%x\nlinux_pid: 0x%x\n",
		      p->enable_switch?"enable":"disable",
		      p->enable_linux_switch?"enable":"disable",
		      p->switch_pid,
		      p->cur_pid,
		      p->linux_pid);
	
	return i;
}
static ssize_t msm_hsusb_store_enable_switch(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
     unsigned long enable;
      	if (!strict_strtoul(buf, 16, &enable)) {
		global_usbwork.enable_switch = enable;
	} 
	return size;
}


static ssize_t msm_hsusb_store_enable_linux_switch(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
     unsigned long enable;
      	if (!strict_strtoul(buf, 16, &enable)) {
		global_usbwork.enable_linux_switch = enable;
	} 
	return size;
}

static ssize_t msm_hsusb_store_cur_pid(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{     
    
       unsigned long value;
	if (!strict_strtoul(buf, 16, &value)) {
      		printk(KERN_ERR"usb: cur_pid value=0x%x\n",(unsigned int)value);
		if(android_validate_product_id((unsigned short)value))
			global_usbwork.cur_pid=(unsigned int) value;
	}
	return size;
}


static ssize_t msm_hsusb_store_switch_pid(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{     
     
       unsigned long value;
	if (!strict_strtoul(buf, 16, &value)) {
      		printk(KERN_ERR"usb: switch_pid value=0x%x\n",(unsigned int)value);
		if(android_validate_product_id((u16)value))
			global_usbwork.switch_pid= (unsigned int)value;
	}
	
	return size;
}

static ssize_t msm_hsusb_store_linux_pid(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{     
   
       unsigned long value;
	if (!strict_strtoul(buf, 16, &value)) {
      		printk(KERN_ERR"usb: linux_pid value=0x%x\n",(unsigned int)value);
		if(android_validate_product_id((u16)value))
			global_usbwork.linux_pid= (unsigned int)value;
	}

	return size;
}

static ssize_t msm_hsusb_show_enable_cdrom(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int i = 0;	
	i = scnprintf(buf, PAGE_SIZE, "is cdrom enabled after switch? [%s]\n",
		      is_cdrom_enabled_after_switch()?"enable":"disable");	
	return i;
}

static ssize_t msm_hsusb_store_enable_cdrom(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
       unsigned long cdrom_enable = 0;
      	if (!strict_strtoul(buf, 16, &cdrom_enable)) {
		enable_cdrom_after_switch((cdrom_enable == 0x5656)? 1 : 0);
		pr_info("%s: Requested enable_cdrom = %d 0x%lx\n",
			__func__, g_enable_cdrom, cdrom_enable);	
	} else {
		pr_info("%s: strict_strtoul conversion failed, %s\n", __func__, buf);
	}
	return size;
}

static DEVICE_ATTR(exwork, 0664,
		   msm_hsusb_show_exwork, NULL);
static DEVICE_ATTR(enable_switch, 0664,
		   NULL, msm_hsusb_store_enable_switch);
static DEVICE_ATTR(linux_switch, 0664,
		   NULL, msm_hsusb_store_enable_linux_switch);
static DEVICE_ATTR(cur_pid, 0664,
		   NULL, msm_hsusb_store_cur_pid);
static DEVICE_ATTR(switch_pid, 0664,
		   NULL, msm_hsusb_store_switch_pid);
static DEVICE_ATTR(linux_pid, 0664,
		   NULL, msm_hsusb_store_linux_pid);
static DEVICE_ATTR(enable_cdrom, 0664,
		   msm_hsusb_show_enable_cdrom, msm_hsusb_store_enable_cdrom);

//ruanmeisi_20110110
static DEVICE_ATTR(pidnv, 0664,
                   msm_hsusb_show_pidnv, msm_hsusb_set_pidnv);
static DEVICE_ATTR(ftm_mode, 0664,
                   msm_hsusb_show_ftm, NULL);
static DEVICE_ATTR(pid_from_nv, 0664,
                   msm_hsusb_show_pid_from_nv, NULL);
static struct attribute *android_attrs[] = {
	&dev_attr_pidnv.attr,
	&dev_attr_ftm_mode.attr,
	&dev_attr_pid_from_nv.attr,
        &dev_attr_manufacturer_name.attr,
        &dev_attr_product_name.attr,
        &dev_attr_mass_vendor_name.attr,
        &dev_attr_serialnumber.attr,
        &dev_attr_enable_cdrom.attr,
    //for cdrom and auto install driver
	&dev_attr_exwork.attr,
	&dev_attr_cur_pid.attr,
	&dev_attr_switch_pid.attr,
	&dev_attr_linux_pid.attr,
	&dev_attr_linux_switch.attr,
	&dev_attr_enable_switch.attr,
	NULL,
};


static struct attribute_group android_attr_grp = {
	.name = "functions",
	.attrs = android_attrs,
};
//xingbeilei 20110829 end


static void android_set_default_product(int pid);

void android_usb_set_connected(int connected)
{
	if (_android_dev && _android_dev->cdev && _android_dev->cdev->gadget) {
		if (connected)
			usb_gadget_connect(_android_dev->cdev->gadget);
		else
			usb_gadget_disconnect(_android_dev->cdev->gadget);
	}
}

static struct android_usb_function *get_function(const char *name)
{
	struct android_usb_function	*f;
	list_for_each_entry(f, &_functions, list) {
		if (!strcmp(name, f->name))
			return f;
	}
	return 0;
}

static bool are_functions_registered(struct android_dev *dev)
{
	char **functions = dev->functions;
	int i;

	/* Look only for functions required by the board config */
	for (i = 0; i < dev->num_functions; i++) {
		char *name = *functions++;
		bool is_match = false;
		/* Could reuse get_function() here, but a reverse search
		 * should yield less comparisons overall */
		struct android_usb_function *f;
		list_for_each_entry_reverse(f, &_functions, list) {
			if (!strcmp(name, f->name)) {
				is_match = true;
				break;
			}
		}
		if (is_match)
			continue;
		else
			return false;
	}

	return true;
}

static bool should_bind_functions(struct android_dev *dev)
{
	/* Don't waste time if the main driver hasn't bound */
	if (!dev->config)
		return false;

	/* Don't waste time if we've already bound the functions */
	if (_are_functions_bound)
		return false;

	/* This call is the most costly, so call it last */
	if (!are_functions_registered(dev))
		return false;

	return true;
}

static void bind_functions(struct android_dev *dev)
{
	struct android_usb_function	*f;
	char **functions = dev->functions;
	int i;

	for (i = 0; i < dev->num_functions; i++) {
		char *name = *functions++;
		f = get_function(name);
		if (f)
			f->bind_config(dev->config);
		else
			pr_err("%s: function %s not found\n", __func__, name);
	}

	_are_functions_bound = true;
	/*
	 * set_alt(), or next config->bind(), sets up
	 * ep->driver_data as needed.
	 */
	usb_ep_autoconfig_reset(dev->cdev->gadget);
}

static int __ref android_bind_config(struct usb_configuration *c)
{
	struct android_dev *dev = _android_dev;

	pr_debug("android_bind_config\n");
	dev->config = c;

	if (should_bind_functions(dev)) {
		bind_functions(dev);
		printk(KERN_ERR"usb_xbl,%s,%d,product_id=0x%d\n",__FUNCTION__,__LINE__,dev->product_id);
		android_set_default_product(dev->product_id);
	} else {
		/* Defer enumeration until all functions are bounded */
		if (c->cdev && c->cdev->gadget)
			usb_gadget_disconnect(c->cdev->gadget);
	}

	return 0;
}

static int android_setup_config(struct usb_configuration *c,
		const struct usb_ctrlrequest *ctrl);

static struct usb_configuration android_config_driver = {
	.label		= "android",
	.setup		= android_setup_config,
	.bConfigurationValue = 1,
	.bmAttributes	= USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower	= 0xFA, /* 500ma */
};

static int android_setup_config(struct usb_configuration *c,
		const struct usb_ctrlrequest *ctrl)
{
	int i;
	int ret = -EOPNOTSUPP;

	for (i = 0; i < android_config_driver.next_interface_id; i++) {
		if (android_config_driver.interface[i]->setup) {
			ret = android_config_driver.interface[i]->setup(
				android_config_driver.interface[i], ctrl);
			if (ret >= 0)
				return ret;
		}
	}
	return ret;
}

static int product_has_function(struct android_usb_product *p,
		struct usb_function *f)
{
	char **functions = p->functions;
	int count = p->num_functions;
	const char *name = f->name;
	int i;

	for (i = 0; i < count; i++) {
		/* For functions with multiple instances, usb_function.name
		 * will have an index appended to the core name (ex: acm0),
		 * while android_usb_product.functions[i] will only have the
		 * core name (ex: acm). So, only compare up to the length of
		 * android_usb_product.functions[i].
		 */
		if (!strncmp(name, functions[i], strlen(functions[i])))
			return 1;
	}
	return 0;
}

static int product_matches_functions(struct android_usb_product *p)
{
	struct usb_function		*f;
	list_for_each_entry(f, &android_config_driver.functions, list) {
		if (product_has_function(p, f) == !!f->disabled)
			return 0;
	}
	return 1;
}

static int get_product_id(struct android_dev *dev)
{
	struct android_usb_product *p = dev->products;
	int count = dev->num_products;
	int i;

	if (p) {
		for (i = 0; i < count; i++, p++) {
			if (product_matches_functions(p))
				return p->product_id;
		}
	}
	/* use default product ID */
	return dev->product_id;
}

static int is_msc_only_comp(int pid)
{
	struct android_dev *dev = _android_dev;
	struct android_usb_product *up = dev->products;
	int count;
	char **functions;
	int index;

	for (index = 0; index < dev->num_products; index++, up++) {
		if (pid == up->product_id)
			break;
	}

	count = up->num_functions;
	functions = up->functions;

	if (count == 1 && !strncmp(*functions, "usb_mass_storage", 32))
		return true;
	else
		return false;
}

//xingbeilei
static int is_function_enable(char *func)
{
	struct android_dev *dev = _android_dev;
	int i = 0;
	char** functions = NULL;
	int num_functions = 0;
	if (NULL == func || NULL == dev || NULL == dev->bind_functions) {
		return 0;
	}

	functions = dev->bind_functions;
	num_functions = dev->bind_num_functions;
	
	for(i = 0; i < num_functions; i++) {
		char* name = *functions++;
		if (!strncmp(func, name, strlen(name))) {
			return 1;
		}
	}
	return 0;
}

int is_iad_enabled(void) {
	if(is_function_enable("rndis"))
		return 1;
	return 0;
}
//end

static int __devinit android_bind(struct usb_composite_dev *cdev)
{
	struct android_dev *dev = _android_dev;
	struct usb_gadget	*gadget = cdev->gadget;
	int			gcnum, id, product_id, ret;

	pr_debug("android_bind\n");

	/* Allocate string descriptor numbers ... note that string
	 * contents can be overridden by the composite_dev glue.
	 */
	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_MANUFACTURER_IDX].id = id;
	device_desc.iManufacturer = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_PRODUCT_IDX].id = id;
	device_desc.iProduct = id;

	if (!is_ftm_mode() && !is_pid_configed_from_nv()) {
	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_SERIAL_IDX].id = id;
	device_desc.iSerialNumber = id;
	} else {
		strings_dev[STRING_SERIAL_IDX].id = 0;
		device_desc.iSerialNumber = 0;
	}


	if (gadget_is_otg(cdev->gadget))
		android_config_driver.descriptors = otg_desc;

	if (!usb_gadget_set_selfpowered(gadget))
		android_config_driver.bmAttributes |= USB_CONFIG_ATT_SELFPOWER;
	/*
	 * Supporting remote wakeup for mass storage only function
	 * doesn't make sense, since there is no notifications that
	 * that can be sent from mass storage during suspend.
	 */
	if (gadget->ops->wakeup && !is_msc_only_comp((dev->product_id)))
		android_config_driver.bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	else
		android_config_driver.bmAttributes &= ~USB_CONFIG_ATT_WAKEUP;


	/* register our configuration */
	ret = usb_add_config(cdev, &android_config_driver, android_bind_config);
	if (ret) {
		pr_err("%s: usb_add_config failed\n", __func__);
		return ret;
	}

	gcnum = usb_gadget_controller_number(gadget);
	if (gcnum >= 0)
		device_desc.bcdDevice = cpu_to_le16(0x0200 + gcnum);
	else {
		/* gadget zero is so simple (for now, no altsettings) that
		 * it SHOULD NOT have problems with bulk-capable hardware.
		 * so just warn about unrcognized controllers -- don't panic.
		 *
		 * things like configuration and altsetting numbering
		 * can need hardware-specific attention though.
		 */
		pr_warning("%s: controller '%s' not recognized\n",
			longname, gadget->name);
		device_desc.bcdDevice = __constant_cpu_to_le16(0x9999);
	}

	usb_gadget_set_selfpowered(gadget);
	dev->cdev = cdev;
	product_id = get_product_id(dev);
	device_desc.idProduct = __constant_cpu_to_le16(product_id);
	cdev->desc.idProduct = device_desc.idProduct;
	
	return 0;
}

static struct usb_composite_driver android_usb_driver = {
	.name		= "android_usb",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.enable_function = android_enable_function,
};

static bool is_func_supported(struct android_usb_function *f)
{
	char **functions = _android_dev->functions;
	int count = _android_dev->num_functions;
	const char *name = f->name;
	int i;

	for (i = 0; i < count; i++) {
		if (!strcmp(*functions++, name))
			return true;
	}
	return false;
}

void android_register_function(struct android_usb_function *f)
{
	struct android_dev *dev = _android_dev;

	pr_debug("%s: %s\n", __func__, f->name);

	if (!is_func_supported(f))
		return;

	list_add_tail(&f->list, &_functions);

	if (dev && should_bind_functions(dev)) {
		bind_functions(dev);
		printk(KERN_ERR"usb_xbl:%s,product_id=0x%d\n",__FUNCTION__,dev->product_id);
		android_set_default_product(dev->product_id);
		/* All function are bounded. Enable enumeration */
		if (dev->cdev && dev->cdev->gadget)
			usb_gadget_connect(dev->cdev->gadget);
	}

}

/**
 * android_set_function_mask() - enables functions based on selected pid.
 * @up: selected product id pointer
 *
 * This function enables functions related with selected product id.
 */
static void android_set_function_mask(struct android_usb_product *up)
{
	int index, found = 0;
	struct usb_function *func;

	printk(KERN_ERR"usb_xbl: %s, %d\n", __FUNCTION__, __LINE__);
	list_for_each_entry(func, &android_config_driver.functions, list) {
		/* adb function enable/disable handled separetely */
		if (!strcmp(func->name, "adb") && !func->disabled)
			continue;

		for (index = 0; index < up->num_functions; index++) {
			if (!strcmp(up->functions[index], func->name)) {
				found = 1;
				break;
			}
		}

		if (found) { /* func is part of product. */
			/* if func is disabled, enable the same. */
			if (func->disabled)
				usb_function_set_enabled(func, 1);
			found = 0;
		} else { /* func is not part if product. */
			/* if func is enabled, disable the same. */
			if (!func->disabled)
				usb_function_set_enabled(func, 0);
		}
	}
}

/**
 * android_set_defaut_product() - selects default product id and enables
 * required functions
 * @product_id: default product id
 *
 * This function selects default product id using pdata information and
 * enables functions for same.
*/
static void android_set_default_product(int pid)
{
	struct android_dev *dev = _android_dev;
	struct android_usb_product *up = dev->products;
	int index;

	printk(KERN_ERR"usb_xbl: %s, %d\n", __FUNCTION__, __LINE__);
	for (index = 0; index < dev->num_products; index++, up++) {
		if (pid == up->product_id)
			break;
	}
	android_set_function_mask(up);
	device_desc.idProduct = __constant_cpu_to_le16(pid);
	if (dev->cdev)
		dev->cdev->desc.idProduct = device_desc.idProduct;
}

/**
 * android_config_functions() - selects product id based on function need
 * to be enabled / disabled.
 * @f: usb function
 * @enable : function needs to be enable or disable
 *
 * This function selects first product id having required function.
 * RNDIS/MTP function enable/disable uses this.
*/
#ifdef CONFIG_USB_ANDROID_RNDIS
static void android_config_functions(struct usb_function *f, int enable)
{
	struct android_dev *dev = _android_dev;
	struct android_usb_product *up = dev->products;
	int index;

	/* Searches for product id having function */
	if (enable) {
		for (index = 0; index < dev->num_products; index++, up++) {
			if (product_has_function(up, f))
				break;
		}
		android_set_function_mask(up);
	} else
		android_set_default_product(dev->product_id);
}
#endif

void update_dev_desc(struct android_dev *dev)
{
	struct usb_function *f;
	struct usb_function *last_enabled_f = NULL;
	int num_enabled = 0;
	int has_iad = 0;

	dev->cdev->desc.bDeviceClass = USB_CLASS_PER_INTERFACE;
	dev->cdev->desc.bDeviceSubClass = 0x00;
	dev->cdev->desc.bDeviceProtocol = 0x00;

	list_for_each_entry(f, &android_config_driver.functions, list) {
		if (!f->disabled) {
			num_enabled++;
			last_enabled_f = f;
			if (f->descriptors[0]->bDescriptorType ==
					USB_DT_INTERFACE_ASSOCIATION)
				has_iad = 1;
		}
		if (num_enabled > 1 && has_iad) {
			dev->cdev->desc.bDeviceClass = USB_CLASS_MISC;
			dev->cdev->desc.bDeviceSubClass = 0x02;
			dev->cdev->desc.bDeviceProtocol = 0x01;
			break;
		}
	}

	if (num_enabled == 1) {
#ifdef CONFIG_USB_ANDROID_RNDIS
		if (!strcmp(last_enabled_f->name, "rndis")) {
#ifdef CONFIG_USB_ANDROID_RNDIS_WCEIS
			dev->cdev->desc.bDeviceClass =
					USB_CLASS_WIRELESS_CONTROLLER;
#else
			dev->cdev->desc.bDeviceClass = USB_CLASS_COMM;
#endif
		}
#endif
	}
}


static char *sysfs_allowed[] = {
	"rndis",
	"mtp",
	"adb",
};

static int is_sysfschange_allowed(struct usb_function *f)
{
	char **functions = sysfs_allowed;
	int count = ARRAY_SIZE(sysfs_allowed);
	int i;

	for (i = 0; i < count; i++) {
		if (!strncmp(f->name, functions[i], 32))
			return 1;
	}
	return 0;
}

int android_enable_function(struct usb_function *f, int enable)
{
	struct android_dev *dev = _android_dev;
	int disable = !enable;
	struct usb_gadget	*gadget = dev->cdev->gadget;
	int product_id;

	pr_info_ratelimited("%s: %s %s\n",
		__func__, enable ? "enable" : "disable", f->name);

	if (!is_sysfschange_allowed(f))
		return -EINVAL;
	if (!!f->disabled != disable) {
		usb_function_set_enabled(f, !disable);

#ifdef CONFIG_USB_ANDROID_RNDIS
		if (!strcmp(f->name, "rndis")) {

			/* We need to specify the COMM class in the device descriptor
			 * if we are using RNDIS.
			 */
			if (enable) {
#ifdef CONFIG_USB_ANDROID_RNDIS_WCEIS
				dev->cdev->desc.bDeviceClass = USB_CLASS_MISC;
				dev->cdev->desc.bDeviceSubClass      = 0x02;
				dev->cdev->desc.bDeviceProtocol      = 0x01;
#else
				dev->cdev->desc.bDeviceClass = USB_CLASS_COMM;
#endif
			} else {
				dev->cdev->desc.bDeviceClass = USB_CLASS_PER_INTERFACE;
				dev->cdev->desc.bDeviceSubClass      = 0;
				dev->cdev->desc.bDeviceProtocol      = 0;
			}

			android_config_functions(f, enable);
		}
#endif

#ifdef CONFIG_USB_ANDROID_MTP
		if (!strcmp(f->name, "mtp"))
			android_config_functions(f, enable);
#endif

		product_id = get_product_id(dev);

		if (gadget && gadget->ops->wakeup &&
				!is_msc_only_comp((product_id)))
			android_config_driver.bmAttributes |=
				USB_CONFIG_ATT_WAKEUP;
		else
			android_config_driver.bmAttributes &=
				~USB_CONFIG_ATT_WAKEUP;

		device_desc.idProduct = __constant_cpu_to_le16(product_id);
		if (dev->cdev)
			dev->cdev->desc.idProduct = device_desc.idProduct;
		usb_composite_force_reset(dev->cdev);
	}
	return 0;
}

/*xingbeilei-20110825-begin*/
struct android_usb_product *android_validate_product_id(unsigned short pid)
{
        struct android_dev *dev = _android_dev;
        struct android_usb_product *up;
        int i;

	printk(KERN_ERR"usb_xbl: %s, %d 0x%x\n", __FUNCTION__, __LINE__, pid);
        for (i = 0; i < dev->num_products; i++) {
                up = &dev->products[i];
                if (up->product_id == pid || up->adb_product_id == pid)
                        return up;
        }
	printk(KERN_ERR"usb_xbl: %s, %d invalid pid(0x%x)\n", __FUNCTION__, __LINE__, pid);
        return NULL;
}

int android_set_bind_functions(u16 pid)
{
        struct android_usb_product *up = NULL;
        struct android_dev *dev = _android_dev;

        if (NULL == dev) {
                pr_err("usb:%s: err %x\n", __func__, pid);
                return -EINVAL;
        }
        up = android_validate_product_id(pid);

        if (!up) {
                pr_err("usb:%s: invalid product id %x\n", __func__, pid);
                return -EINVAL;
        }

        /* DisHonour adb users */

	printk(KERN_ERR"xbl:is_adb_enabled = %d",is_adb_enabled());
        if (is_adb_enabled() && up->adb_product_id) {
                dev->bind_functions = up->adb_functions;
                dev->bind_num_functions = up->adb_num_functions;
                dev->product_id = up->adb_product_id;
        } else {
                dev->bind_functions = up->functions;
                dev->bind_num_functions = up->num_functions;
                dev->product_id = up->product_id;
        }

        device_desc.idProduct = __constant_cpu_to_le16(dev->product_id);
        if (dev->cdev)
		dev->cdev->desc.idProduct = device_desc.idProduct;

        pr_err("%s: product id = %x num functions = %x curr pid 0x%x\n",
                __func__,
                up->product_id,
                up->num_functions,
                dev->product_id);
        return 0;
}

static void android_enable_multi_functions(struct android_dev *dev)
{
	int index, found = 0;
	struct usb_function *func;

	printk(KERN_ERR"usb_xbl: %s, %d\n", __FUNCTION__, __LINE__);
	list_for_each_entry(func, &android_config_driver.functions, list) {

		for (index = 0; index < dev->bind_num_functions; index++) {
			if (!strcmp(dev->bind_functions[index], func->name)) {
				found = 1;
				break;
			}
		}

		if (found) { /* func is part of bind_functions. */
			/* if func is disabled, enable the same. */
			if (func->disabled)
				usb_function_set_enabled(func, 1);
			found = 0;
		} else { /* func is not part of bind_functions. */
			/* if func is enabled, disable the same. */
			if (!func->disabled)
				usb_function_set_enabled(func, 0);
		}
	}
}

static int rndis_mute_switch = 0;
int get_mute_switch(void)
{
	return rndis_mute_switch ;
}
EXPORT_SYMBOL(get_mute_switch);
void set_mute_switch(int mute)
{
	rndis_mute_switch = mute;
	return ;
}
EXPORT_SYMBOL(set_mute_switch);

static int android_switch_composition(u16 pid)
{
        int ret;
	struct android_dev *dev = _android_dev;

        ret = android_set_bind_functions(pid);
        if (ret < 0) {
                pr_err("%s: %d: err android_set_bind_functions \n", __func__, __LINE__);
                return ret;
        }

	printk(KERN_ERR"usb_xbl: %s, %d\n", __FUNCTION__, __LINE__);
	android_enable_multi_functions(dev);

	if (is_iad_enabled()) {
		set_mute_switch(1);
#ifdef CONFIG_USB_ANDROID_RNDIS_WCEIS
		dev->cdev->desc.bDeviceClass = USB_CLASS_MISC;
		dev->cdev->desc.bDeviceSubClass      = 0x02;
		dev->cdev->desc.bDeviceProtocol      = 0x01;
		if (!support_assoc_desc()) {
                        device_desc.bDeviceClass         =  USB_CLASS_WIRELESS_CONTROLLER;
                        device_desc.bDeviceSubClass      = 0;
                        device_desc.bDeviceProtocol      = 0;
                }
#else
		dev->cdev->desc.bDeviceClass = USB_CLASS_COMM;
#endif
		} else {
		dev->cdev->desc.bDeviceClass = USB_CLASS_PER_INTERFACE;
		dev->cdev->desc.bDeviceSubClass      = 0;
		dev->cdev->desc.bDeviceProtocol      = 0;
		}
        if (is_pid_configed_from_nv()) {
                strings_dev[STRING_SERIAL_IDX].id = 0;
                device_desc.iSerialNumber = 0;             
		if (dev->cdev)
			dev->cdev->desc.iSerialNumber = device_desc.iSerialNumber;
        }
	printk(KERN_ERR"usb_xbl: %s, %d,iSerialNumber = %d\n",
		       __FUNCTION__, __LINE__,device_desc.iSerialNumber);
	usb_composite_force_reset(dev->cdev);

	return 0;
}

static int android_set_pid(const char *val, struct kernel_param *kp)
{
	int ret = 0;
	unsigned long tmp;

	pr_err("got val = %s\n", val);
	printk(KERN_ERR"usb_xbl: %s, %d\n", __FUNCTION__, __LINE__);	
	ret = strict_strtoul(val, 16, &tmp);
	if (ret) {
		pr_err("strict_strtoul failed\n");
		goto out;
	}
	pr_err("pid is 0x%x\n", (unsigned int)tmp);
	/* We come here even before android_probe, when product id
	 * is passed via kernel command line.
	 */
	/* dbg_err("verifying _android_dev\n"); */
	if (!_android_dev) {
		global_product_id = tmp;
		goto out;
	}

	printk(KERN_ERR"usb_xbl: %s, %d\n", __FUNCTION__, __LINE__);	
	mutex_lock(&_android_dev->lock);
	ret = android_switch_composition(tmp);
	mutex_unlock(&_android_dev->lock);
	//ruanmeisi_20100713 for auto swith usb mode
	clear_switch_flag(); 
	//end
out:
	return strlen(val);
}

static int android_get_pid(char *buffer, struct kernel_param *kp)
{
	struct android_dev *dev = _android_dev;
	int ret;

	mutex_lock(&_android_dev->lock);
	ret = scnprintf(buffer, PAGE_SIZE, "%x", get_product_id(dev));
	mutex_unlock(&_android_dev->lock);

	return ret;
}
/*xingbeilei-20110825-end*/

#ifdef CONFIG_DEBUG_FS
static int android_debugfs_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t android_debugfs_serialno_write(struct file *file, const char
				__user *buf,	size_t count, loff_t *ppos)
{
	char str_buf[MAX_STR_LEN];

	if (count > MAX_STR_LEN)
		return -EFAULT;

	if (copy_from_user(str_buf, buf, count))
		return -EFAULT;

	memcpy(serial_number, str_buf, count);

	if (serial_number[count - 1] == '\n')
		serial_number[count - 1] = '\0';

	strings_dev[STRING_SERIAL_IDX].s = serial_number;

	return count;
}
const struct file_operations android_fops = {
	.open	= android_debugfs_open,
	.write	= android_debugfs_serialno_write,
};

struct dentry *android_debug_root;
struct dentry *android_debug_serialno;

static int android_debugfs_init(struct android_dev *dev)
{
	android_debug_root = debugfs_create_dir("android", NULL);
	if (!android_debug_root)
		return -ENOENT;

	android_debug_serialno = debugfs_create_file("serial_number", 0220,
						android_debug_root, dev,
						&android_fops);
	if (!android_debug_serialno) {
		debugfs_remove(android_debug_root);
		android_debug_root = NULL;
		return -ENOENT;
	}
	return 0;
}

static void android_debugfs_cleanup(void)
{
       debugfs_remove(android_debug_serialno);
       debugfs_remove(android_debug_root);
}
#endif

static int __devinit android_probe(struct platform_device *pdev)
{
	struct android_usb_platform_data *pdata = pdev->dev.platform_data;
	struct android_dev *dev = _android_dev;
	int result;

	dev_dbg(&pdev->dev, "%s: pdata: %p\n", __func__, pdata);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	result = pm_runtime_get(&pdev->dev);
	if (result < 0) {
		dev_err(&pdev->dev,
			"Runtime PM: Unable to wake up the device, rc = %d\n",
			result);
		return result;
	}

	if (pdata) {
		dev->products = pdata->products;
		dev->num_products = pdata->num_products;
		dev->functions = pdata->functions;
		dev->num_functions = pdata->num_functions;
		config_ftm_from_tag();

		if (pdata->vendor_id)
			device_desc.idVendor =
				__constant_cpu_to_le16(pdata->vendor_id);
		printk(KERN_ERR"usb_xbl,pdata->product_id 0x%d\n", pdata->product_id);
		if (pdata->product_id) {
			dev->product_id = pdata->product_id;
			device_desc.idProduct =
				__constant_cpu_to_le16(pdata->product_id);
		}
		if (is_ftm_mode()) {
			dev->product_id = PRODUCT_ID_DIAG;
			printk(KERN_ERR"dev->product_id 0x%x\n", dev->product_id);
			device_desc.idProduct =
				__constant_cpu_to_le16(PRODUCT_ID_DIAG);
		}
		if (pdata->version)
			dev->version = pdata->version;

		//xingbeilei begin
		/*if (pdata->product_name)
			strings_dev[STRING_PRODUCT_IDX].s = pdata->product_name;
		if (pdata->manufacturer_name)
			strings_dev[STRING_MANUFACTURER_IDX].s =
					pdata->manufacturer_name;
		if (pdata->serial_number)
		strings_dev[STRING_SERIAL_IDX].s = pdata->serial_number;*/
		if (pdata->product_name) {
			set_android_usb_product_name(pdata->product_name);
		}
		strings_dev[STRING_PRODUCT_IDX].s =
			get_android_usb_product_name();
		if (pdata->manufacturer_name) {
			set_android_usb_manufacturer_name(pdata->manufacturer_name);
		}
		strings_dev[STRING_MANUFACTURER_IDX].s =
			get_android_usb_manufacturer_name();
		if (pdata->serial_number) {
			set_android_usb_serialnumber(pdata->serial_number);
		}
		strings_dev[STRING_SERIAL_IDX].s =
			get_android_usb_serialnumber();
		//xingbeilei end
	}
#ifdef CONFIG_DEBUG_FS
	result = android_debugfs_init(dev);
	if (result)
		pr_debug("%s: android_debugfs_init failed\n", __func__);
#endif
//xingbeilei
	result = sysfs_create_group(&pdev->dev.kobj, &android_attr_grp);
        if (result < 0) {
		pr_err("%s: Failed to create the sysfs entry \n", __func__);
		return result;
        }
//end
	return usb_composite_probe(&android_usb_driver, android_bind);
}

static int andr_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int andr_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static struct dev_pm_ops andr_dev_pm_ops = {
	.runtime_suspend = andr_runtime_suspend,
	.runtime_resume = andr_runtime_resume,
};

static struct platform_driver android_platform_driver = {
	.driver = { .name = "android_usb", .pm = &andr_dev_pm_ops},
};

static int adb_enable_open(struct inode *ip, struct file *fp)
{
	if (atomic_inc_return(&adb_enable_excl) != 1) {
		atomic_dec(&adb_enable_excl);
		return -EBUSY;
	}

	printk(KERN_ERR "usb %s: Enabling adb\n", __func__);
	//android_enable_function(&_adb_dev->function, 1);
	if (_android_dev && _android_dev->product_id) {
		/*do not enable adb when pid is 0x0112 on ftm*/
		if(is_ftm_mode()&&(_android_dev->product_id == 0x0112))
			return 0;
		mutex_lock(&_android_dev->lock);
		android_switch_composition(_android_dev->product_id);
		mutex_unlock(&_android_dev->lock);
	}
	return 0;
}

static int adb_enable_release(struct inode *ip, struct file *fp)
{
	pr_debug("%s: Disabling adb\n", __func__);
	//android_enable_function(&_adb_dev->function, 0);
	atomic_dec(&adb_enable_excl);
	if (_android_dev && _android_dev->product_id) {
		mutex_lock(&_android_dev->lock);
		android_switch_composition(_android_dev->product_id);
		mutex_unlock(&_android_dev->lock);
	}
	return 0;
}

static const struct file_operations adb_enable_fops = {
	.owner =   THIS_MODULE,
	.open =    adb_enable_open,
	.release = adb_enable_release,
};

static struct miscdevice adb_enable_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "android_adb_enable",
	.fops = &adb_enable_fops,
};

static int __init init(void)
{
	struct android_dev *dev;
	int ret;

	pr_debug("android init\n");

	//xingbeilei begin
        ret = misc_register(&adb_enable_device);
        if (ret)
                return ret;
	//xingbeilei end

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	create_usb_work_queue();
	/* set default values, which should be overridden by platform data */
	dev->product_id = PRODUCT_ID;
	_android_dev = dev;
	mutex_init(&dev->lock);

	return platform_driver_probe(&android_platform_driver, android_probe);
}
module_init(init);

static void __exit cleanup(void)
{
#ifdef CONFIG_DEBUG_FS
	android_debugfs_cleanup();
#endif
	usb_composite_unregister(&android_usb_driver);
	platform_driver_unregister(&android_platform_driver);
	kfree(_android_dev);
	_android_dev = NULL;
	destroy_usb_work_queue();
	misc_deregister(&adb_enable_device);
}
module_exit(cleanup);


//ruanmeisi_20100712 add for switch usb mode
static int create_usb_work_queue(void)
{
	struct usb_ex_work *p = &global_usbwork;
	if (p->workqueue) {
		printk(KERN_ERR"usb:workqueue has created");
		return 0;
	}
	spin_lock_init(&p->lock);
	p->enable_switch = 1;
	p->enable_linux_switch = 0;
	p->switch_pid = PRODUCT_ID_MS_ADB;
	p->linux_pid = PRODUCT_ID_MS_ADB;
	p->cur_pid = PRODUCT_ID_MS_CDROM;
	p->has_switch = 0;
	p->workqueue = create_singlethread_workqueue("usb_workqueue");
	if (NULL == p->workqueue) {
		printk(KERN_ERR"usb:workqueue created fail");
		p->enable_switch = 0;
		return -1;
	}
	INIT_DELAYED_WORK(&p->switch_work, usb_switch_work);
	INIT_DELAYED_WORK(&p->linux_switch_work, usb_switch_os_work);
	INIT_DELAYED_WORK(&p->plug_work, usb_plug_work);
	wake_lock_init(&p->wlock,
		       WAKE_LOCK_SUSPEND, "usb_switch_wlock");
	return 0;
}



static int destroy_usb_work_queue(void)
{
	struct usb_ex_work *p = &global_usbwork;
	if (NULL != p->workqueue) {
		destroy_workqueue(p->workqueue);
		p->workqueue = NULL;
	}
	wake_lock_destroy(&p->wlock);
	memset(&global_usbwork, 0, sizeof(global_usbwork));
	return 0;
}

static void usb_plug_work(struct work_struct *w)
{
	unsigned long flags;
	int pid = 0;
	struct usb_ex_work *p = container_of(w, struct usb_ex_work, plug_work.work);

	if (!_android_dev) {

		printk(KERN_ERR"usb:%s: %d: _android_dev == NULL\n",
		       __FUNCTION__, __LINE__);
		return ;
	}
	
	spin_lock_irqsave(&p->lock, flags);
	if (!p->has_switch) {
		printk("usb:rms: %s %d: \n", __FUNCTION__, __LINE__);
		spin_unlock_irqrestore(&p->lock, flags);
		return ;
	}
	printk("usb:rms: %s %d: \n", __FUNCTION__, __LINE__);
	p->has_switch = 0;
	pid = p->cur_pid;
	spin_unlock_irqrestore(&p->lock, flags);
	//enable_cdrom(1);
	//DBG("plug work");
	printk("usb:rms %s:%d pid 0x%x cur_pid 0x%x\n",
	       __FUNCTION__, __LINE__, current_pid(), pid);

	mutex_lock(&_android_dev->lock);
	wake_lock(&p->wlock);
	android_switch_composition((unsigned short)pid);
	wake_unlock(&p->wlock);
	mutex_unlock(&_android_dev->lock);

	return ;
}

static void usb_switch_work(struct work_struct *w)
{
	struct usb_ex_work *p = container_of(w, struct usb_ex_work, switch_work.work);
	unsigned long flags;
	if (!_android_dev) {

		printk(KERN_ERR"usb:%s: %d: _android_dev == NULL\n",
		       __FUNCTION__, __LINE__);
		return ;
	}
	if (!p->enable_switch) {
		return ;
	}
	if (p->has_switch) {
		printk("usb:rms:%s %d: already switch pid 0x%x switch_pid 0x%x\n",
		       __FUNCTION__, __LINE__, current_pid(), p->switch_pid);
		return ;
	}
	spin_lock_irqsave(&p->lock, flags);
//	p->cur_pid = ui->composition->product_id;
	p->has_switch = 1;
	spin_unlock_irqrestore(&p->lock, flags);
//	DBG("auto switch usb mode");
	printk("usb:rms:%s %d: pid 0x%x switch_pid 0x%x\n",
	       __FUNCTION__, __LINE__, current_pid(), p->switch_pid);
	//enable_cdrom(0);

	mutex_lock(&_android_dev->lock);
	wake_lock(&p->wlock);
	android_switch_composition((unsigned short)p->switch_pid);
	wake_unlock(&p->wlock);
	mutex_unlock(&_android_dev->lock);

	return ;
}

static void usb_switch_os_work(struct work_struct *w)
{
	struct usb_ex_work *p =
		container_of(w, struct usb_ex_work, linux_switch_work.work);
	unsigned long flags;

	if (!_android_dev) {

		printk(KERN_ERR"usb:%s: %d: _android_dev == NULL\n",
		       __FUNCTION__, __LINE__);
		return ;
	}

	if (!p->enable_switch || !p->enable_linux_switch || p->has_switch) {
		//switch  or linux_switch are enable, or we has already switch,return direct
		printk("usb:rms:%s:%d, switch %s: linux switch %s: %s switch\n",
		       __FUNCTION__, __LINE__, p->enable_switch?"enable":"disable",
		       p->enable_linux_switch?"enable":"disable",
		       p->has_switch?"has":"has not");
		return ;
	}
	spin_lock_irqsave(&p->lock, flags);
//	p->cur_pid = ui->composition->product_id;
	p->has_switch = 1;
	spin_unlock_irqrestore(&p->lock, flags);
	printk("usb:rms:%s %d: pid 0x%x linux_pid 0x%x\n",
	       __FUNCTION__, __LINE__, current_pid(), p->linux_pid);

	mutex_lock(&_android_dev->lock);
	wake_lock(&p->wlock);
	android_switch_composition((unsigned short)p->linux_pid);
	wake_unlock(&p->wlock);
	mutex_unlock(&_android_dev->lock);

	return ;
}

void schedule_cdrom_stop(void)
{
	
	if (NULL == global_usbwork.workqueue) {
		return ;
	}
	queue_delayed_work(global_usbwork.workqueue, &global_usbwork.switch_work, HZ/10);

	return;
}
EXPORT_SYMBOL(schedule_cdrom_stop);
void schedule_linux_os(void)
{
	if (NULL == global_usbwork.workqueue) {
		return ;
	}
	queue_delayed_work(global_usbwork.workqueue,
			   &global_usbwork.linux_switch_work, 0);

	return;
}
EXPORT_SYMBOL(schedule_linux_os);

void schedule_usb_plug(void)
{
	
	if (NULL == global_usbwork.workqueue) {
		return ;
	}
	printk("usb:rms: %s %d: \n", __FUNCTION__, __LINE__);
	queue_delayed_work(global_usbwork.workqueue, &global_usbwork.plug_work, 0);

	return ;
}
EXPORT_SYMBOL(schedule_usb_plug);

static void clear_switch_flag(void)
{
	unsigned long flags;
	struct usb_ex_work *p = &global_usbwork;
	spin_lock_irqsave(&p->lock, flags);
	p->has_switch = 0;
	spin_unlock_irqrestore(&p->lock, flags);

	return ;
}


static int usb_cdrom_is_enable(void)
{
	return (PRODUCT_ID_MS_CDROM == current_pid()) ? 1:0;
}


int os_switch_is_enable(void)
{
	struct usb_ex_work *p = &global_usbwork;
	
	return usb_cdrom_is_enable()?(p->enable_linux_switch) : 0;
}
EXPORT_SYMBOL(os_switch_is_enable);

int get_nuluns(void)
{
        if (usb_cdrom_is_enable() || is_cdrom_enabled_after_switch()) {
                return 2;
        }
        return 1;
}
EXPORT_SYMBOL(get_nuluns);
//end
