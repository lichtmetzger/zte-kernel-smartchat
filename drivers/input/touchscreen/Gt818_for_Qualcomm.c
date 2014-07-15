/* drivers/input/touchscreen/goodix_touch.c
 *
 * Copyright (C) 2011 Goodix, Inc.
 * 
 * Author: kuuga
 * Date: 2011.12.28
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>
#include <linux/irq.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <asm/uaccess.h>
#include "Gt818_for_Qualcomm.h"


#include <linux/fb.h>

#ifdef CONFIG_TOUCHSCREEN_GOODIX_IAP
#include "Gt818_update.h"
#endif
/*
#ifdef CREATE_WR_NODE
extern s32 init_wr_node(struct i2c_client*);
extern void uninit_wr_node(void);
#endif

#if !defined(GT801_PLUS) && !defined(GT801_NUVOTON)
#error The code does not match this touchscreen.
#endif
*/
static struct workqueue_struct *goodix_wq;
//static const char *gt818_ts_name = "Goodix Capacitive TouchScreen";
//static struct point_queue finger_list;
struct i2c_client * i2c_connect_client = NULL;
static int gdx_update_result_flag=0;


//EXPORT_SYMBOL(i2c_connect_client);
//static struct proc_dir_entry *goodix_proc_entry;
	
#ifdef CONFIG_HAS_EARLYSUSPEND
static void goodix_ts_early_suspend(struct early_suspend *h);
static void goodix_ts_late_resume(struct early_suspend *h);
#endif

//used by firmware update CRC
unsigned int oldcrc32 = 0xFFFFFFFF;
unsigned int crc32_table[256];
unsigned int ulPolynomial = 0x04c11db7;

#ifdef CONFIG_TOUCHSCREEN_GOODIX_IAP
static int  gt818_downloader( struct goodix_ts_data *ts,  unsigned char * data, unsigned char * path);
#endif


#if defined(CONFIG_TOUCHSCREEN_VIRTUAL_KEYS)
#define virtualkeys virtualkeys.Goodix-TS
#if defined (CONFIG_MACH_ROAMER2)//N766  320*480       MENU HOME BACK SEARCH
static const char ts_keys_size[] = "0x01:139:45:520:50:50:0x01:102:120:520:60:50:0x01:158:200:520:60:50:0x01:217:265:520:60:50";
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

static int
proc_read_val(char *page, char **start, off_t off, int count, int *eof,
	  void *data)
{
	int len = 0;
	char buf = 0;

	len += sprintf(page + len, "%s\n", "touchscreen module");
	len += sprintf(page + len, "name     : %s\n", "Goodix");
	len += sprintf(page + len, "i2c address  : 0x%x\n", 0x5D);
	
	buf = i2c_smbus_read_byte_data(i2c_connect_client, 0);
	switch (buf){
	default:
		len += sprintf(page + len, "module : %s (0x%x)\n", "FT5x06 + unknown vendor", buf);
	}

	buf = i2c_smbus_read_byte_data(i2c_connect_client, 0);
	len += sprintf(page + len, "firmware : 0x%x\n", buf );
	
//#ifdef CONFIG_SUPPORT_FTS_CTP_UPG
	len += sprintf(page + len, "update flag : 0x%x\n", gdx_update_result_flag);
//#endif

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
    unsigned long val;
    
    sscanf(buffer, "%lu", &val);

	gdx_update_result_flag = 0;

	gdx_update_result_flag = 2;
    
	return 0;
}


/*Function as i2c_master_send */
static int i2c_read_bytes(struct i2c_client *client, uint8_t *buf, int len)
{
	struct i2c_msg msgs[2];
	int ret=-1;
	msgs[0].flags=!I2C_M_RD;
	msgs[0].addr=client->addr;
	msgs[0].len=2;
	msgs[0].buf=&buf[0];
	msgs[1].flags=I2C_M_RD;
	msgs[1].addr=client->addr;
	msgs[1].len=len-2;
	msgs[1].buf=&buf[2];
	
	ret=i2c_transfer(client->adapter,msgs, 2);
	return ret;
}

/*Function as i2c_master_send */
static int i2c_write_bytes(struct i2c_client *client,uint8_t *data,int len)
{
	struct i2c_msg msg;
	int ret=-1;
	msg.flags=!I2C_M_RD;
	msg.addr=client->addr;
	msg.len=len;
	msg.buf=data;		
	
	ret=i2c_transfer(client->adapter,&msg, 1);
	return ret;
}

static int i2c_pre_cmd(struct goodix_ts_data *ts)
{
	int ret;
	uint8_t pre_cmd_data[2]={0};	
	pre_cmd_data[0]=0x0f;
	pre_cmd_data[1]=0xff;
	ret=i2c_write_bytes(ts->client,pre_cmd_data,2);
	//msleep(2);
	return ret;
}

static int i2c_end_cmd(struct goodix_ts_data *ts)
{
	int ret;
	uint8_t end_cmd_data[2]={0};	
	end_cmd_data[0]=0x80;
	end_cmd_data[1]=0x00;
	ret=i2c_write_bytes(ts->client,end_cmd_data,2);
	//msleep(2);
	return ret;
}

#ifdef CONFIG_TOUCHSCREEN_GOODIX_IAP
static short get_chip_version( unsigned int sw_ver )
{
   if ( (sw_ver&0xff) < TPD_CHIP_VERSION_C_FIRMWARE_BASE )           	
   	return TPD_GT818_VERSION_B;       
   else if ( (sw_ver&0xff) < TPD_CHIP_VERSION_D1_FIRMWARE_BASE )           	
   	return TPD_GT818_VERSION_C;       
   else if((sw_ver&0xff) < TPD_CHIP_VERSION_E_FIRMWARE_BASE)           	
   	return TPD_GT818_VERSION_D1;       
   else if((sw_ver&0xff) < TPD_CHIP_VERSION_D2_FIRMWARE_BASE)           	
   	return TPD_GT818_VERSION_E;       
   else           
   	return TPD_GT818_VERSION_D2;
}
#endif

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



static int goodix_init_panel(struct goodix_ts_data *ts)
{

	int ret=-1;
	uint8_t config_info[2][108] = {
	// sensor id = 0
	{
			0x06,0xA2,
	0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,
	0x10,0x12,0x82,0x22,0x92,0x22,0xA2,0x22,
	0xB2,0x22,0xC2,0x22,0xD2,0x22,0xE2,0x22,
	0xF2,0x22,0x72,0x22,0x62,0x22,0x52,0x22,
	0x42,0x22,0x32,0x22,0x22,0x22,0x12,0x22,
	0x02,0x22,0x07,0x03,0x10,0x10,0x10,0x33,
	0x33,0x33,0x10,0x10,0x0A,0x47,0x2A,0x01,
	0x03,0x00,0x05,0x40,0x01,0x12,0x02,0x00,
	0x00,0x5C,0x5D,0x60,0x61,0x00,0x00,0x13,
	0x14,0x25,0x06,0x00,0x00,0x00,0x00,0x00,
	0x14,0x10,0x06,0x04,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x0D,0x40,
	0x2A,0x20,0x14,0x00,0x00,0x00,0x00,0x00,
	0x00,0x01
	},
	// sensor id = 1
	{
		0x06,0xA2,
	0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,
	0x10,0x12,0xF2,0x22,0xE2,0x22,0xD2,0x22,
	0xC2,0x22,0x82,0x22,0x92,0x22,0xA2,0x22,
	0xB2,0x22,0x72,0x22,0x62,0x22,0x52,0x22,
	0x42,0x22,0x32,0x22,0x22,0x22,0x12,0x22,
	0x02,0x22,0x07,0x03,0x10,0x10,0x10,0x33,
	0x33,0x33,0x10,0x10,0x0A,0x47,0x2A,0x01,
	0x03,0x00,0x05,0x40,0x01,0x12,0x02,0x00,
	0x00,0x5C,0x5D,0x60,0x61,0x00,0x00,0x13,
	0x14,0x25,0x06,0x00,0x00,0x00,0x00,0x00,
	0x14,0x10,0x06,0x04,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x0D,0x40,
	0x2A,0x20,0x14,0x00,0x00,0x00,0x00,0x00,
	0x00,0x01
	}
	};		

#if 0 //def SELF_DEF
	ret=i2c_write_bytes(ts->client,config_info, (sizeof(config_info)/sizeof(config_info[0])));
	ts->abs_x_max = 320;
	ts->abs_y_max = 480;
	ts->max_touch_num = config_info[60];
	ts->int_trigger_type = ((config_info[57]>>3)&0x01);
#else
	ret=i2c_write_bytes(
		ts->client,
		config_info[ts->sensor_id], 
		sizeof(config_info[0])/sizeof(config_info[0][0])
		);
	ts->abs_x_max = 320;
	ts->abs_y_max = 480;
	ts->max_touch_num = config_info[ts->sensor_id][60];
	ts->int_trigger_type = ((config_info[ts->sensor_id][57]>>3)&0x01);

#endif
	if (ret < 0) 
		return ret;
	//msleep(10);
	return 0;

}


static int  goodix_read_version(struct goodix_ts_data *ts)
{
	int ret;
	uint8_t version_data[5]={0};	//store touchscreen version infomation
	uint8_t version_data2[5]={0x07,0x17,0,0};	//store touchscreen version infomation
	char i = 0;
	char cpf = 0;
	memset(version_data, 0, 5);
	version_data[0]=0x07;
	version_data[1]=0x17;	
	//msleep(2);
	ret=i2c_read_bytes(ts->client, version_data, 4);
	if (ret < 0) 
		return ret;
		
	for(i = 0;i < 10;i++)
	{
		i2c_read_bytes(ts->client, version_data2, 4);
		if((version_data[2] !=version_data2[2])||(version_data[3] != version_data2[3]))
		{
			version_data[2] = version_data2[2];
			version_data[3] = version_data2[3];
			//msleep(5);
			break;
		}
		msleep(5);
		cpf++;
	}

	if(cpf == 10)
	{
	dev_info(&ts->client->dev," Guitar Version: %d.%d\n",version_data[3],version_data[2]);
	printk(" Guitar Version: %d.%d\n",version_data[3],version_data[2]);
	ts->version = (version_data[3]<<8)+version_data[2];
	ret = 0;
	}
	else
	{
		dev_info(&ts->client->dev," Guitar Version Read Error: %d.%d\n",version_data[3],version_data[2]);
		printk(" Guitar Version Read Error: %d.%d\n",version_data[3],version_data[2]);
		ts->version = 0xffff;
		ret = -1;
	}
	return ret;
}

/******************start add by kuuga*******************/
/*
static void gt818_irq_enable(struct goodix_ts_data *ts)
{	
	unsigned long irqflags;	
	spin_lock_irqsave(&ts->irq_lock, irqflags);
	if (ts->irq_is_disable) 
	{		
		enable_irq(ts->irq);		
		ts->irq_is_disable = 0;	
	}	
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}

static void gt818_irq_disable(struct goodix_ts_data *ts)
{	
	unsigned long irqflags;
	spin_lock_irqsave(&ts->irq_lock, irqflags);
	if (!ts->irq_is_disable) 
		{		
			disable_irq_nosync(ts->irq);		
			ts->irq_is_disable = 1;	
		}	
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}
*/
/*****************end add by kuuga****************/
//#ifdef TOUCHSCREEN_GOODIX_READ_SENSOR_ID
static int  goodix_read_sensor_ID(struct i2c_client *client)
{
	int ret;
	uint8_t sensor_data[5]={0};    //store touchscreen version infomation
	memset(sensor_data, 0, 5);
	sensor_data[0]=0x07;
	sensor_data[1]=0x10;
	ret= i2c_read_bytes(client, sensor_data, 3);

	if (ret < 0)
		return ret;

	pr_info(" sensor_ID: %d\n",sensor_data[2]);
	return (sensor_data[2]&0x07);
}
//#endif


static void goodix_ts_work_func(struct work_struct *work)
{	
	uint8_t  touch_data[3] = {READ_TOUCH_ADDR_H,READ_TOUCH_ADDR_L,0};
	uint8_t  key_data[3] ={READ_KEY_ADDR_H,READ_KEY_ADDR_L,0};
	uint8_t  point_data[8*MAX_FINGER_NUM+2]={ 0 };  
	static uint8_t   finger_last[MAX_FINGER_NUM+1]={0};		//\u4e0a\u6b21\u89e6\u6478\u6309\u952e\u7684\u624b\u6307\u7d22\u5f15
	uint8_t  finger_current[MAX_FINGER_NUM+1] = {0};		//\u5f53\u524d\u89e6\u6478\u6309\u952e\u7684\u624b\u6307\u7d22\u5f15
	uint8_t  coor_data[6*MAX_FINGER_NUM] = {0};				//\u5bf9\u5e94\u624b\u6307\u7684\u6570\u636e
//	static uint8_t  last_key = 0;
	uint8_t  finger = 0;
	uint8_t  key = 0;
	unsigned int  count = 0;
	unsigned int position = 0;	
	int ret=-1;
	int tmp = 0;
	int temp = 0;
	int retry; 
	uint16_t *coor_point;
	
	struct goodix_ts_data *ts = container_of(work, struct goodix_ts_data, work);


	printk("%s ~\n",__func__);

	i2c_pre_cmd(ts);
/*	if( tmp > 9) {
		printk("Because of transfer error,touchscreen stop working.\n");
		goto XFER_ERROR ;
	}
	*/

	ret=i2c_read_bytes(ts->client, touch_data,sizeof(touch_data)/sizeof(touch_data[0]));  //\u8bfb0x712\uff0c\u89e6\u6478	
	if(ret <= 0) {
		printk("I2C transfer error. Number:%d\n ", ret);
		ts->bad_data = 1;
		tmp ++;
		ts->retry++;
		goto XFER_ERROR;
	}

	
//	printk("xiayc: touch data = 0x%x\n",touch_data[2]);


	
	ret=i2c_read_bytes(ts->client, key_data,sizeof(key_data)/sizeof(key_data[0]));  //\u8bfb0x721\uff0c\u6309\u952e 
	if(ret <= 0) {
		printk("I2C transfer error. Number:%d\n ", ret);
		ts->bad_data = 1;
		tmp ++;
		ts->retry++;
		goto XFER_ERROR;
	}
	key = key_data[2]&0x0f;

//	printk("xiayc: key  data = 0x%x\n",key_data[2]);

	if(ts->bad_data)
	{
		printk(" bad data \n");
		//TODO:Is sending config once again (to reset the chip) useful?	
		msleep(20);
	}
	
#if 1
	if(touch_data[2]==0x0f)
	{
		for(retry=0; retry<3; retry++)
		{
			ret=goodix_init_panel(ts);
			printk("the config ret is :%d\n",ret);
			msleep(2);
			if(ret != 0)	//Initiall failed
				continue;
			else
				break;
		}
		goto DATA_NO_READY;		
	}
#endif	
	if( ( touch_data[2] &0x30 ) != 0x20 ){
//		printk("xiayc: data not ready!\n");
		goto DATA_NO_READY;		
		}
		


	ts->bad_data = 0;

	finger = touch_data[2]&0x0f;
//	printk("xiayc: finger =%d\n",finger);
	if(finger != 0)
	{
		point_data[0] = READ_COOR_ADDR_H;		//read coor high address
		point_data[1] = READ_COOR_ADDR_L;		//read coor low address
		ret=i2c_read_bytes(ts->client, point_data, finger*8+2);
		if(ret <= 0)	
		{
			printk("I2C transfer error. Number:%d\n ", ret);
			ts->bad_data = 1;
			tmp ++;
			ts->retry++;
			goto XFER_ERROR;
		}		
		for(position=2; position<((finger-1)*8+2+1); position += 8)
		{
			temp = point_data[position];
			if(temp<(MAX_FINGER_NUM+1))	{
				finger_current[temp] = 1;
				for(count=0; count<6; count++){
					coor_data[(temp-1)*6+count] = point_data[position+1+count];
//					printk("xiayc: point %d: %d\n",position,coor_data[(temp-1)*6+count]);
				}
			}else{
				//dev_err(&(ts->client->dev),"Track Id error:%d\n ");
				printk(" Track Id error:\n ");
				ts->bad_data = 1;
				tmp ++;
				ts->retry++;
				goto XFER_ERROR;
			}		
		}
		//coor_point = (uint16_t *)coor_data;		
	
	}
	else
	{
		for(position=1;position < MAX_FINGER_NUM+1; position++)		
		{
//			printk("xiayc......%d\n", position);
			finger_current[position] = 0;
		}
	}

	coor_point = (uint16_t *)coor_data;	
	for( position = 1; position < MAX_FINGER_NUM+1; position++)
	{
//		printk("xiayc: position %d\n", position);
		if( (finger_current[position] == 0) && (finger_last[position] != 0) )
		{
//			printk("xiayc: 111 %d\n",position);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, 0);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, 0);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);	
			input_mt_sync(ts->input_dev);
		}
		else if(finger_current[position])
		{ 	
		
//		printk("xiayc: 222 %d\n",position);
		
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, position-1);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,((*(coor_point+3*(position-1)+2))));
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,5);

			input_report_abs(ts->input_dev, ABS_MT_POSITION_X,((*(coor_point+3*(position-1)))));
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,((*(coor_point+3*(position-1)+1))));
			
			input_mt_sync(ts->input_dev);			
	#if 0
			printk("%d*",(*(coor_point+3*(position-1))));
			printk("%d*",(*(coor_point+3*(position-1)+1)));
			printk("\n");
	#endif	
		}
			
	}
	input_sync(ts->input_dev);	
	
	for(position=1;position<MAX_FINGER_NUM+1; position++)
	{
		finger_last[position] = finger_current[position];
	}

//#ifdef HAVE_TOUCH_KEY
#if 0
	if((last_key == 0)&&(key == 0))
		;
	else
	{
		for(count = 0; count < 4; count++)
		{
			input_report_key(ts->input_dev, touch_key_array[count], !!(key&(0x01<<count)));	
		}
	}		
	last_key = key;	
#endif

DATA_NO_READY:
XFER_ERROR:
	i2c_end_cmd(ts);
	if( ts->use_irq ){
//		printk("xiayc: enable irq!\n");
		enable_irq(ts->client->irq);
		}

}


static enum hrtimer_restart goodix_ts_timer_func(struct hrtimer *timer)
{
	struct goodix_ts_data *ts = container_of(timer, struct goodix_ts_data, timer);

	#if 0
		printk(KERN_INFO"-------------------goodix_ts_timer_func------------------\n");
	#endif
	queue_work(goodix_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, (POLL_TIME+6)*1000000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static irqreturn_t goodix_ts_irq_handler(int irq, void *dev_id)
{
	struct goodix_ts_data *ts = dev_id;

#if 0
	printk(KERN_INFO"-------------------ts_irq_handler------------------\n");
#endif
	disable_irq_nosync(ts->client->irq);            //  kuuga del 111205
//#ifndef STOP_IRQ_TYPE
//		gt818_irq_disable(ts);     //KT ADD 1202
//#endif
	queue_work(goodix_wq, &ts->work);
//	schedule_work(&ts->work);
	return IRQ_HANDLED;
}

static int goodix_ts_power(struct goodix_ts_data * ts, int on)
{
	int ret = -1;
//	int tmp;
//	char mode;
	unsigned char i2c_control_buf[3] = {0x06,0x92,0x01};		//suspend cmd

//#if 0
//	printk(KERN_INFO"-------------------goodix_ts_power------on = %d------------\n", on);
//#endif

//#ifdef GPIO_TS_INT	
	if(ts != NULL && !ts->use_irq)
		return -2;
//#endif		
	switch(on)
	{
		case 0:
			
			gpio_direction_output(GPIO_TS_INT,1);
			i2c_pre_cmd(ts);               //must
			ret = i2c_write_bytes(ts->client, i2c_control_buf, 3);
//			pr_info("xiayc: Send suspend cmd\n");
			if(ret > 0)						//failed
				ret = 0;
			i2c_end_cmd(ts);                     //must
			return ret;
			
		case 1:
//			#ifdef GPIO_TS_INT	                     //suggest use INT PORT to wake up !!!
/*				if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_INT,0,GPIO_OUTPUT,GPIO_PULL_UP,GPIO_2MA), GPIO_ENABLE)) {
					printk(KERN_INFO "config ts int pin failed \n");
					goto config_err;
				}
		*/		
//						i2c_read_bytes(ts->client, &mode, 1);
//				printk("xiayc: resume  0x%x\n", mode);

				gpio_set_value(GPIO_TS_INT, 0);
				msleep(5);
				gpio_set_value(GPIO_TS_INT, 1);
				msleep(5);
				
				gpio_direction_input(GPIO_TS_INT);
//				gpio_set_value(GPIO_TS_INT, 0);

//				gpio_free(GPIO_TS_INT);

/*				if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_INT,0,GPIO_INPUT,GPIO_NO_PULL,GPIO_2MA), GPIO_ENABLE)) {
					printk(KERN_INFO "config ts int pin failed \n");
					goto config_err;
				}
	*/			
//			#else
//				gpio_direction_output(SHUTDOWN_PORT,0);
//				msleep(1);
//				gpio_direction_input(SHUTDOWN_PORT);		
//			#endif					
				msleep(40);
				ret = 0;
				return ret;
				
		default:
			//printk(KERN_DEBUG "%s: Cant't support this command.", goodix_ts_power);
			printk("%s: Cant't support this command.", __func__);
			return -EINVAL;
	}

//	config_err:
//		gpio_free(GPIO_TS_INT);
		return -EINVAL;

}

static int goodix_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	int retry=0;
//	uint8_t test_data = 1;
	uint8_t goodix_id[3] = {0,0xff,0};
	struct goodix_ts_data *ts;
//	uint8_t test_iic_buf[1]={0xFF};
	uint8_t update_path[1]= {0};
//	const char irq_table[2] = {IRQ_TYPE_EDGE_FALLING,IRQ_TYPE_EDGE_RISING};
	
	int xres, yres;	// lcd xy resolution
	struct proc_dir_entry *dir, *refresh;


	struct goodix_i2c_rmi_platform_data *pdata;
	dev_dbg(&client->dev,"Install touch driver.\n");

	printk("%s enter\n", __func__);

	//Check I2C function
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) 
	{
		dev_err(&client->dev, "Must have I2C_FUNC_I2C.\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	i2c_connect_client = client;	//used by Guitar_Update

	if ( gpio_request(GPIO_TS_INT,"TS_INT") ) {	
		printk(KERN_INFO "request ts int pin failed \n");
		goto int_port_failed;
	}
/*	
	if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_INT,0,GPIO_INPUT,GPIO_NO_PULL,GPIO_2MA),GPIO_ENABLE)) {
		printk(KERN_INFO "config ts int pin failed \n");
		goto config_int_port_err;
	}

	if(gpio_request(GPIO_TS_RESET,"TS_RESET")) {
		printk(KERN_INFO "request ts reset pin failed \n");
		goto reset_port_failed;
	}

	if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_RESET,0,GPIO_OUTPUT,GPIO_PULL_UP,GPIO_2MA),GPIO_ENABLE)) {
		printk(KERN_INFO "config ts reset pin failed \n");
		goto config_reset_port_err;

	}
*/
	if( gpio_request(SHUTDOWN_PORT, "touchscreen power")){
		printk("%s: unable to request gpio %d (%d)\n",	__func__,SHUTDOWN_PORT ,ret);
			goto power_port_failed;
	}

//	gpio_direction_output(GPIO_TS_POWER,0);
//	msleep(1);
//	gpio_direction_input(GPIO_TS_POWER);
//	msleep(20);

	gpio_direction_output(SHUTDOWN_PORT, 1);
	msleep(250);
	
	//char test_data = 1;
//	printk("xiayc: test i2c++++++++++ \n");
	ret =i2c_read_bytes(client, goodix_id, 3);	//Test I2C connection.
	if (ret > 0)
	{
		printk("id: 0x%x, 0x%x, 0x%x\n", goodix_id[0],goodix_id[1],goodix_id[2] );
	}
	else
		goto err_i2c_failed;
//	printk("xiayc: test i2c-------- \n");
	


#if  0//xiayc
#if defined(NO_DEFAULT_ID) && defined(GPIO_TS_INT)
	for(retry=0;retry < 3; retry++)
	{
		//reset 1ms 	
		gpio_set_value(GPIO_TS_RESET, 0);
		msleep(1);
		if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_RESET,0,GPIO_INPUT,GPIO_NO_PULL,GPIO_2MA),GPIO_ENABLE)) {
			printk(KERN_INFO "config ts reset pin input failed \n");
			goto config_reset_port_err;

		}
		msleep(20);
		ret =i2c_write_bytes(client, &test_data, 1);	//Test I2C connection.
		if (ret > 0)
			break;
	}
	
	if(ret <= 0)
	{
		if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_INT,0,GPIO_OUTPUT,GPIO_PULL_UP,GPIO_2MA),GPIO_ENABLE)) {
			printk(KERN_INFO "config ts int pin failed \n");
			goto config_reset_port_err;
		}
		gpio_set_value(GPIO_TS_INT,0);
		msleep(1);
		if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_RESET,0,GPIO_OUTPUT,GPIO_NO_PULL,GPIO_2MA),GPIO_ENABLE)) {
			printk(KERN_INFO "config ts reset pin input failed \n");
			goto config_reset_port_err;

		}
		gpio_set_value(GPIO_TS_RESET,0);
		msleep(20);
		if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_RESET,0,GPIO_INPUT,GPIO_NO_PULL,GPIO_2MA),GPIO_ENABLE)) {
			printk(KERN_INFO "config ts reset pin input failed \n");
			goto config_reset_port_err;

		}

		for(retry=0;retry < 80; retry++)
		{
			ret =i2c_write_bytes(client, &test_data, 1);	//Test I2C connection.
			if (ret > 0)
			{
				msleep(10);
				ret =i2c_read_bytes(client, goodix_id, 3);	//Test I2C connection.
				if (ret > 0)
				{
					if(goodix_id[2] == 0x55)
						{
						if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_INT,0,GPIO_OUTPUT,GPIO_PULL_UP,GPIO_2MA),GPIO_ENABLE)) {
							printk(KERN_INFO "config ts int pin failed \n");
							goto config_reset_port_err;
						}						
						msleep(1);
						if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_INT,0,GPIO_INPUT,GPIO_NO_PULL,GPIO_2MA),GPIO_ENABLE)) {
							printk(KERN_INFO "config ts int pin failed \n");
							goto config_reset_port_err;
						}						
						msleep(10);
						break;						
						}
				}			
			}	
		
		}
	}

#endif


	for(retry=0;retry < 3; retry++)
	{
		//reset 1ms 	
		gpio_set_value(GPIO_TS_RESET, 0);
		msleep(1);
		#if 0
		if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_RESET,0,GPIO_INPUT,GPIO_NO_PULL,GPIO_2MA),GPIO_ENABLE)) {
			printk(KERN_INFO "config ts reset pin input failed \n");
			goto config_reset_port_err;

		}
		msleep(20);
		#endif
		if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_RESET,0,GPIO_INPUT,GPIO_PULL_UP,GPIO_2MA),GPIO_ENABLE)) {
			printk(KERN_INFO "config ts reset pin input failed \n");
			goto config_reset_port_err;

		}                 // turn to input and pull up  add kuuga 
		msleep(20);
		ret =i2c_write_bytes(client, &test_data, 1);	//Test I2C connection.
		if (ret > 0)
			break;
	}

	if(ret <= 0)
	{
		dev_err(&client->dev, "Warnning: I2C communication might be ERROR!\n");
		goto err_i2c_failed;
	}
	
#endif


	goodix_wq = create_singlethread_workqueue("goodix_wq");		//create a work queue and worker thread
	if (!goodix_wq) {
		printk(KERN_ALERT "creat workqueue faiked\n");
		goto err_create_proc_entry;
		
	}
	
	INIT_WORK(&ts->work, goodix_ts_work_func);		//init work_struct
	ts->client = client;
	i2c_set_clientdata(client, ts);
	pdata = client->dev.platform_data;
	
//#ifndef STOP_IRQ_TYPE		
	//ts->irq = TS_INT;     //KT ADD 1202		
//	ts->irq_is_disable = 0;           // enable irq	
//#endif

/////////////////////////////// UPDATE STEP 1 START/////////////////////////////////////////////////////////////////
#ifdef CONFIG_TOUCHSCREEN_GOODIX_IAP
            i2c_pre_cmd(ts);
            goodix_read_version(ts);
            i2c_end_cmd(ts);
            
            ret = gt818_downloader( ts, goodix_gt818_firmware, update_path);
            if(ret < 0)
            {
                dev_err(&client->dev, "Warnning: GT818 update might be ERROR!\n");
                //goto err_input_dev_alloc_failed;
            }
//			printk("xiayc test\n");
//			return 0;

#endif

///////////////////////////////UPDATE STEP 1 END////////////////////////////////////////////////////////////////       

////////////////////////////////////////////////////////////probe 16 pin/////////////////////////////////////

//#ifdef TOUCHSCREEN_GOODIX_READ_SENSOR_ID
	i2c_pre_cmd(ts);
	ts->sensor_id = goodix_read_sensor_ID(client);
	i2c_end_cmd(ts);
//#endif

////////////////////////////////////////////////////////////probe 16 pin/////////////////////////////////////
	
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		dev_dbg(&client->dev,"goodix_ts_probe: Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}


	get_screeninfo(&xres, &yres);

	i2c_pre_cmd(ts);
	
	for(retry=0; retry<3; retry++)
	{
		ret=goodix_init_panel(ts);
		dev_info(&client->dev,"the config ret is :%d\n",ret);
		msleep(2);
		if(ret != 0){	//Initiall failed
			printk("goodix_init_panel failed!!\n");
			continue;
		}
		else
			break;
	}
	
	if(ret != 0) 
	{
		ts->bad_data=1;
		goto err_init_godix_ts;
	}
	printk("ts: x,y = (%d, %d)\n",ts->abs_x_max,ts->abs_y_max );
	printk("lcd: x,y = (%d, %d)\n",xres, yres );

	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	//ts->input_dev->absbit[0] = BIT(ABS_X) | BIT(ABS_Y) | BIT(ABS_PRESSURE); 						// absolute coor (x,y)
//#ifdef HAVE_TOUCH_KEY
	for(retry = 0; retry < MAX_KEY_NUM; retry++)
	{
		input_set_capability(ts->input_dev,EV_KEY,touch_key_array[retry]);	
	}
//#endif

//	input_set_abs_params(ts->input_dev, ABS_X, 0, ts->abs_x_max, 0, 0);
//	input_set_abs_params(ts->input_dev, ABS_Y, 0, ts->abs_y_max, 0, 0);

//	input_set_abs_params(ts->input_dev, ABS_PRESSURE, 0, 255, 0, 0);
	
//#ifdef GOODIX_MULTI_TOUCH
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 10, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 2, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);
//	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->abs_x_max, 0, 0);
//	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->abs_y_max, 0, 0);	
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, xres, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, yres, 0, 0);	
/*	input_set_abs_params(ts->input_dev, ABS_SINGLE_TAP, 0, 5, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_TAP_HOLD, 0, 5, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_EARLY_TAP, 0, 5, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_FLICK, 0, 5, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_PRESS, 0, 5, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_DOUBLE_TAP, 0, 5, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_PINCH, -0xFF, 0xFF, 0, 0);
*/
//#endif	

//#ifdef CREATE_WR_NODE
//    init_wr_node(client);
//#endif

	sprintf(ts->phys, "input/ts");
	ts->input_dev->name = GOODIX_I2C_NAME;//gt818_ts_name;
	ts->input_dev->phys = ts->phys;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->id.vendor = 0xDEAD;
	ts->input_dev->id.product = 0xBEEF;
	ts->input_dev->id.version = 10427;	//screen firmware version
	
	ret = input_register_device(ts->input_dev);
	if (ret) {
		dev_err(&client->dev,"Probe: Unable to register %s input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	}
	ts->bad_data = 0;
//	finger_list.length = 0;


	if (client->irq) {
		//ret = request_irq(client->irq, goodix_ts_irq_handler, irq_table[ts->int_trigger_type], client->name, ts);
		ret = request_irq(
		client->irq, goodix_ts_irq_handler, IRQ_TYPE_EDGE_FALLING, client->name, ts);
		if (ret == 0)
		{
			disable_irq(client->irq);
//#ifndef STOP_IRQ_TYPE
//		gt818_irq_disable(ts);     //KT ADD 1202
//#elif
//#else
//		disable_irq(client->irq);
//#endif
			ts->use_irq = 1;
		}
	}

	if (!ts->use_irq) 
	{
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = goodix_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}
	
	if(ts->use_irq)
		enable_irq(client->irq);
//#ifndef STOP_IRQ_TYPE
//		gt818_irq_enable(ts);     //KT ADD 1202
//#elif
//#else
//		enable_irq(client->irq);
//#endif
		
	ts->power = goodix_ts_power;

	goodix_read_version(ts);

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = goodix_ts_early_suspend;
	ts->early_suspend.resume = goodix_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif	

	i2c_end_cmd(ts);

#if defined(CONFIG_TOUCHSCREEN_VIRTUAL_KEYS)
	ts_key_report_init();
#endif
	dir = proc_mkdir("touchscreen", NULL);
	refresh = create_proc_entry("ts_information", 0777, dir);
	if (refresh) {
		refresh->data		= NULL;
		refresh->read_proc  = proc_read_val;
		refresh->write_proc = proc_write_val;
	}
	


	dev_info(&client->dev,"Start %s in %s mode\n", 
		ts->input_dev->name, ts->use_irq ? "interrupt" : "polling");



	return 0;

err_init_godix_ts:
	i2c_end_cmd(ts);
	if(ts->use_irq)
	{
		ts->use_irq = 0;
		free_irq(client->irq,ts);
	#ifdef GPIO_TS_INT	
		gpio_direction_input(GPIO_TS_INT);
		gpio_free(GPIO_TS_INT);
	#endif	
	}
	else 
		hrtimer_cancel(&ts->timer);

err_input_register_device_failed:
	input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
	i2c_set_clientdata(client, NULL);
err_i2c_failed:
	gpio_free(SHUTDOWN_PORT);
power_port_failed:
//config_power_port_err:
//config_reset_port_err:
//	gpio_free(GPIO_TS_RESET);
//reset_port_failed:
//config_int_port_err:
	gpio_free(GPIO_TS_INT);
int_port_failed:
	kfree(ts);
err_alloc_data_failed:
err_check_functionality_failed:
err_create_proc_entry:
	printk("%s exit\n", __func__);
	return ret;
//gt818_iic_failed:
//	return -1;	
}

static int goodix_ts_remove(struct i2c_client *client)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

#if defined(CONFIG_TOUCHSCREEN_VIRTUAL_KEYS)
	ts_key_report_deinit();
#endif

	
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif
#ifdef CONFIG_TOUCHSCREEN_GOODIX_IAP
	remove_proc_entry("goodix-update", NULL);
#endif

#ifdef CREATE_WR_NODE
    uninit_wr_node();
#endif

	if (ts && ts->use_irq) 
	{
	#ifdef GPIO_TS_INT
		gpio_direction_input(GPIO_TS_INT);
		gpio_free(GPIO_TS_INT);
	#endif	
		free_irq(client->irq, ts);
	}	
	else if(ts)
		hrtimer_cancel(&ts->timer);
	
	dev_notice(&client->dev,"The driver is removing...\n");
	i2c_set_clientdata(client, NULL);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	return 0;
}

static int goodix_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts->use_irq)
		//disable_irq(client->irq);
//#ifndef STOP_IRQ_TYPE
//		gt818_irq_disable(ts);     //KT ADD 1202
//#else //#elif
		disable_irq(client->irq);
//#endif
	else
		hrtimer_cancel(&ts->timer);
//#ifndef STOP_IRQ_TYPE
		cancel_work_sync(&ts->work);
//#endif
	if (ts->power) {
		ret = ts->power(ts, 0);
		if (ret < 0)
			printk(KERN_ERR "goodix_ts_resume power off failed\n");
	}
	
	return 0;
}

static int goodix_ts_resume(struct i2c_client *client)
{
	int ret;
//	int retry=0;

	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts->power) {
		ret = ts->power(ts, 1);
		if (ret < 0)
			printk(KERN_ERR "goodix_ts_resume power on failed\n");
	}

	if (ts->use_irq)
		//enable_irq(client->irq);
//#ifndef STOP_IRQ_TYPE
//		gt818_irq_enable(ts);     //KT ADD 1202
//#else //#elif
		enable_irq(client->irq);
//#endif
	else
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void goodix_ts_early_suspend(struct early_suspend *h)
{
	struct goodix_ts_data *ts;
	ts = container_of(h, struct goodix_ts_data, early_suspend);
	goodix_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void goodix_ts_late_resume(struct early_suspend *h)
{
	struct goodix_ts_data *ts;
	ts = container_of(h, struct goodix_ts_data, early_suspend);
	goodix_ts_resume(ts->client);
}
#endif

//******************************Begin of firmware update surpport*******************************
#ifdef CONFIG_TOUCHSCREEN_GOODIX_IAP

static struct file * update_file_open(char * path, mm_segment_t * old_fs_p)
{
	struct file * filp = NULL;
	int errno = -1;
		
	filp = filp_open(path, O_RDONLY, 0644);
	
	if(!filp || IS_ERR(filp))
	{
		if(!filp)
			errno = -ENOENT;
		else 
			errno = PTR_ERR(filp);					
		printk(KERN_ERR "The update file for Guitar open error.\n");
		return NULL;
	}
	*old_fs_p = get_fs();
	set_fs(get_ds());

	filp->f_op->llseek(filp,0,0);
	return filp ;
}

static void update_file_close(struct file * filp, mm_segment_t old_fs)
{
	set_fs(old_fs);
	if(filp)
		filp_close(filp, NULL);
}
static int update_get_flen(char * path)
{
	struct file * file_ck = NULL;
	mm_segment_t old_fs;
	int length ;
	
	file_ck = update_file_open(path, &old_fs);
	if(file_ck == NULL)
		return 0;

	length = file_ck->f_op->llseek(file_ck, 0, SEEK_END);
	//printk("File length: %d\n", length);
	if(length < 0)
		length = 0;
	update_file_close(file_ck, old_fs);
	return length;	
}
#if 0 //xiayc
static int goodix_update_write(struct file *filp, const char __user *buff, unsigned long len, void *data)
{
	unsigned char cmd[120];
	int ret = -1;
       int retry = 0;
	static unsigned char update_path[60];
	struct goodix_ts_data *ts;
	
	ts = i2c_get_clientdata(i2c_connect_client);
	if(ts==NULL)
	{
            printk(KERN_INFO"goodix write to kernel via proc file!@@@@@@\n");
		return 0;
	}
	
	//printk(KERN_INFO"goodix write to kernel via proc file!@@@@@@\n");
	if(copy_from_user(&cmd, buff, len))
	{
            printk(KERN_INFO"goodix write to kernel via proc file!@@@@@@\n");
		return -EFAULT;
	}
	//printk(KERN_INFO"Write cmd is:%d,write len is:%ld\n",cmd[0], len);
	switch(cmd[0])
	{
            case APK_UPDATE_TP:
            printk(KERN_INFO"Write cmd is:%d,cmd arg is:%s,write len is:%ld\n",cmd[0], &cmd[1], len);
            memset(update_path, 0, 60);
            strncpy(update_path, cmd+1, 60);
 #ifndef STOP_IRQ_TYPE
		gt818_irq_disable(ts);     //KT ADD 1202
#else //#elif
		disable_irq(ts->client->irq);
#endif           
            ret = gt818_downloader( ts, goodix_gt818_firmware, update_path);

             if(ret < 0)
            {
                printk(KERN_INFO"Warnning: GT818 update might be ERROR!\n");
                return 0;
            }
             
            i2c_pre_cmd(ts);
	     msleep(2);
	
	    for(retry=0; retry<3; retry++)
	    {
		    ret=goodix_init_panel(ts);
		    printk(KERN_INFO"the config ret is :%d\n",ret);
            
		    msleep(2);
		    if(ret != 0)	//Initiall failed
			    continue;
		    else
			    break;
	    }
/*      gpio_tlmm_config(GPIO_CFG(GPIO_TS_INT,0,GPIO_INPUT,GPIO_NO_PULL,GPIO_2MA), GPIO_ENABLE);
			printk(KERN_INFO "config ts int pin failed \n");*/
           //if(ts->use_irq) 
          //      s3c_gpio_cfgpin(INT_PORT, INT_CFG);	//Set IO port as interrupt port	
	    //else 
            //    gpio_direction_input(INT_PORT);
#ifndef STOP_IRQ_TYPE
		gt818_irq_enable(ts);     //KT ADD 1202
#else	//#elif
		enable_irq(ts->client->irq);
#endif                 
        i2c_end_cmd(ts);

        if(ret != 0) 
        {
            ts->bad_data=1;
            return 1;
        }
        return 1;
    
    case APK_READ_FUN:							//functional command
		if(cmd[1] == CMD_READ_VER)
		{
			printk(KERN_INFO"Read version!\n");
			ts->read_mode = MODE_RD_VER;
		}
        else if(cmd[1] == CMD_READ_CFG)
		{
			printk(KERN_INFO"Read config info!\n");

			ts->read_mode = MODE_RD_CFG;
		}
		else if (cmd[1] == CMD_READ_RAW)
		{
		    printk(KERN_INFO"Read raw data!\n");

			ts->read_mode = MODE_RD_RAW;
		}
        else if (cmd[1] == CMD_READ_CHIP_TYPE)
		{
		    printk(KERN_INFO"Read chip type!\n");

			ts->read_mode = MODE_RD_CHIP_TYPE;
		}
        return 1;
        
    case APK_WRITE_CFG:			
		printk(KERN_INFO"Begin write config info!Config length:%d\n",cmd[1]);
		i2c_pre_cmd(ts);
        ret = i2c_write_bytes(ts->client, cmd+2, cmd[1]+2); 
        i2c_end_cmd(ts);
        if(ret != 1)
        {
            printk("Write Config failed!return:%d\n",ret);
            return -1;
        }
        return 1;
            
    default:
	    return 0;
	}
	return 0;
}
static int goodix_update_read( char *page, char **start, off_t off, int count, int *eof, void *data )
{
//    int i;
	int ret = -1;
    int len = 0;
//    int read_times = 0;
	struct goodix_ts_data *ts;

	unsigned char read_data[360] = {80, };

	ts = i2c_get_clientdata(i2c_connect_client);
	if(ts==NULL)
		return 0;
    
       printk("___READ__\n");
	if(ts->read_mode == MODE_RD_VER)		//read version data
	{
		i2c_pre_cmd(ts);
		ret = goodix_read_version(ts);
             i2c_end_cmd(ts);
		if(ret <= 0)
		{
			printk(KERN_INFO"Read version data failed!\n");
			return 0;
		}
        
             read_data[1] = (char)(ts->version&0xff);
             read_data[0] = (char)((ts->version>>8)&0xff);

		printk(KERN_INFO"Gt818 ROM version is:%x%x\n", read_data[0],read_data[1]);
		memcpy(page, read_data, 2);
		//*eof = 1;
		return 2;
	}
    else if (ts->read_mode == MODE_RD_CHIP_TYPE)
    {
        page[0] = -1;
        return 1;
    }
    else if(ts->read_mode == MODE_RD_CFG)
	{

            read_data[0] = 0x06;
            read_data[1] = 0xa2;       // cfg start address
            printk("read config addr is:%x,%x\n", read_data[0],read_data[1]);

	     len = 106;
           i2c_pre_cmd(ts);
	     ret = i2c_read_bytes(ts->client, read_data, len+2);
            i2c_end_cmd(ts);
            if(ret <= 0)
		{
			printk(KERN_INFO"Read config info failed!\n");
			return 0;
		}
              
		memcpy(page, read_data+2, len);
		return len;
	}
	
}             
#endif

//********************************************************************************************
static u8  is_equal( u8 *src , u8 *dst , int len )
{
    int i;
    
   // for( i = 0 ; i < len ; i++ )
  //  {
        //printk(KERN_INFO"[%02X:%02X]\n", src[i], dst[i]);
  //  }

    for( i = 0 ; i < len ; i++ )
    {
        if ( src[i] != dst[i] )
        {
            return 0;
        }
    }
    
    return 1;
}

static  u8 gt818_nvram_store( struct goodix_ts_data *ts )
{
    int ret;
    int i;
    u8 inbuf[3] = {REG_NVRCS_H,REG_NVRCS_L,0};
    //u8 outbuf[3] = {};
    ret = i2c_read_bytes( ts->client, inbuf, 3 );
    
    if ( ret < 0 )
    {
        return 0;
    }
    
    if ( ( inbuf[2] & BIT_NVRAM_LOCK ) == BIT_NVRAM_LOCK )
    {
        return 0;
    }
    
    inbuf[2] = (1<<BIT_NVRAM_STROE);		//store command
	    
    for ( i = 0 ; i < 300 ; i++ )
    {
        ret = i2c_write_bytes( ts->client, inbuf, 3 );
        
        if ( ret < 0 )
            break;
    }
    
    return ret;
}

static u8  gt818_nvram_recall( struct goodix_ts_data *ts )
{
    int ret;
    u8 inbuf[3] = {REG_NVRCS_H,REG_NVRCS_L,0};
    
    ret = i2c_read_bytes( ts->client, inbuf, 3 );
    
    if ( ret < 0 )
    {
        return 0;
    }
    
    if ( ( inbuf[2]&BIT_NVRAM_LOCK) == BIT_NVRAM_LOCK )
    {
        return 0;
    }
    
    inbuf[2] = ( 1 << BIT_NVRAM_RECALL );		//recall command
    ret = i2c_write_bytes( ts->client , inbuf, 3);
    return ret;
}

static  int gt818_reset( struct goodix_ts_data *ts )
{
    int ret = 1;
    u8 retry;
    
    unsigned char outbuf[3] = {0,0xff,0};
    unsigned char inbuf[3] = {0,0xff,0};
    //outbuf[1] = 1;

/*	if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_RESET,0,GPIO_OUTPUT,GPIO_NO_PULL,GPIO_2MA),GPIO_ENABLE)) {
		printk(KERN_INFO "config ts reset pin input failed \n");

	}*/
//	gpio_set_value(GPIO_TS_RESET,0);
//	msleep(20);
/*	if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_RESET,0,GPIO_INPUT,GPIO_NO_PULL,GPIO_2MA),GPIO_ENABLE)) {
		printk(KERN_INFO "config ts reset pin input failed \n");
	}*/
    msleep(100);
    for(retry=0;retry < 80; retry++)
    {
        ret =i2c_write_bytes(ts->client, inbuf, 0);	//Test I2C connection.
        if (ret > 0)
        {
            msleep(10);
            ret =i2c_read_bytes(ts->client, inbuf, 3);	//Test I2C connection.
            if (ret > 0)
            {
                if(inbuf[2] == 0x55)
                    {
			    ret =i2c_write_bytes(ts->client, outbuf, 3);
			    msleep(10);
			    break;						
			}
				}			
			}	
		
		}
    printk(KERN_INFO"Detect address %0X\n", ts->client->addr);
    //msleep(500);
    return ret;	
}

static  int gt818_reset2( struct goodix_ts_data *ts )
{
    int ret = 1;
    u8 retry;
    
//    unsigned char outbuf[3] = {0,0xff,0};
    unsigned char inbuf[3] = {0,0xff,0};
    //outbuf[1] = 1;

/*	if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_RESET,0,GPIO_OUTPUT,GPIO_NO_PULL,GPIO_2MA),GPIO_ENABLE)) {
		printk(KERN_INFO "config ts reset pin input failed \n");

	}*/
//	gpio_set_value(GPIO_TS_RESET,0);
//	msleep(20);
/*	if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_RESET,0,GPIO_INPUT,GPIO_NO_PULL,GPIO_2MA),GPIO_ENABLE)) {
		printk(KERN_INFO "config ts reset pin input failed \n");
	}*/
//    msleep(100);
    for(retry=0;retry < 80; retry++)
	{
		ret =i2c_write_bytes(ts->client, inbuf, 0);	//Test I2C connection.
		if (ret > 0)
		{
			break;						
		}	
		
	}
    printk(KERN_INFO"Detect address %0X\n", ts->client->addr);
    //msleep(500);
    return ret;	
}



static  int gt818_set_address_2( struct goodix_ts_data *ts )
{
    unsigned char inbuf[3] = {0,0,0};
    int i;

    for ( i = 0 ; i < 12 ; i++ )
    {
        if ( i2c_read_bytes( ts->client, inbuf, 3) )
        {
            printk(KERN_INFO"Got response\n");
            return 1;
        }
        printk(KERN_INFO"wait for retry\n");
        msleep(50);
    } 
    return 0;
}

static u8  gt818_update_firmware( u8 *nvram, u16 length, struct goodix_ts_data *ts)
{
    u8 ret,err,retry_time,i;
    u16 cur_code_addr;
    u16 cur_frame_num, total_frame_num, cur_frame_len;
    u32 gt80x_update_rate;

    unsigned char i2c_data_buf[PACK_SIZE+2] = {0,};        //\u02fd�\u077b병\u7238
    unsigned char i2c_chk_data_buf[PACK_SIZE+2] = {0,};        //\u02fd�\u077b병\u7238
    
    if( length > NVRAM_LEN - NVRAM_BOOT_SECTOR_LEN )
    {
        printk(KERN_INFO"length too big %d %d\n", length, NVRAM_LEN - NVRAM_BOOT_SECTOR_LEN );
        return 0;
    }
    	
    total_frame_num = ( length + PACK_SIZE - 1) / PACK_SIZE;  

    //gt80x_update_sta = _UPDATING;
    gt80x_update_rate = 0;

    for( cur_frame_num = 0 ; cur_frame_num < total_frame_num ; cur_frame_num++ )	  
    {
        retry_time = 5;
        
        cur_code_addr = NVRAM_UPDATE_START_ADDR + cur_frame_num * PACK_SIZE; 	
        i2c_data_buf[0] = (cur_code_addr>>8)&0xff;
        i2c_data_buf[1] = cur_code_addr&0xff;
        
        i2c_chk_data_buf[0] = i2c_data_buf[0];
        i2c_chk_data_buf[1] = i2c_data_buf[1];
        
        if( cur_frame_num == total_frame_num - 1 )
        {
            cur_frame_len = length - cur_frame_num * PACK_SIZE;
        }
        else
        {
            cur_frame_len = PACK_SIZE;
        }
        
        //strncpy(&i2c_data_buf[2], &nvram[cur_frame_num*PACK_SIZE], cur_frame_len);
        for(i=0;i<cur_frame_len;i++)
        {
            i2c_data_buf[2+i] = nvram[cur_frame_num*PACK_SIZE+i];
		}
        do
        {
            err = 0;

            //ret = gt818_i2c_write( guitar_i2c_address, cur_code_addr, &nvram[cur_frame_num*I2C_FRAME_MAX_LENGTH], cur_frame_len );		
            ret = i2c_write_bytes(ts->client, i2c_data_buf, (cur_frame_len+2));
            
            if ( ret <= 0 )
            {
                printk(KERN_INFO"write fail\n");
                err = 1;
            }
            
            ret = i2c_read_bytes(ts->client, i2c_chk_data_buf, (cur_frame_len+2));
           // ret = gt818_i2c_read( guitar_i2c_address, cur_code_addr, inbuf, cur_frame_len);
            
            if ( ret <= 0 )
            {
                printk(KERN_INFO"read fail\n");
                err = 1;
            }
            
            if( is_equal( &i2c_data_buf[2], &i2c_chk_data_buf[2], cur_frame_len ) == 0 )
            {
                printk(KERN_INFO"not equal\n");
                err = 1;
            }
			
        } while ( err == 1 && (--retry_time) > 0 );
        
        if( err == 1 )
        {
            break;
        }
		
        gt80x_update_rate = ( cur_frame_num + 1 )*128/total_frame_num;
    
    }

    if( err == 1 )
    {
        printk(KERN_INFO"write nvram fail\n");
        return 0;
    }
    
    ret = gt818_nvram_store(ts);
    
    msleep( 20 );

    if( ret == 0 )
    {
        printk(KERN_INFO"nvram store fail\n");
        return 0;
    }
    
    ret = gt818_nvram_recall(ts);

    msleep( 20 );
    
    if( ret == 0 )
    {
        printk(KERN_INFO"nvram recall fail\n");
        return 0;
    }

    for ( cur_frame_num = 0 ; cur_frame_num < total_frame_num ; cur_frame_num++ )		 //	read out all the code
    {

        cur_code_addr = NVRAM_UPDATE_START_ADDR + cur_frame_num*PACK_SIZE;
        retry_time=5;
        i2c_chk_data_buf[0] = (cur_code_addr>>8)&0xff;
        i2c_chk_data_buf[1] = cur_code_addr&0xff;
        
        
        if ( cur_frame_num == total_frame_num-1 )
        {
            cur_frame_len = length - cur_frame_num*PACK_SIZE;
        }
        else
        {
            cur_frame_len = PACK_SIZE;
        }
        
        do
        {
            err = 0;
            //ret = gt818_i2c_read( guitar_i2c_address, cur_code_addr, inbuf, cur_frame_len);
            ret = i2c_read_bytes(ts->client, i2c_chk_data_buf, (cur_frame_len+2));

            if ( ret == 0 )
            {
                err = 1;
            }
            
            if( is_equal( &nvram[cur_frame_num*PACK_SIZE], &i2c_chk_data_buf[2], cur_frame_len ) == 0 )
            {
                err = 1;
            }
        } while ( err == 1 && (--retry_time) > 0 );
        
        if( err == 1 )
        {
            break;
        }
        
        gt80x_update_rate = 127 + ( cur_frame_num + 1 )*128/total_frame_num;
    }
    
    gt80x_update_rate = 255;
    //gt80x_update_sta = _UPDATECHKCODE;

    if( err == 1 )
    {
        printk(KERN_INFO"nvram validate fail\n");
        return 0;
    }
    
    return 1;
}

static u8  gt818_update_proc( u8 *nvram, u16 length, struct goodix_ts_data *ts )
{
    u8 ret;
    u8 error = 0;
    //struct tpd_info_t tpd_info;
//    GT818_SET_INT_PIN( 0 );
/*	if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_INT,0,GPIO_OUTPUT,GPIO_PULL_UP,GPIO_2MA), GPIO_ENABLE)) {
		printk(KERN_INFO "config ts int pin failed \n");
	}
	*/
	gpio_set_value(GPIO_TS_INT, 0);

    msleep( 20 );
    ret = gt818_reset(ts);
    if ( ret < 0 )
    {
        error = 1;
        printk(KERN_INFO"reset fail\n");
        goto end;
    }

    ret = gt818_set_address_2( ts );
    if ( ret == 0 )
    {
        error = 1;
        printk(KERN_INFO"set address fail\n");
        goto end;
    }

    ret = gt818_update_firmware( nvram, length, ts);
    if ( ret == 0 )
    {
        error=1;
        printk(KERN_INFO"firmware update fail\n");
        goto end;
    }

end:
//    GT818_SET_INT_PIN( 1 );
/*	if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_INT,0,GPIO_OUTPUT,GPIO_PULL_UP,GPIO_2MA), GPIO_ENABLE)) {
		printk(KERN_INFO "config ts int pin failed \n");
	}*/
	gpio_set_value(GPIO_TS_INT, 1);
//	gpio_free(GPIO_TS_INT);
/*	if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_INT,0,GPIO_INPUT,GPIO_NO_PULL,GPIO_2MA), GPIO_ENABLE)) {
		printk(KERN_INFO "config ts int pin failed \n");
 	}
  */  
    msleep( 500 );
    ret = gt818_reset2(ts);
    if ( ret < 0 )
    {
        error=1;
        printk(KERN_INFO"final reset fail\n");
        goto end;
    }
    if ( error == 1 )
    {
        return 0; 
    }

    i2c_pre_cmd(ts);
    while(goodix_read_version(ts)<0);
    
    i2c_end_cmd(ts);
    return 1;
}

static int  gt818_downloader( struct goodix_ts_data *ts,  unsigned char * data, unsigned char * path)
{
    struct tpd_firmware_info_t *fw_info = (struct tpd_firmware_info_t *)data;
    int i;
    unsigned short checksum = 0;
    unsigned char *data_ptr = &(fw_info->data);
    int retry = 0,ret;
    int err = 0;

    struct file * file_data = NULL;
    mm_segment_t old_fs;
    //unsigned int rd_len;
    unsigned int file_len = 0;
    //unsigned char i2c_data_buf[PACK_SIZE] = {0,};
    
    const int MAGIC_NUMBER_1 = 0x4D454449;
    const int MAGIC_NUMBER_2 = 0x4154454B;
    
    if(path[0] == 0)
    {
        printk(KERN_INFO"%s\n", __func__ );
        printk(KERN_INFO"magic number 0x%08X 0x%08X\n", fw_info->magic_number_1, fw_info->magic_number_2 );
        printk(KERN_INFO"magic number 0x%08X 0x%08X\n", MAGIC_NUMBER_1, MAGIC_NUMBER_2 );
        printk(KERN_INFO"current version 0x%04X, target verion 0x%04X\n", ts->version, fw_info->version );
        printk(KERN_INFO"size %d\n", fw_info->length );
        printk(KERN_INFO"checksum %d\n", fw_info->checksum );

        if ( fw_info->magic_number_1 != MAGIC_NUMBER_1 && fw_info->magic_number_2 != MAGIC_NUMBER_2 )
        {
            printk(KERN_INFO"Magic number not match\n");
            err = 0;
            goto exit_downloader;
        }
		
	if( ((ts->version&0xff) > (fw_info->version&0xff)) )   // current low byte higher than back-up low byte
		{
		
        			printk(KERN_INFO"No need to upgrade\n");
        			goto exit_downloader;			 
		}
	else if(((ts->version&0xff) == (fw_info->version&0xff)))
		{
			if(((ts->version&0xff00) < (fw_info->version&0xff00)))
				{
					printk(KERN_INFO"Need to upgrade\n");
				}
			else
				{
					printk(KERN_INFO"No need to upgrade\n");
        				goto exit_downloader;
				}
		}
	else
		{
			printk(KERN_INFO"Need to upgrade\n");
		}

        if ( get_chip_version( ts->version ) != get_chip_version( fw_info->version ) )
            {
               	 printk(KERN_INFO"Chip version incorrect");
                	err = 0;
               	 goto exit_downloader;        
            }
//update_start:
        for ( i = 0 ; i < fw_info->length ; i++ )
            checksum += data_ptr[i];

        checksum = checksum%0xFFFF;

        if ( checksum != fw_info->checksum )
        {
            printk(KERN_INFO"Checksum not match 0x%04X\n", checksum);
            err = 0;
            goto exit_downloader;
        }
    }
    else
    {
        printk(KERN_INFO"Write cmd arg is:\n");
        file_data = update_file_open(path, &old_fs);
        printk(KERN_INFO"Write cmd arg is\n");
        if(file_data == NULL)	//file_data has been opened at the last time
        {
            err = -1;
            goto exit_downloader;
        }
    

    file_len = (update_get_flen(path))-2;

        printk(KERN_INFO"current length:%d\n", file_len);

            ret = file_data->f_op->read(file_data, &data_ptr[0], file_len, &file_data->f_pos);
            
            if(ret <= 0)
            {
               err = -1;
                goto exit_downloader;
            }
            update_file_close(file_data, old_fs);   
        }

    printk(KERN_INFO"STEP_1:\n");
    err = -1;
    while (  retry < 3 ) 
        {
            ret = gt818_update_proc( data_ptr, fw_info->length, ts);
            if(ret == 1)
            {
                err = 1;
                break;
            }
            retry++;
    }
    
exit_downloader:
	
//         gpio_free(GPIO_TS_INT);
/*	if(gpio_tlmm_config(GPIO_CFG(GPIO_TS_INT,0,GPIO_INPUT,GPIO_NO_PULL,GPIO_2MA), GPIO_ENABLE)) {
		printk(KERN_INFO "config ts int pin failed \n");
 	}*/
    return err;

}

#endif

//******************************End of firmware update surpport*******************************



//only one client
static const struct i2c_device_id goodix_ts_id[] = {
	{ GOODIX_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver goodix_ts_driver = {
	.probe		= goodix_ts_probe,
	.remove		= goodix_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= goodix_ts_suspend,
	.resume		= goodix_ts_resume,
#endif
	.id_table	= goodix_ts_id,
	.driver = {
		.name	= GOODIX_I2C_NAME,
		.owner = THIS_MODULE,
	},
};

static int __devinit goodix_ts_init(void)
{
	int ret;
	ret=i2c_add_driver(&goodix_ts_driver);
	return ret; 
}

static void __exit goodix_ts_exit(void)
{
#if 0
	printk(KERN_ALERT "Touchscreen driver of guitar exited.\n");
#endif
	i2c_del_driver(&goodix_ts_driver);
	if (goodix_wq)
		destroy_workqueue(goodix_wq);		//release our work queue
}

late_initcall(goodix_ts_init);				
module_exit(goodix_ts_exit);

MODULE_DESCRIPTION("Goodix Touchscreen Driver");
MODULE_LICENSE("GPL");
