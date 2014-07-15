/*drivers/input/keyboard/ft5x0x_ts.c
 *This file is used for FocalTech ft5x0x_ts touchscreen
 *
*/

/*
=======================================================================================================
When		Who	What,Where,Why		Comment			Tag
2011-07-11  xym  update ft driver    ZTE_TS_XYM_20110711
2011-04-20	zfj	add virtual key for P732A			ZFJ_TS_ZFJ_20110420     
2011-03-02	zfj	use create_singlethread_workqueue instead 	ZTE_TS_ZFJ_20110302 
2011-01-08	zfj	Create file			
*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include "ft5x0x_ts_new.h"//ZTE_TS_XYM_20110711
#include <mach/gpio.h>
#include <linux/fb.h>
#include <asm/uaccess.h>
#include <linux/fs.h>


static struct i2c_client *update_client;
int ft_update_result_flag=0;

#if defined(CONFIG_SUPPORT_FTS_CTP_UPG)
int Ft5x0x_fwupdate(struct i2c_client *client);
int Ft5x0x_fwupdate_init(struct i2c_client *client);
int Ft5x0x_fwupdate_deinit(struct i2c_client *client);
#endif

#if defined(CONFIG_FTS_USB_NOTIFY)
static int usb_plug_status=0;
#endif


#if defined(CONFIG_MACH_ATLAS32)//P732B
#define GPIO_TOUCH_EN_OUT  107
#elif defined(CONFIG_MACH_BLADE2)//P735T
#define GPIO_TOUCH_EN_OUT  107
#elif defined(CONFIG_MACH_ROAMER2)//N766
#define GPIO_TOUCH_EN_OUT  107
#elif defined(CONFIG_MACH_DEEPBLUE)//N790
#define GPIO_TOUCH_EN_OUT  107
#elif defined(CONFIG_MACH_ATLAS40)//P740A
#define GPIO_TOUCH_EN_OUT  13
#else
#define GPIO_TOUCH_EN_OUT  107
#endif

#if defined(CONFIG_MACH_ATLAS40)//P740A
#define GPIO_TOUCH_RST_OUT  12
#endif



/*ZTE_TS_ZFJ_20110215 begin*/
#if defined(CONFIG_MACH_ATLAS32)//P732B
#define GPIO_TOUCH_INT_WAKEUP	82
#elif defined(CONFIG_MACH_BLADE2)//P735T
#define GPIO_TOUCH_INT_WAKEUP	82
#elif defined(CONFIG_MACH_ROAMER2)//N766
#define GPIO_TOUCH_INT_WAKEUP	82
#elif defined(CONFIG_MACH_DEEPBLUE)//N790
#define GPIO_TOUCH_INT_WAKEUP	82
#elif defined(CONFIG_MACH_ATLAS40)//P740A
#define GPIO_TOUCH_INT_WAKEUP	82
#else
#define GPIO_TOUCH_INT_WAKEUP	82
#endif
/*ZTE_TS_ZFJ_20110215 end*/

#define ABS_SINGLE_TAP	0x21	/* Major axis of touching ellipse */
#define ABS_TAP_HOLD	0x22	/* Minor axis (omit if circular) */
#define ABS_DOUBLE_TAP	0x23	/* Major axis of approaching ellipse */
#define ABS_EARLY_TAP	0x24	/* Minor axis (omit if circular) */
#define ABS_FLICK	0x25	/* Ellipse orientation */
#define ABS_PRESS	0x26	/* Major axis of touching ellipse */
#define ABS_PINCH 	0x27	/* Minor axis (omit if circular) */



static struct workqueue_struct *Fts_wq;
static struct i2c_driver Fts_ts_driver;

struct Fts_ts_data
{
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct Fts_finger_data finger_data[5];//ZTE_TS_XYM_20110711
	int touch_number;
	int touch_event;
	int use_irq;
	struct hrtimer timer;
	struct work_struct  work;
	uint16_t max[2];
	struct early_suspend early_suspend;
};


//static u8 fwVer=0;//ZTE_TS_XYM_20110830
//#if defined(CONFIG_SUPPORT_FTS_CTP_UPG) ||defined(CONFIG_FTS_USB_NOTIFY)
//static struct i2c_client *update_client;
//static int update_result_flag=0;
//#endif



#if defined (CONFIG_FTS_USB_NOTIFY)
static int Ft5x0x_ts_event(struct notifier_block *this, unsigned long event,void *ptr)
{
	int ret;

	switch(event)
		{
		case 0:
			//offline
			if(usb_plug_status!=0){
		 		usb_plug_status=0;
				//printk("ts config change to offline status\n");
				//Fts_i2c_write( update_client, 0x86,0x1);
				i2c_smbus_write_byte_data( update_client, 0x86,0x1);
			}
			break;
		case 1:
			//online
			if(usb_plug_status!=1){
		 		usb_plug_status=1;
				//printk("ts config change to online status\n");
				//Fts_i2c_write( update_client, 0x86,0x3);
				i2c_smbus_write_byte_data( update_client, 0x86,0x3);
			}
			break;
		default:
			break;
		}

	ret = NOTIFY_DONE;

	return ret;
}

static struct notifier_block ts_notifier = {
	.notifier_call = Ft5x0x_ts_event,
};


static BLOCKING_NOTIFIER_HEAD(ts_chain_head);

int Ft5x0x_register_ts_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&ts_chain_head, nb);
}
EXPORT_SYMBOL_GPL(Ft5x0x_register_ts_notifier);

int Ft5x0x_unregister_ts_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&ts_chain_head, nb);
}
EXPORT_SYMBOL_GPL(Ft5x0x_unregister_ts_notifier);

int Ft5x0x_ts_notifier_call_chain(unsigned long val)
{
	return (blocking_notifier_call_chain(&ts_chain_head, val, NULL)
			== NOTIFY_BAD) ? -EINVAL : 0;
}

#endif


#if defined(CONFIG_TOUCHSCREEN_VIRTUAL_KEYS)
#define virtualkeys virtualkeys.Fts-touchscreen
#if defined (CONFIG_MACH_ROAMER2)||defined (CONFIG_MACH_DEEPBLUE)//N766  320*480       MENU HOME BACK SEARCH
static const char ts_keys_size[] = "0x01:139:45:520:50:50:0x01:102:120:520:60:50:0x01:158:200:520:60:50:0x01:217:265:520:60:50";
#elif defined (CONFIG_MACH_ATLAS40)//P740A  800*480       MENU HOME BACK SEARCH  
static const char ts_keys_size[] = "0x01:139:45:850:100:60:0x01:102:175:850:100:60:0x01:158:306:850:100:60:0x01:217:437:850:100:60";
#elif defined (CONFIG_MACH_HOPE)//N855  320*480       MENU HOME BACK SEARCH
static const char ts_keys_size[] = "0x01:139:35:530:50:60:0x01:102:118:530:60:60:0x01:158:200:530:60:60:0x01:217:277:530:60:60";
#else
static const char ts_keys_size[] = "0x01:139:30:520:50:80:0x01:102:110:520:60:80:0x01:158:200:520:60:80:0x01:217:300:520:60:80";
#endif
static ssize_t virtualkeys_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	sprintf(buf,"%s\n",ts_keys_size);
//	pr_info("%s\n",__FUNCTION__);
    return strlen(ts_keys_size)+2;
}
static DEVICE_ATTR(virtualkeys, 0444, virtualkeys_show, NULL);
extern struct kobject *android_touch_kobj;
static struct kobject * virtual_key_kobj;
static int ts_key_report_init(void)
{
	int ret;

#ifdef CONFIG_MACH_MOONCAKE2
	// mooncake2 did vkey init in board_xxx.c
	return 0;
#endif

	virtual_key_kobj = kobject_get(android_touch_kobj);
	if (virtual_key_kobj == NULL) {
		virtual_key_kobj = kobject_create_and_add("board_properties", NULL);
		if (virtual_key_kobj == NULL) {
			pr_err("%s: subsystem_register failed\n", __func__);
			ret = -ENOMEM;
			return ret;
		}
	}
 
	ret = sysfs_create_file(virtual_key_kobj, &dev_attr_virtualkeys.attr);
	if (ret) {
		pr_err("%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	pr_info("%s: virtual key init succeed!\n", __func__);
	return 0;
}
static void ts_key_report_deinit(void)
{
	sysfs_remove_file(virtual_key_kobj, &dev_attr_virtualkeys.attr);
	kobject_del(virtual_key_kobj);
}

#endif



static int  validate_fts_ctpm(struct i2c_client *client)
{
	int retry;//ret;
	signed int buf;
	
	retry = 3;
	while (retry-- > 0)
	{
		buf = i2c_smbus_read_byte_data(client, FT5X0X_REG_FIRMID);
		if ( buf >= 0 ){
		pr_info("wly: i2c_smbus_read_byte_data, FT5X0X_REG_FIRMID = 0x%x.\n", buf);
		
		buf = i2c_smbus_read_byte_data(client, FT5X0X_REG_FT5201ID);
		switch (buf){
		case 0x57://����Goworld
			pr_info( "module : %s (0x%x)\n", "FT5x06 + Goworld", buf);
			break;
		case 0x51://ŷ�ƹ�ofilm
			pr_info( "module : %s (0x%x)\n", "FT5x06 + ofilm", buf);
			break;
		case 0x55://����
			pr_info(  "module : %s (0x%x)\n", "FT5x06 + laibao", buf);
			break;
		case 0x5a://
			pr_info(  "module : %s (0x%x)\n", "FT5x06 + TRULY", buf);
			break;
		case 0x5f://��˳
			pr_info( "module : %s (0x%x)\n", "FT5x06 + success", buf);
			break;
		case 0x60://
			pr_info(  "module : %s (0x%x)\n", "FT5x06 + lead", buf);
			break;
		case 0x5d://����
			pr_info( "module : %s (0x%x)\n", "FT5x06 + BM", buf);
			break;
		case 0x8f://����
			pr_info(  "module : %s (0x%x)\n", "FT5x06 + CMI", buf);
			break;
		case 0xA5:
			pr_info( "module : %s (0x%x)\n", "FT5x06 + jiaguan", buf);
			break;
		default:
			pr_info( "module : %s (0x%x)\n", "FT5x06 + unknown vendor", buf);
		}
		return true;
		}
		msleep(10);
	}

	printk("wly: focaltech touch is not exsit.\n");
	return false;
}

static int get_screeninfo(uint *xres, uint *yres)
{
	struct fb_info *info;

	info = registered_fb[0];
	if (!info) {
		pr_err("%s: Can not access lcd info \n",__func__);
		return -ENODEV;
	}

	*xres = info->var.xres;
	*yres = info->var.yres;
	printk("%s: xres=%d, yres=%d \n",__func__,*xres,*yres);

	return 1;
}


static int
proc_read_val(char *page, char **start, off_t off, int count, int *eof,
	  void *data)
{
	int len = 0;
	char buf = 0;

	len += sprintf(page + len, "%s\n", "touchscreen module");
	len += sprintf(page + len, "name     : %s\n", "FocalTech");
	len += sprintf(page + len, "i2c address  : 0x%x\n", 0x3E);
	
	buf = i2c_smbus_read_byte_data(update_client, FT5X0X_REG_FT5201ID);
	switch (buf){
	case 0x57://����Goworld
		len += sprintf(page + len, "module : %s (0x%x)\n", "FT5x06 + Goworld", buf);
		break;
	case 0x51://ŷ�ƹ�ofilm
		len += sprintf(page + len, "module : %s (0x%x)\n", "FT5x06 + ofilm", buf);
		break;
	case 0x55://����
		len += sprintf(page + len, "module : %s (0x%x)\n", "FT5x06 + laibao", buf);
		break;
	case 0x5a://
		len += sprintf(page + len, "module : %s (0x%x)\n", "FT5x06 + TRULY", buf);
		break;
	case 0x5f://��˳
		len += sprintf(page + len, "module : %s (0x%x)\n", "FT5x06 + success", buf);
		break;
	case 0x60://
		len += sprintf(page + len, "module : %s (0x%x)\n", "FT5x06 + lead", buf);
		break;
	case 0x5d://����
		len += sprintf(page + len, "module : %s (0x%x)\n", "FT5x06 + BM", buf);
		break;
	case 0x8f://����
		len += sprintf(page + len, "module : %s (0x%x)\n", "FT5x06 + CMI", buf);
		break;
	case 0xA5:
		len += sprintf(page + len, "module : %s (0x%x)\n", "FT5x06 + jiaguan", buf);
		break;
	default:
		len += sprintf(page + len, "module : %s (0x%x)\n", "FT5x06 + unknown vendor", buf);
	}

	buf = i2c_smbus_read_byte_data(update_client, FT5X0X_REG_FIRMID);
	len += sprintf(page + len, "firmware : 0x%x\n", buf );
	
#ifdef CONFIG_SUPPORT_FTS_CTP_UPG
	len += sprintf(page + len, "update flag : 0x%x\n", ft_update_result_flag);
#endif

	if (off + count >= len)
		*eof = 1;
	if (len < off)
		return 0;
	*start = page + off;
	return ((count < len - off) ? count : len - off);
}

static int proc_write_val(struct file *file, const char *buffer,
           unsigned long count, void *data)
{
#ifdef CONFIG_SUPPORT_FTS_CTP_UPG
    int ret = 0;
#endif
    unsigned long val;
    
    sscanf(buffer, "%lu", &val);
    
#ifdef CONFIG_SUPPORT_FTS_CTP_UPG
	printk("%s: Fts Upgrade Start\n", __func__);
	ft_update_result_flag=0;
    
	ret = Ft5x0x_fwupdate(update_client);
	if  (ret < 0){
		printk("%s: fts fw update failed!\n", __func__);
		ft_update_result_flag=1;
		return -EINVAL;
	}else{
		printk("%s: fts fw update successfully!\n", __func__);
		ft_update_result_flag=2;
	}
#endif
	return -EINVAL;//��������ɹ�֮�󻹷�������������
}

static void Fts_ts_work_func(struct work_struct *work)
{
	int ret, i;
	uint8_t buf[33];
#if defined(CONFIG_MACH_MOONCAKE2)//20120109
	static uint8_t ucLast_Point_Num = 0;
	uint8_t ucMaxPoint;
#endif	
	struct Fts_ts_data *ts = container_of(work, struct Fts_ts_data, work);


	//ret = Fts_i2c_read(ts->client, 0x00, buf, 33); 
	ret = i2c_smbus_read_i2c_block_data(ts->client, 0x00, 33, buf);
	if (ret < 0){
   		printk(KERN_ERR "%s: Fts_i2c_read failed, go to poweroff.\n", __func__);
	   	gpio_direction_output(GPIO_TOUCH_EN_OUT, 0);
	    msleep(200);
	    gpio_direction_output(GPIO_TOUCH_EN_OUT, 1);
	    msleep(220);
	}
	else
	{
		ts->touch_number = buf[2]&0x0f;
		ts->touch_event = buf[2] >> 4;
		for (i = 0; i< 5; i++)
		{
			ts->finger_data[i].x = (uint16_t)((buf[3 + i*6] & 0x0F)<<8 )| (uint16_t)buf[4 + i*6];
			ts->finger_data[i].y = (uint16_t)((buf[5 + i*6] & 0x0F)<<8 )| (uint16_t)buf[6 + i*6];
			ts->finger_data[i].z = buf[7 + i*6];
			ts->finger_data[i].w = buf[8 + i*6];
			ts->finger_data[i].touch_id = buf[5 + i*6] >> 4;
			ts->finger_data[i].event_flag = buf[3 + i*6] >> 6;

		}
#if defined(CONFIG_MACH_MOONCAKE2) //20120109
		if ( ucLast_Point_Num > ts->touch_number )
			ucMaxPoint = ucLast_Point_Num;
		else 
			ucMaxPoint = ts->touch_number;
		for (i = 0; i< ucMaxPoint; i++)
		{
			if ( i < ts->touch_number )
			{
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 200);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 10 );
			}else
			{
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
			}

		if ( ts->finger_data[i].y == 1161 ){
			if ( ts->finger_data[i].x == 0 )
				ts->finger_data[i].x = 40;
			if (ts->finger_data[i].x == 1 )
				ts->finger_data[i].x = 120;
			if (ts->finger_data[i].x == 2 )
				ts->finger_data[i].x = 200;
			ts->finger_data[i].y = 350;
		}
#else
		for (i = 0; i< ts->touch_event; i++)
		{
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, ts->finger_data[i].touch_id);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, ts->finger_data[i].z);
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, ts->finger_data[i].w );

#endif
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, ts->finger_data[i].x );
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, ts->finger_data[i].y );
			input_mt_sync(ts->input_dev);
			//printk("%s: finger=%d, z=%d, event_flag=%d, touch_id=%d\n", __func__, i, 
			//ts->finger_data[i].z, ts->finger_data[i].event_flag,ts->finger_data[i].touch_id);
		}

#if defined (CONFIG_MACH_ATLAS40)
		if(ts->finger_data[0].y >=810)
		{
			input_report_key(ts->input_dev, BTN_TOUCH, !!ts->finger_data[0].z);// recovery mode use
		}
#elif defined (CONFIG_MACH_ROAMER2) ||defined (CONFIG_MACH_DEEPBLUE)
		if(ts->finger_data[0].y >=495)
		{
			input_report_key(ts->input_dev, BTN_TOUCH, !!ts->finger_data[0].z);// recovery mode use
		}	
#elif defined (CONFIG_MACH_HOPE)
		if(ts->finger_data[0].y >=495)
		{
			input_report_key(ts->input_dev, BTN_TOUCH, !!ts->finger_data[0].z);// recovery mode use
		}	
#endif			
		input_sync(ts->input_dev);
	}

#if defined(CONFIG_MACH_MOONCAKE2)//20120109
	ucLast_Point_Num = ts->touch_number;
#endif
	if (ts->use_irq)
		enable_irq(ts->client->irq);
}

static irqreturn_t Fts_ts_irq_handler(int irq, void *dev_id)
{
	struct Fts_ts_data *ts = dev_id;

	disable_irq_nosync(ts->client->irq);
	queue_work(Fts_wq, &ts->work);

	return IRQ_HANDLED;
}

static int Fts_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret = 0;
	struct Fts_ts_data *ts;
	
	//ts = container_of(client, struct Fts_ts_data , client);
	ts = i2c_get_clientdata(client);
	disable_irq(client->irq);
	ret = cancel_work_sync(&ts->work);
	if(ret & ts->use_irq)
		enable_irq(client->irq);
	//flush_workqueue(ts->work);
	// ==set mode ==, 
	//ft5x0x_set_reg(FT5X0X_REG_PMODE, PMODE_HIBERNATE);
	//i2c_smbus_write_byte_data(client, FT5X0X_REG_PMODE, PMODE_HIBERNATE);
	gpio_direction_output(GPIO_TOUCH_INT_WAKEUP,1);
	//Fts_i2c_write(client, FT5X0X_REG_PMODE, PMODE_HIBERNATE);
	i2c_smbus_write_byte_data(client, FT5X0X_REG_PMODE, PMODE_HIBERNATE);
	return 0;
}

static int Fts_ts_resume(struct i2c_client *client)
{
	uint8_t buf,retry=0;

Fts_resume_start:	
	//gpio_direction_output(GPIO_TOUCH_EN_OUT,1);
	//gpio_direction_output(GPIO_TOUCH_INT_WAKEUP,1);
	//msleep(250);
	gpio_set_value(GPIO_TOUCH_INT_WAKEUP,0);
	msleep(3);
	gpio_set_value(GPIO_TOUCH_INT_WAKEUP,1);
	msleep(220);
	gpio_direction_input(GPIO_TOUCH_INT_WAKEUP);

	//fix bug: fts failed set reg when usb plug in under suspend mode
#if defined(CONFIG_FTS_USB_NOTIFY)
	if(usb_plug_status==1)
		//Fts_i2c_write( update_client, 0x86,0x3);
		i2c_smbus_write_byte_data( update_client, 0x86,0x3);
	else 
		//Fts_i2c_write( update_client, 0x86,0x1);
		i2c_smbus_write_byte_data( update_client, 0x86,0x1);
#endif

	//if ( Fts_i2c_read(client, FT5X0X_REG_FIRMID, &buf,1) < 0)
	buf = i2c_smbus_read_byte_data(client, FT5X0X_REG_FIRMID );
	if ( !buf )
	{//I2C error read firmware ID
		printk("%s: Fts FW ID read Error: retry=0x%X\n", __func__, retry);
		if ( ++retry < 3 )
			goto Fts_resume_start;
	}

	enable_irq(client->irq);
	
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void Fts_ts_early_suspend(struct early_suspend *h)
{
	struct Fts_ts_data *ts;
	
	ts = container_of(h, struct Fts_ts_data, early_suspend);
	Fts_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void Fts_ts_late_resume(struct early_suspend *h)
{
	struct Fts_ts_data *ts;
	ts = container_of(h, struct Fts_ts_data, early_suspend);
	Fts_ts_resume(ts->client);
}
#endif

static int Fts_ts_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	struct Fts_ts_data *ts;
	int ret = 0;//, retry = 0;
	//u8 fwVer;
	struct proc_dir_entry *dir, *refresh;
	//u8 buf;
	int xres, yres;	// lcd xy resolution

	printk("%s: enter\n", __func__);

#if defined(CONFIG_MACH_ATLAS40)//P740A
	ret = gpio_request(GPIO_TOUCH_RST_OUT, "touch voltage");
	if (ret)
	{	
		printk("%s: gpio %d request is error!\n", __func__, GPIO_TOUCH_RST_OUT);
		return ret;
	}   

	gpio_direction_output(GPIO_TOUCH_RST_OUT, 1);
	msleep(10);
	printk("%s: GPIO_TOUCH_RST_OUT(%d) = %d\n", __func__, GPIO_TOUCH_RST_OUT, gpio_get_value(GPIO_TOUCH_RST_OUT));
#endif	

	ret = gpio_request(GPIO_TOUCH_EN_OUT, "touch voltage");
	if (ret)
	{	
		printk("%s: gpio %d request is error!\n", __func__, GPIO_TOUCH_EN_OUT);
		goto err_gpio_request_GPIO_TOUCH_EN_OUT_failed;
	}   

	gpio_direction_output(GPIO_TOUCH_EN_OUT, 1);
	msleep(300);//xym
	printk("%s: GPIO_TOUCH_EN_OUT(%d) = %d\n", __func__, GPIO_TOUCH_EN_OUT, gpio_get_value(GPIO_TOUCH_EN_OUT));

	ret = gpio_request(GPIO_TOUCH_INT_WAKEUP, "touch voltage");
	if (ret)
	{	
		printk("%s: gpio %d request is error!\n", __func__, GPIO_TOUCH_INT_WAKEUP);
		goto err_gpio_request_GPIO_TOUCH_INT_failed;
	}   

	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		printk(KERN_ERR "%s: need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}
	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL)
	{
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	if ( !validate_fts_ctpm(client) )
		goto err_detect_failed;

	Fts_wq= create_singlethread_workqueue("Fts_wq");
	if(!Fts_wq)
	{
		ret = -ESRCH;
		pr_err("%s: creare single thread workqueue failed!\n", __func__);
		goto err_create_singlethread;
	}

	INIT_WORK(&ts->work, Fts_ts_work_func);
	ts->client = client;

	i2c_set_clientdata(client, ts);
	client->driver = &Fts_ts_driver;

	update_client = client;
	ft_update_result_flag = 0;
	
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		printk(KERN_ERR "%s: Failed to allocate input device\n", __func__);
		goto err_input_dev_alloc_failed;
	}
	
	ts->input_dev->name = "Fts-touchscreen";
	//ts->input_dev->phys = "Fts-touchscreen/input0";

	get_screeninfo(&xres, &yres);

	set_bit(ABS_MT_TRACKING_ID, ts->input_dev->absbit);
	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(BTN_2, ts->input_dev->keybit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	
	set_bit(KEY_HOME, ts->input_dev->keybit);
	set_bit(KEY_MENU, ts->input_dev->keybit);
	set_bit(KEY_BACK, ts->input_dev->keybit);
	set_bit(KEY_SEARCH, ts->input_dev->keybit);
	
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 10, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 127, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, xres, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, yres, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 16, 208, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_SINGLE_TAP, 0, 5, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_TAP_HOLD, 0, 5, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_EARLY_TAP, 0, 5, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_FLICK, 0, 5, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_PRESS, 0, 5, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_DOUBLE_TAP, 0, 5, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_PINCH, -255, 255, 0, 0);

	ret = input_register_device(ts->input_dev);
	if (ret)
	{
		printk(KERN_ERR "%s: Unable to register %s input device\n", __func__, ts->input_dev->name);
		goto err_input_register_device_failed;
	}

    if (client->irq)
    {
      ret = request_irq(client->irq, Fts_ts_irq_handler, IRQF_TRIGGER_FALLING, "ft5x0x_ts", ts);
      if (ret == 0)
        ts->use_irq = 1;
      else
      {
        dev_err(&client->dev, "request_irq failed\n");
        goto err_input_request_irq_failed;
      }
    }
#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = Fts_ts_early_suspend;
	ts->early_suspend.resume = Fts_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif
	dir = proc_mkdir("touchscreen", NULL);
	refresh = create_proc_entry("ts_information", 0664, dir);
	if (refresh) {
		refresh->data		= NULL;
		refresh->read_proc  = proc_read_val;
		refresh->write_proc = proc_write_val;
	}
	printk(KERN_INFO "%s: Start touchscreen %s in %s mode\n", __func__, ts->input_dev->name, ts->use_irq ? "interrupt" : "polling");
	
#if defined(CONFIG_TOUCHSCREEN_VIRTUAL_KEYS)
	ts_key_report_init();
#endif

#if defined(CONFIG_FTS_USB_NOTIFY)
	Ft5x0x_register_ts_notifier(&ts_notifier);
#endif

#if defined(CONFIG_SUPPORT_FTS_CTP_UPG)
	ret = Ft5x0x_fwupdate_init(client);
	if ( ret < 0 )
		printk("%s: firmware update initialization failed!\n ",__func__);
#endif

	return 0;

err_input_request_irq_failed:
err_input_register_device_failed:
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
	destroy_workqueue(Fts_wq);
err_create_singlethread:
err_detect_failed:
	kfree(ts);	
err_alloc_data_failed:
err_check_functionality_failed:
	gpio_free(GPIO_TOUCH_INT_WAKEUP);
err_gpio_request_GPIO_TOUCH_INT_failed:
	gpio_free(GPIO_TOUCH_EN_OUT);
err_gpio_request_GPIO_TOUCH_EN_OUT_failed:
#if defined(CONFIG_MACH_ATLAS40)//P740A
	gpio_free(GPIO_TOUCH_RST_OUT);
#endif	

	printk("%s: exit\n", __func__);

	return ret;
}

static int Fts_ts_remove(struct i2c_client *client)
{
	struct Fts_ts_data *ts = i2c_get_clientdata(client);
#if defined(CONFIG_TOUCHSCREEN_VIRTUAL_KEYS)
	ts_key_report_deinit();
#endif

#if defined(CONFIG_SUPPORT_FTS_CTP_UPG)
	Ft5x0x_fwupdate_deinit(client);
#endif

	unregister_early_suspend(&ts->early_suspend);
	if (ts->use_irq)
		free_irq(client->irq, ts);
	else
		hrtimer_cancel(&ts->timer);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	gpio_direction_output(GPIO_TOUCH_EN_OUT, 0);
	return 0;
}




static const struct i2c_device_id Fts_ts_id[] = {
	{ "ft5x0x_ts", 0 },
	{ }
};

static struct i2c_driver Fts_ts_driver = {
	.probe		= Fts_ts_probe,
	.remove		= Fts_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= Fts_ts_suspend,
	.resume		= Fts_ts_resume,
#endif
	.id_table	= Fts_ts_id,
	.driver 	= {
		.name	= "ft5x0x_ts",
	},
};

static int __devinit Fts_ts_init(void)
{
	/*ZTE_TS_ZFJ_20110302 begin*/
	#if 0
	Fts_wq = create_rt_workqueue("Fts_wq");
	if (!Fts_wq)
		return -ENOMEM;
	#endif
	/*ZTE_TS_ZFJ_20110302 end*/
	return i2c_add_driver(&Fts_ts_driver);
}

static void __exit Fts_ts_exit(void)
{
	i2c_del_driver(&Fts_ts_driver);
	if (Fts_wq)
		destroy_workqueue(Fts_wq);
}

module_init(Fts_ts_init);
module_exit(Fts_ts_exit);

MODULE_DESCRIPTION("Fts Touchscreen Driver");
MODULE_LICENSE("GPL");
