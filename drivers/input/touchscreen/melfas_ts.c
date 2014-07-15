//--------------------------------------------------------
//
//
//	Melfas MCS8000 Series Download base v1.0 2011.08.26
//
//
//--------------------------------------------------------

/* drivers/input/touchscreen/melfas_ts.c
 *
 * Copyright (C) 2010 Melfas, Inc.
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/melfas_ts.h>
#include <mach/gpio.h>
#include <linux/proc_fs.h>


#define MELFAS_MAX_TOUCH       2
#define FW_VERSION          0x00 //Check Version
#define HW_VERSION			    0x00 //Check Version

#define TS_MAX_X_COORD      320 //Check resolution
#define TS_MAX_Y_COORD      240 //Check resolution
#define TS_MAX_Z_TOUCH      255
#define TS_MAX_W_TOUCH      30

#define TS_READ_EVENT_PACKET_SIZE    0x0F
#define TS_READ_START_ADDR 	         0x10
#define TS_READ_VERSION_ADDR         0x20 //Start read H/W, S/W Version
#define TS_READ_REGS_LEN             14

#define TOUCH_TYPE_NONE       0
#define TOUCH_TYPE_SCREEN     1
#define TOUCH_TYPE_KEY        2

#define I2C_RETRY_CNT			10

#define SET_DOWNLOAD_BY_GPIO	0

#define PRESS_KEY				1
#define RELEASE_KEY		  0
#define DEBUG_PRINT 		0

//#if SET_DOWNLOAD_BY_GPIO
#include <melfas_download.h>
//#endif // SET_DOWNLOAD_BY_GPIO

struct muti_touch_info
{
	int strength;
	int width;	
	int posX;
	int posY;
};



#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h);
static void melfas_ts_late_resume(struct early_suspend *h);
#endif

static struct muti_touch_info g_Mtouch_info[MELFAS_MAX_TOUCH];

static int melfas_init_panel(struct melfas_ts_data *ts)
{
    uint8_t buf[1] = {0x00};
    i2c_master_send(ts->client, buf, 1);
    return true;
}
#define TOUCH_OFF 0
#define FIST_TOUCH 1
#define SECOND_TOUCH 2
#define DUAL_TOUCH 3

static void melfas_ts_work_func(struct work_struct *work)
{
	struct melfas_ts_data *ts = container_of(work, struct melfas_ts_data, work);
	int ret = 0, i;
	uint8_t buf[TS_READ_REGS_LEN];
  
  uint8_t stMasterPositionC;
  int16_t stMasterPositionX;
  int16_t stMasterPositionY;
  uint8_t stMasterPositionZ;
  int16_t stMasterPositionX_2nd;
  int16_t stMasterPositionY_2nd;
  uint8_t stMasterPositionZ_2nd;
  
  uint8_t stMasterPositionKeyIdx;
  uint8_t stMasterPositionKeyState;  

#if DEBUG_PRINT
	printk(KERN_ERR "melfas_ts_work_func\n");

	if(ts ==NULL)
			printk(KERN_ERR "melfas_ts_work_func : TS NULL\n");
#endif 

	/**
	Simple send transaction:
		S Addr Wr [A]  Data [A] Data [A] ... [A] Data [A] P
	Simple recv transaction:
		S Addr Rd [A]  [Data] A [Data] A ... A [Data] NA P
	*/

	//buf[0]=TS_READ_START_ADDR;
	for(i=0; i<I2C_RETRY_CNT; i++)
	{
		//ret = i2c_master_send(ts->client, buf, 1);
        //if (ret >= 0)
        {
			//ret = i2c_master_recv(ts->client, buf, TS_READ_REGS_LEN);
			ret = i2c_smbus_read_i2c_block_data(ts->client, TS_READ_START_ADDR, TS_READ_REGS_LEN, buf);
#if DEBUG_PRINT			
			printk(KERN_ERR "melfas_ts_work_func : i2c_smbus_read_i2c_block_data [%d]\n", ret);			
#endif


			if(ret >=0)
			{
				break; // i2c success
			}
        }
	}

	if (ret < 0)
	{
		printk(KERN_ERR "melfas_ts_work_func: i2c failed\n");
		return ;	
	}
	else 
	{

    stMasterPositionC = buf[0] & 0x07;
    stMasterPositionX = buf[1]<<8;
    stMasterPositionX |= buf[2];
    stMasterPositionY = buf[3]<<8;
    stMasterPositionY |= buf[4];
    stMasterPositionZ = buf[5];

    stMasterPositionX_2nd = buf[9]<<8;
    stMasterPositionX_2nd |= buf[10];
    stMasterPositionY_2nd = buf[11]<<8;
    stMasterPositionY_2nd |= buf[12];
	stMasterPositionZ_2nd = buf[13];
	
	stMasterPositionKeyIdx = (buf[8]>>2)&0x03;
	stMasterPositionKeyState = buf[8]&0x03;

    switch(stMasterPositionC)
    {
      case TOUCH_OFF:
        g_Mtouch_info[0].strength = 0;
        g_Mtouch_info[1].strength = 0;
      break;

      case FIST_TOUCH:
        g_Mtouch_info[0].strength = stMasterPositionZ;
        g_Mtouch_info[1].strength = 0;
      break;

      case SECOND_TOUCH:
        g_Mtouch_info[0].strength = 0;
        g_Mtouch_info[1].strength = stMasterPositionZ_2nd;
      break;

      case DUAL_TOUCH:
        g_Mtouch_info[0].strength = stMasterPositionZ;
        g_Mtouch_info[1].strength = stMasterPositionZ_2nd;
      break;

      default:
        g_Mtouch_info[0].strength = 0;
        g_Mtouch_info[1].strength = 0;
      break;
    }
    
    g_Mtouch_info[0].posX= (int)stMasterPositionX;
    g_Mtouch_info[0].posY= (int)stMasterPositionY;
//    g_Mtouch_info[0].width= (int)width;			

    g_Mtouch_info[1].posX= (int)stMasterPositionX_2nd;
    g_Mtouch_info[1].posY= (int)stMasterPositionY_2nd;
//    g_Mtouch_info[1].width= (int)width;	


		if(stMasterPositionC > DUAL_TOUCH)
		{
#if DEBUG_PRINT
		printk(KERN_ERR "melfas_ts_work_func: stMasterPositionC: %d\n",  stMasterPositionC);
#endif			
			return ;
		}

    for(i=0; i<MELFAS_MAX_TOUCH; i++)
    {
      if(g_Mtouch_info[i].strength== -1)
      continue;

      input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, i);
      input_report_abs(ts->input_dev, ABS_MT_POSITION_X, g_Mtouch_info[i].posX);
      input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, g_Mtouch_info[i].posY);      
      input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, g_Mtouch_info[i].strength);
      input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, g_Mtouch_info[i].width); 

      input_mt_sync(ts->input_dev);      

#if DEBUG_PRINT
		printk(KERN_ERR "melfas_ts_work_func: Touch ID: %d, strength : %d, x: %d, y: %d, z: %d w: %d\n", 
			i, g_Mtouch_info[i].strength, g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].strength, g_Mtouch_info[i].width);
#endif					
      if(g_Mtouch_info[i].strength == 0)
      	g_Mtouch_info[i].strength = -1;
    }    
		input_sync(ts->input_dev);
    
	}
			
	enable_irq(ts->client->irq);
}

static irqreturn_t melfas_ts_irq_handler(int irq, void *handle)
{
    struct melfas_ts_data *ts = (struct melfas_ts_data *)handle;
#if DEBUG_PRINT
    printk(KERN_ERR "melfas_ts_irq_handler\n");
#endif

    disable_irq_nosync(ts->client->irq);
    schedule_work(&ts->work);

    return IRQ_HANDLED;
}
static struct melfas_ts_data *melfas_ts;
#ifdef CONFIG_TOUCHSCREEN_MELFAS_FWUPDATE
int update_result_flag_melfas=0;
#endif
static int
proc_read_val(char *page, char **start, off_t off, int count, int *eof,
			  void *data)
{
	//int ret=0;
	int len = 0,i,ret;
	uint8_t buf[2];
    //buf[0] = TS_READ_VERSION_ADDR;
    for (i = 0; i < I2C_RETRY_CNT; i++)
    {
        //ret = i2c_master_send(melfas_ts->client, buf, 1);
        //if (ret >= 0)
        {
            //ret = i2c_master_recv(ts->client, buf, 2);
			ret = i2c_smbus_read_i2c_block_data(melfas_ts->client, TS_READ_VERSION_ADDR, 2, buf);

            if (ret >= 0)
            {
                break; // i2c success
            }
        }
    }

	len += sprintf(page + len, "Manufacturer : %s\n", "melfas");
	len += sprintf(page + len, "hw version : 0x%x\n", buf[0]);
	len += sprintf(page + len, "sw version : 0x%x\n", buf[1]);
	#ifdef CONFIG_TOUCHSCREEN_MELFAS_FWUPDATE
	len += sprintf(page + len, "Update flag : 0x%x\n", update_result_flag_melfas);
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
	unsigned long val;
	//int ret;
	#ifdef CONFIG_TOUCHSCREEN_MELFAS_FWUPDATE
	update_result_flag_melfas=0;
	printk("MELFAS FW UPDATE START!\n");
    disable_irq_nosync(melfas_ts->client->irq);
	
	//free_irq(melfas_ts->client->irq,melfas_ts);
	mcsdl_download_binary_data(melfas_ts);
	//ret = request_irq(melfas_ts->client->irq, melfas_ts_irq_handler, IRQF_TRIGGER_FALLING, melfas_ts->client->name, melfas_ts);
	//if (ret > 0)
	//{
	//	printk(KERN_ERR "melfas_ts_probe: Can't allocate irq %d, ret %d\n", melfas_ts->client->irq, ret);
	//}
	
	enable_irq(melfas_ts->client->irq);
	update_result_flag_melfas=2;
	printk("MELFAS FW UPDATE finish!\n");
	#endif
	sscanf(buffer, "%lu", &val);
	
	return -EINVAL;
}

#define GPIO_TOUCH_EN_OUT  107
static int melfas_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct melfas_ts_data *ts;
    int ret = 0, i;

    uint8_t buf[2];
	struct proc_dir_entry *dir, *refresh;

#if DEBUG_PRINT
    printk(KERN_ERR "kim ms : melfas_ts_probe\n");
#endif
	ret = gpio_request(GPIO_TOUCH_EN_OUT, "touch voltage");
	if (ret)
	{	
		printk("%s: gpio %d request is error!\n", __func__, GPIO_TOUCH_EN_OUT);
		return ret;
	}   

	gpio_direction_output(GPIO_TOUCH_EN_OUT, 1);
	msleep(300);//xym
	printk("%s: GPIO_TOUCH_EN_OUT(%d) = %d\n", __func__, GPIO_TOUCH_EN_OUT, gpio_get_value(GPIO_TOUCH_EN_OUT));

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        printk(KERN_ERR "melfas_ts_probe: need I2C_FUNC_I2C\n");
        ret = -ENODEV;
        goto err_check_functionality_failed;
    }

    ts = kmalloc(sizeof(struct melfas_ts_data), GFP_KERNEL);
    if (ts == NULL)
    {
        printk(KERN_ERR "melfas_ts_probe: failed to create a state of melfas-ts\n");
        ret = -ENOMEM;
        goto err_alloc_data_failed;
    }

    INIT_WORK(&ts->work, melfas_ts_work_func);

    ts->client = client;
    i2c_set_clientdata(client, ts);
	melfas_ts=ts;

	//这个似乎是用来看触摸屏是否存在的
    ret = i2c_master_send(ts->client, buf, 1);

#if DEBUG_PRINT
    printk(KERN_ERR "melfas_ts_probe: i2c_master_send() [%d], Add[%d]\n", ret, ts->client->addr);
#endif
	if(ret<0)
	{
		goto err_detect_failed;
	}
		
#if SET_DOWNLOAD_BY_GPIO
    //buf[0] = TS_READ_VERSION_ADDR;
    for (i = 0; i < I2C_RETRY_CNT; i++)
    {
        //ret = i2c_master_send(ts->client, buf, 1);
        //if (ret >= 0)
        {
            //ret = i2c_master_recv(ts->client, buf, 2);
			ret = i2c_smbus_read_i2c_block_data(ts->client, TS_READ_VERSION_ADDR, 2, buf);

            if (ret >= 0)
            {
                break; // i2c success
            }
        }
    }

	if(i == I2C_RETRY_CNT) //VERSION READ Fail
	{
		 //ret = mcsdl_download_binary_file();
		ret = mcsdl_download_binary_data(ts);
	}
	else {
		if (buf[0] == HW_VERSION && buf[1] < FW_VERSION)
		{
			//ret = mcsdl_download_binary_file();
			ret = mcsdl_download_binary_data(ts);
		}
	}
#endif // SET_DOWNLOAD_BY_GPIO

    ts->input_dev = input_allocate_device();
    if (!ts->input_dev)
    {
        printk(KERN_ERR "melfas_ts_probe: Not enough memory\n");
        ret = -ENOMEM;
        goto err_input_dev_alloc_failed;
    }

    ts->input_dev->name = "melfas-touchscreen" ;

    ts->input_dev->evbit[0] = BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);

    ts->input_dev->keybit[BIT_WORD(KEY_MENU)] |= BIT_MASK(KEY_MENU);
    ts->input_dev->keybit[BIT_WORD(KEY_HOME)] |= BIT_MASK(KEY_HOME);
    ts->input_dev->keybit[BIT_WORD(KEY_BACK)] |= BIT_MASK(KEY_BACK);		
    ts->input_dev->keybit[BIT_WORD(KEY_SEARCH)] |= BIT_MASK(KEY_SEARCH);			

//	__set_bit(BTN_TOUCH, ts->input_dev->keybit);
//	__set_bit(EV_ABS,  ts->input_dev->evbit);
//	ts->input_dev->evbit[0] =  BIT_MASK(EV_SYN) | BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);

    input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, TS_MAX_X_COORD, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, TS_MAX_Y_COORD, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, TS_MAX_Z_TOUCH, 0, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, MELFAS_MAX_TOUCH - 1, 0, 0);
//    input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, TS_MAX_W_TOUCH, 0, 0);
//	__set_bit(EV_SYN, ts->input_dev->evbit);
//	__set_bit(EV_KEY, ts->input_dev->evbit);

    ret = input_register_device(ts->input_dev);
    if (ret)
    {
        printk(KERN_ERR "melfas_ts_probe: Failed to register device\n");
        ret = -ENOMEM;
        goto err_input_register_device_failed;
    }

    if (ts->client->irq)
    {
#if DEBUG_PRINT
        printk(KERN_ERR "melfas_ts_probe: trying to request irq: %s-%d\n", ts->client->name, ts->client->irq);
#endif
        ret = request_irq(client->irq, melfas_ts_irq_handler, IRQF_TRIGGER_FALLING, ts->client->name, ts);
        if (ret > 0)
        {
            printk(KERN_ERR "melfas_ts_probe: Can't allocate irq %d, ret %d\n", ts->client->irq, ret);
            ret = -EBUSY;
            goto err_request_irq;
        }
    }

    schedule_work(&ts->work);

	for (i = 0; i < MELFAS_MAX_TOUCH ; i++)  /* _SUPPORT_MULTITOUCH_ */
		g_Mtouch_info[i].strength = -1;	

#if DEBUG_PRINT
    printk(KERN_ERR "melfas_ts_probe: succeed to register input device\n");
#endif

#if CONFIG_HAS_EARLYSUSPEND
    ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    ts->early_suspend.suspend = melfas_ts_early_suspend;
    ts->early_suspend.resume = melfas_ts_late_resume;
    register_early_suspend(&ts->early_suspend);
#endif

dir = proc_mkdir("touchscreen", NULL);
  refresh = create_proc_entry("ts_information", 0664, dir);
  if (refresh) {
	  refresh->data 	  = NULL;
	  refresh->read_proc  = proc_read_val;
	  refresh->write_proc = proc_write_val;
  }

#if DEBUG_PRINT
    printk(KERN_INFO "melfas_ts_probe: Start touchscreen. name: %s, irq: %d\n", ts->client->name, ts->client->irq);
#endif
    return 0;

err_request_irq:
    printk(KERN_ERR "melfas-ts: err_request_irq failed\n");
    free_irq(client->irq, ts);
err_input_register_device_failed:
    printk(KERN_ERR "melfas-ts: err_input_register_device failed\n");
    input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
    printk(KERN_ERR "melfas-ts: err_input_dev_alloc failed\n");
err_alloc_data_failed:
    printk(KERN_ERR "melfas-ts: err_alloc_data failed_\n");
err_detect_failed:
	printk(KERN_ERR "melfas-ts: err_detect failed\n");
    kfree(ts);
err_check_functionality_failed:
    printk(KERN_ERR "melfas-ts: err_check_functionality failed_\n");
	gpio_free(GPIO_TOUCH_EN_OUT);
    return ret;
}

static int melfas_ts_remove(struct i2c_client *client)
{
    struct melfas_ts_data *ts = i2c_get_clientdata(client);

    unregister_early_suspend(&ts->early_suspend);
    free_irq(client->irq, ts);
    input_unregister_device(ts->input_dev);
    kfree(ts);
    return 0;
}

static int melfas_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
    int ret;
	int i;    
    struct melfas_ts_data *ts = i2c_get_clientdata(client);

	for (i = 0; i < MELFAS_MAX_TOUCH ; i++)
	{
		g_Mtouch_info[i].strength = -1;
		g_Mtouch_info[i].width = 0;
		g_Mtouch_info[i].posX= 0;
		g_Mtouch_info[i].posY= 0;
	}
    
    disable_irq(client->irq);

    ret = cancel_work_sync(&ts->work);
    if (ret) /* if work was pending disable-count is now 2 */
        enable_irq(client->irq);

//	ret = i2c_smbus_write_byte_data(client, 0x01, 0x00); /* deep sleep */
//	if (ret < 0)
//		printk(KERN_ERR "melfas_ts_suspend: i2c_smbus_write_byte_data failed\n");

    return 0;
}

static int melfas_ts_resume(struct i2c_client *client)
{
      struct melfas_ts_data *ts = i2c_get_clientdata(client);

      melfas_init_panel(ts);

      cancel_work_sync(&ts->work);      
      schedule_work(&ts->work);

      enable_irq(client->irq);

      return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h)
{
    struct melfas_ts_data *ts;
    ts = container_of(h, struct melfas_ts_data, early_suspend);
    melfas_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void melfas_ts_late_resume(struct early_suspend *h)
{
    struct melfas_ts_data *ts;
    ts = container_of(h, struct melfas_ts_data, early_suspend);
    melfas_ts_resume(ts->client);
}
#endif

static const struct i2c_device_id melfas_ts_id[] =
{
    { MELFAS_TS_NAME, 0 },
    { }
};

static struct i2c_driver melfas_ts_driver =
{
    .driver = {
    .name = MELFAS_TS_NAME,
    },
    .id_table = melfas_ts_id,
    .probe = melfas_ts_probe,
    .remove = __devexit_p(melfas_ts_remove),
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend = melfas_ts_suspend,
    .resume = melfas_ts_resume,
#endif
};

static int __devinit melfas_ts_init(void)
{
    return i2c_add_driver(&melfas_ts_driver);
}

static void __exit melfas_ts_exit(void)
{
    i2c_del_driver(&melfas_ts_driver);
}

MODULE_DESCRIPTION("Driver for Melfas TSI Touchscreen Controller");
MODULE_AUTHOR("SuHyun, Ahn <shahn@melfas.com>");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");

module_init(melfas_ts_init);
module_exit(melfas_ts_exit);
