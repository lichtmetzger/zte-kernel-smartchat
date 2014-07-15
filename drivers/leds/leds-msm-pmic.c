/*
 * leds-msm-pmic.c - MSM PMIC LEDs driver.
 *
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 
 * modified history
 
         when             who   what  where
     11/07/26	     zhang.yu_1 for led indication support, red and green
 */

#ifdef CONFIG_LEDS_MSM_PMIC
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/timer.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <mach/pmic.h>
#include <linux/gpio.h>

#define ZHY_BL_TAG "[ZHY@pmic-leds]"

#define MAX_PMIC_BL_LEVEL	16
#define BLINK_LED_NUM   2


#ifdef CONFIG_GPIO_19_20_CTRL_LED
#define GPIO_RED_LIGHT_CTRL        19
#define GPIO_GREEN_LIGHT_CTRL    20
#elif defined  CONFIG_GPIO_10_11_CTRL_LED
#define GPIO_RED_LIGHT_CTRL        10
#define GPIO_GREEN_LIGHT_CTRL    11
#endif

#ifdef CONFIG_GPIO_6_CTRL_BLUE_LED	//zte-ccb-20120322-0
#define GPIO_BLUE    6
#else
#if defined CONFIG_GPIO_19_20_CTRL_LED || defined CONFIG_GPIO_10_11_CTRL_LED
static unsigned data_leds_gpio[] = {
	GPIO_CFG(GPIO_RED_LIGHT_CTRL, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	/* red */
	GPIO_CFG(GPIO_GREEN_LIGHT_CTRL, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	/* green */
};
#endif
#endif	//CONFIG_GPIO_6_CTRL_BLUE_LED

struct BLINK_LED_data{
         int blink_flag;
	int blink_led_flag;  // 0: off, 1:0n
	int blink_on_time;  //ms
	int blink_off_time;  //ms
	struct timer_list timer;
         struct work_struct work_led_on;
         struct work_struct work_led_off;
	struct led_classdev led;
};

struct STATUS_LED_data {
	spinlock_t data_lock;
	struct BLINK_LED_data blink_led[2];  /* red, green */
};

struct STATUS_LED_data *STATUS_LED;

static int led_not_suspend_flag=1;	//chenchongbao.20120101 suspend when blink, the blink timer will control led via rpc, rpc will break suspend.

static void pmic_red_led_on(struct work_struct *work)
{
#ifndef CONFIG_GPIO_6_CTRL_BLUE_LED	//zte-ccb-20120322-0
       #if defined CONFIG_GPIO_19_20_CTRL_LED || defined CONFIG_GPIO_10_11_CTRL_LED
       //not use b_led
       #else
       struct BLINK_LED_data *b_led = container_of(work, struct BLINK_LED_data, work_led_on);
       #endif	   

       #if defined CONFIG_GPIO_19_20_CTRL_LED || defined CONFIG_GPIO_10_11_CTRL_LED
	gpio_direction_output(GPIO_RED_LIGHT_CTRL, 1);   
       #else 
	//pmic_set_led_intensity(LED_KEYPAD, b_led->led.brightness / MAX_PMIC_BL_LEVEL);
	pmic_secure_mpp_config_i_sink(PM_MPP_3, 
                         b_led->led.brightness / MAX_PMIC_BL_LEVEL, PM_MPP__I_SINK__SWITCH_ENA);
       #endif
#endif
}

static void pmic_red_led_off(struct work_struct *work)
{
#ifndef CONFIG_GPIO_6_CTRL_BLUE_LED	//zte-ccb-20120322-0
       #if defined CONFIG_GPIO_19_20_CTRL_LED || defined CONFIG_GPIO_10_11_CTRL_LED
	gpio_direction_output(GPIO_RED_LIGHT_CTRL, 0);   
       #else 
	//pmic_set_led_intensity(LED_KEYPAD, LED_OFF);
         pmic_secure_mpp_config_i_sink(PM_MPP_3, 0, PM_MPP__I_SINK__SWITCH_DIS);	
       #endif
#endif
}

static void pmic_green_led_on(struct work_struct *work)
{

#ifdef CONFIG_GPIO_6_CTRL_BLUE_LED	//zte-ccb-20120322-0

	gpio_direction_output(GPIO_BLUE, 1);   

#else

       #if defined  CONFIG_GPIO_19_20_CTRL_LED || defined CONFIG_GPIO_10_11_CTRL_LED
       //not use b_led
       #else
       struct BLINK_LED_data *b_led = container_of(work, struct BLINK_LED_data, work_led_on);
       #endif	
	   
       #if defined CONFIG_GPIO_19_20_CTRL_LED || defined CONFIG_GPIO_10_11_CTRL_LED
	gpio_direction_output(GPIO_GREEN_LIGHT_CTRL, 1);   
       #else 
	//pmic_set_led_intensity(LED_LCD, b_led->led.brightness / MAX_PMIC_BL_LEVEL);
	pmic_secure_mpp_config_i_sink(PM_MPP_8, 
                         b_led->led.brightness / MAX_PMIC_BL_LEVEL, PM_MPP__I_SINK__SWITCH_ENA);
	
       #endif	   

#endif

}

static void pmic_green_led_off(struct work_struct *work)
{
#ifdef CONFIG_GPIO_6_CTRL_BLUE_LED	//zte-ccb-20120322-0

	gpio_direction_output(GPIO_BLUE, 0);   

#else

       #if defined CONFIG_GPIO_19_20_CTRL_LED || defined CONFIG_GPIO_10_11_CTRL_LED
	gpio_direction_output(GPIO_GREEN_LIGHT_CTRL, 0);   
       #else 
	//pmic_set_led_intensity(LED_LCD, LED_OFF);
         pmic_secure_mpp_config_i_sink(PM_MPP_8, 0, PM_MPP__I_SINK__SWITCH_DIS);		
       #endif	
	   
#endif
}

static void (*func[4])(struct work_struct *work) = {
	pmic_red_led_on,
	pmic_red_led_off,
	pmic_green_led_on,
	pmic_green_led_off,
};

static void msm_pmic_bl_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	int ret=0;

#ifdef CONFIG_GPIO_6_CTRL_BLUE_LED	//zte-ccb-20120322-0

	if (!strcmp(led_cdev->name, "green")) 
	{
		if(value == LED_OFF)
		{
			ret = gpio_direction_output(GPIO_BLUE, 0);   
		}
		else
		{
			ret = gpio_direction_output(GPIO_BLUE, 1);   
		}
	}

#else
	
	if (!strcmp(led_cdev->name, "red")) 
	{
       		#if defined CONFIG_GPIO_19_20_CTRL_LED || defined CONFIG_GPIO_10_11_CTRL_LED
		if(value == LED_OFF)
		{
			ret = gpio_direction_output(GPIO_RED_LIGHT_CTRL, 0);   
		}
		else
		{
			ret = gpio_direction_output(GPIO_RED_LIGHT_CTRL, 1);   
		}
       		#else 		
		//ret = pmic_set_led_intensity(LED_KEYPAD, value / MAX_PMIC_BL_LEVEL);
		if(value == LED_OFF)
		{
			ret = pmic_secure_mpp_config_i_sink(PM_MPP_3,  value / MAX_PMIC_BL_LEVEL, PM_MPP__I_SINK__SWITCH_DIS);		   
		}
		else
		{
			ret = pmic_secure_mpp_config_i_sink(PM_MPP_3,  value / MAX_PMIC_BL_LEVEL, PM_MPP__I_SINK__SWITCH_ENA);		
		}			         
		#endif
	} 
	else //green
	{
       		#if defined CONFIG_GPIO_19_20_CTRL_LED || defined CONFIG_GPIO_10_11_CTRL_LED
		if(value == LED_OFF)
		{
			ret = gpio_direction_output(GPIO_GREEN_LIGHT_CTRL, 0);   
		}
		else
		{
			ret = gpio_direction_output(GPIO_GREEN_LIGHT_CTRL, 1);   
		}
       		#else 		
		//ret = pmic_set_led_intensity(LED_LCD, value / MAX_PMIC_BL_LEVEL);
		if(value == LED_OFF)
		{
			ret = pmic_secure_mpp_config_i_sink(PM_MPP_8,  value / MAX_PMIC_BL_LEVEL, PM_MPP__I_SINK__SWITCH_DIS);		   
		}
		else
		{
			ret = pmic_secure_mpp_config_i_sink(PM_MPP_8,  value / MAX_PMIC_BL_LEVEL, PM_MPP__I_SINK__SWITCH_ENA);		
		}			
		#endif		
	}
	
#endif	//CONFIG_GPIO_6_CTRL_BLUE_LED
	
	if (ret)
		dev_err(led_cdev->dev, "[ZHY@PMIC LEDS]can't set pmic backlight\n");
}

static void pmic_leds_timer(unsigned long arg)
{
      struct BLINK_LED_data *b_led = (struct BLINK_LED_data *)arg;

              if(b_led->blink_flag)
		{
		       if(b_led->blink_led_flag)
		       {
				if(led_not_suspend_flag) 
					schedule_work(&b_led->work_led_off);
		       	     b_led->blink_led_flag = 0;  
		       	     mod_timer(&b_led->timer,jiffies + msecs_to_jiffies(b_led->blink_off_time));
		       }
			else
			{
				if(led_not_suspend_flag) 
			       		schedule_work(&b_led->work_led_on);
		       	       b_led->blink_led_flag = 1;
			       mod_timer(&b_led->timer,jiffies + msecs_to_jiffies(b_led->blink_on_time));
		       }
		}	
		else
		{
			#if 0	//we should not always turn the led on. chenchongbao.20111202
				//schedule_work(&b_led->work_led_on);
				schedule_work(&b_led->work_led_off);
			#else
				if(b_led->led.brightness)
					schedule_work(&b_led->work_led_on);
				else
					schedule_work(&b_led->work_led_off);
			#endif
		}
		
}

static ssize_t led_blink_solid_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct STATUS_LED_data *STATUS_LED;
	int idx = 1;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = 0;

	if (!strcmp(led_cdev->name, "red"))
		idx = 0;
	else
		idx = 1;

	STATUS_LED = container_of(led_cdev, struct STATUS_LED_data, blink_led[idx].led);

	/* no lock needed for this */
	sprintf(buf, "%u\n", STATUS_LED->blink_led[idx].blink_flag);
	ret = strlen(buf) + 1;

	return ret;
}

static ssize_t led_blink_solid_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct STATUS_LED_data *STATUS_LED;
	int idx = 1;
	char *after;
	unsigned long state;
	ssize_t ret = -EINVAL;
	size_t count;

	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	if (!strcmp(led_cdev->name, "red"))
		idx = 0;
	else
		idx = 1;

	STATUS_LED = container_of(led_cdev, struct STATUS_LED_data, blink_led[idx].led);

	state = simple_strtoul(buf, &after, 10);

	count = after - buf;

	if (*after && isspace(*after))
		count++;

	if (count == size) {
		ret = count;
		spin_lock(&STATUS_LED->data_lock);
		if(0==state)
		{
		       STATUS_LED->blink_led[idx].blink_flag= 0;
			pr_info(ZHY_BL_TAG "DISABLE %s led blink \n",idx?"green":"red");
		}
		else
		{
		       STATUS_LED->blink_led[idx].blink_flag= 1;
		       STATUS_LED->blink_led[idx].blink_led_flag = 1;
			schedule_work(&STATUS_LED->blink_led[idx].work_led_on);
			mod_timer(&STATUS_LED->blink_led[idx].timer,jiffies + msecs_to_jiffies(STATUS_LED->blink_led[idx].blink_off_time));
			pr_info(ZHY_BL_TAG "ENABLE %s led blink \n",idx?"green":"red");
		}
		spin_unlock(&STATUS_LED->data_lock);
	}

	return ret;
}

static DEVICE_ATTR(blink, 0644, led_blink_solid_show, led_blink_solid_store);

static ssize_t cpldled_grpfreq_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct STATUS_LED_data *STATUS_LED;
	int idx = 1;

	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	if (!strcmp(led_cdev->name, "red"))
		idx = 0;
	else
		idx = 1;

	STATUS_LED = container_of(led_cdev, struct STATUS_LED_data, blink_led[idx].led);
	return sprintf(buf, "blink_on_time = %u ms \n", STATUS_LED->blink_led[idx].blink_on_time);
}

static ssize_t cpldled_grpfreq_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct STATUS_LED_data *STATUS_LED;
	int idx = 1;
	char *after;
	unsigned long state;
	ssize_t ret = -EINVAL;
	size_t count;

	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	if (!strcmp(led_cdev->name, "red"))
		idx = 0;
	else
		idx = 1;

	STATUS_LED = container_of(led_cdev, struct STATUS_LED_data, blink_led[idx].led);

	state = simple_strtoul(buf, &after, 10);

	count = after - buf;

	if (*after && isspace(*after))
		count++;

	if (count == size) {
		ret = count;
		spin_lock(&STATUS_LED->data_lock);
		STATUS_LED->blink_led[idx].blink_on_time = state;  //in ms
		pr_info(ZHY_BL_TAG "Set %s led blink_on_time=%d ms \n",idx?"green":"red",STATUS_LED->blink_led[idx].blink_on_time);
		spin_unlock(&STATUS_LED->data_lock);
	}

	return ret;
}

static DEVICE_ATTR(grpfreq, 0644, cpldled_grpfreq_show, cpldled_grpfreq_store);

static ssize_t cpldled_grppwm_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct STATUS_LED_data *STATUS_LED;
	int idx = 1;

	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	if (!strcmp(led_cdev->name, "red"))
		idx = 0;
	else
		idx = 1;
	
	STATUS_LED = container_of(led_cdev, struct STATUS_LED_data, blink_led[idx].led);
	return sprintf(buf, "blink_off_time = %u ms\n", STATUS_LED->blink_led[idx].blink_off_time);
}

static ssize_t cpldled_grppwm_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct STATUS_LED_data *STATUS_LED;
	int idx = 1;
	char *after;
	unsigned long state;
	ssize_t ret = -EINVAL;
	size_t count;

	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	if (!strcmp(led_cdev->name, "red"))
		idx = 0;
	else
		idx = 1;

	STATUS_LED = container_of(led_cdev, struct STATUS_LED_data, blink_led[idx].led);

	state = simple_strtoul(buf, &after, 10);

	count = after - buf;

	if (*after && isspace(*after))
		count++;

	if (count == size) {
		ret = count;
		spin_lock(&STATUS_LED->data_lock);
		STATUS_LED->blink_led[idx].blink_off_time= state;  //in ms
		pr_info(ZHY_BL_TAG "Set %s led blink_off_time=%d ms \n",idx?"green":"red",STATUS_LED->blink_led[idx].blink_off_time);
		spin_unlock(&STATUS_LED->data_lock);
	}

	return ret;
}

static DEVICE_ATTR(grppwm, 0644, cpldled_grppwm_show, cpldled_grppwm_store);

static int msm_pmic_led_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i, j;

#ifdef CONFIG_GPIO_6_CTRL_BLUE_LED	//zte-ccb-20120322-0

	ret= gpio_tlmm_config(
		GPIO_CFG(GPIO_BLUE, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),GPIO_CFG_ENABLE);

	if (ret) 
	{
		printk(KERN_ERR "%s: gpio_tlmm_config() return %d\n", __func__, ret);
		return -EIO;
	}

	if((gpio_direction_output(GPIO_BLUE, 0))<0)  //turn  off
	{
		printk(KERN_ERR "gpio_direction_output failed!\n");
      		return -EIO;
	}

#else

         #if defined CONFIG_GPIO_19_20_CTRL_LED || defined CONFIG_GPIO_10_11_CTRL_LED
	for (i = 0;i < 2; i++) 
	{
		ret= gpio_tlmm_config(data_leds_gpio[i],GPIO_CFG_ENABLE);
		if (ret) 
		{
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n", __func__, data_leds_gpio[i], ret);
			return -EIO;
	         }
	}	

	if((gpio_direction_output(GPIO_RED_LIGHT_CTRL, 0))<0)  //turn  off red
	{
		printk(KERN_ERR "gpio_direction_output: %d failed!\n", GPIO_RED_LIGHT_CTRL);
      		return -EIO;
	}	

	if((gpio_direction_output(GPIO_GREEN_LIGHT_CTRL, 0))<0)  //turn  off  green
	{
	    	printk(KERN_ERR "gpio_direction_output: %d failed!\n", GPIO_GREEN_LIGHT_CTRL);
      		return -EIO;
	}
         #endif
  
#endif	//CONFIG_GPIO_6_CTRL_BLUE_LED
  
	STATUS_LED = kzalloc(sizeof(struct STATUS_LED_data), GFP_KERNEL);
	if (STATUS_LED == NULL) {
		printk(KERN_ERR "STATUS_LED_probe: no memory for device\n");
		ret = -ENOMEM;
		goto err_alloc_failed;
	}
	
	STATUS_LED->blink_led[0].led.name = "red";
	STATUS_LED->blink_led[0].led.brightness_set = msm_pmic_bl_led_set;
	STATUS_LED->blink_led[0].led.brightness = LED_OFF;
	STATUS_LED->blink_led[0].blink_flag = 0;
	STATUS_LED->blink_led[0].blink_on_time = 500;
	STATUS_LED->blink_led[0].blink_off_time = 500;

	STATUS_LED->blink_led[1].led.name = "green";
	STATUS_LED->blink_led[1].led.brightness_set = msm_pmic_bl_led_set;
	STATUS_LED->blink_led[1].led.brightness = LED_OFF;
	STATUS_LED->blink_led[1].blink_flag = 0;
	STATUS_LED->blink_led[1].blink_on_time = 500;
	STATUS_LED->blink_led[1].blink_off_time = 500;

	spin_lock_init(&STATUS_LED->data_lock);

	for (i = 0; i < 2; i++) {	/* red, green */
		ret = led_classdev_register(&pdev->dev, &STATUS_LED->blink_led[i].led);
		if (ret) {
			printk(KERN_ERR
			       "STATUS_LED: led_classdev_register failed\n");
			goto err_led_classdev_register_failed;
		}
	}

	for (i = 0; i < 2; i++) {
		ret =
		    device_create_file(STATUS_LED->blink_led[i].led.dev, &dev_attr_blink);
		if (ret) {
			printk(KERN_ERR
			       "STATUS_LED: create dev_attr_blink failed\n");
			goto err_out_attr_blink;
		}
	}

	for (i = 0; i < 2; i++) {
		ret =
		    device_create_file(STATUS_LED->blink_led[i].led.dev, &dev_attr_grppwm);
		if (ret) {
			printk(KERN_ERR
			       "STATUS_LED: create dev_attr_grppwm failed\n");
			goto err_out_attr_grppwm;
		}
	}

	for (i = 0; i < 2; i++) {
		ret =
		    device_create_file(STATUS_LED->blink_led[i].led.dev, &dev_attr_grpfreq);
		if (ret) {
			printk(KERN_ERR
			       "STATUS_LED: create dev_attr_grpfreq failed\n");
			goto err_out_attr_grpfreq;
		}
	}
	dev_set_drvdata(&pdev->dev, STATUS_LED);
	
	for (i = 0; i < 2; i++)
	{
	       INIT_WORK(&STATUS_LED->blink_led[i].work_led_on, func[i*2]);
	       INIT_WORK(&STATUS_LED->blink_led[i].work_led_off, func[i*2+1]);
	       setup_timer(&STATUS_LED->blink_led[i].timer, pmic_leds_timer, (unsigned long)&STATUS_LED->blink_led[i]);
	       msm_pmic_bl_led_set(&STATUS_LED->blink_led[i].led, LED_OFF);
	}
	
       return 0;
	   
err_out_attr_grpfreq:
	for (j = 0; j < i; j++)
		device_remove_file(STATUS_LED->blink_led[i].led.dev, &dev_attr_blink);
	i = 2;
	
err_out_attr_grppwm:
	for (j = 0; j < i; j++)
		device_remove_file(STATUS_LED->blink_led[i].led.dev, &dev_attr_blink);
	i = 2;
	
err_out_attr_blink:
	for (j = 0; j < i; j++)
		device_remove_file(STATUS_LED->blink_led[i].led.dev, &dev_attr_blink);
	i = 2;

err_led_classdev_register_failed:
	for (j = 0; j < i; j++)
		led_classdev_unregister(&STATUS_LED->blink_led[i].led);

err_alloc_failed:
	kfree(STATUS_LED);

	return ret;
	
}

static int __devexit msm_pmic_led_remove(struct platform_device *pdev)
{
       int i;
	   
       for (i = 0; i < 2; i++)
		led_classdev_unregister(&STATUS_LED->blink_led[i].led);

	return 0;
}

#ifndef CONFIG_ZTE_NLED_BLINK_WHILE_APP_SUSPEND	//LHX_NLED_20110107_01 enable NLED to blink for all projects
#define CONFIG_ZTE_NLED_BLINK_WHILE_APP_SUSPEND
#endif

#ifdef CONFIG_ZTE_NLED_BLINK_WHILE_APP_SUSPEND
#include "../../arch/arm/mach-msm/proc_comm.h"
#define NLED_APP2SLEEP_FLAG_LCD 0x0001//green
#define NLED_APP2SLEEP_FLAG_LCD_BLINK 0x0002//green blink flag

#define NLED_APP2SLEEP_FLAG_KBD 0x0010//red
#define NLED_APP2SLEEP_FLAG_KBD_BLINK 0x0020//red blink flag
//#define NLED_APP2SLEEP_FLAG_VIB 0x0100
#define ZTE_PROC_COMM_CMD3_NLED_BLINK_DISABLE 2
#define ZTE_PROC_COMM_CMD3_NLED_BLINK_ENABLE 3

//inform modem the states of NLED through PCOM_CUSTOMER_CMD2
int msm_pmic_led_config_while_app2sleep(unsigned blink_kbd,unsigned blink_lcd, int blink_kbd_flag, int blink_lcd_flag, unsigned set)
{
	int config_last = 0;
	
	if(blink_kbd > 0)//red
	{
		config_last |= NLED_APP2SLEEP_FLAG_KBD;
	}
	if(blink_lcd > 0)//green
	{
		config_last |= NLED_APP2SLEEP_FLAG_LCD;
	}
	
	if(blink_kbd_flag > 0)//red
	{
		config_last |= NLED_APP2SLEEP_FLAG_KBD_BLINK;
	}
	if(blink_lcd_flag > 0)//green
	{
		config_last |= NLED_APP2SLEEP_FLAG_LCD_BLINK;
	}

		pr_info(ZHY_BL_TAG"green %d,green_blink %d,red %d,red_blink %d;set(2:dis;3:enable):%d\n",blink_lcd,blink_lcd_flag,blink_kbd,blink_kbd_flag,set);
		//return msm_proc_comm(PCOM_CUSTOMER_CMD2, &config_last, &set);
	return msm_proc_comm(PCOM_CUSTOMER_CMD3, &config_last, &set);
}
#endif

#ifdef CONFIG_PM
static int msm_pmic_led_suspend(struct platform_device *dev,
		pm_message_t state)
{
	//int i;

	led_not_suspend_flag = 0;	//ccb add
	   
	   #ifdef CONFIG_ZTE_NLED_BLINK_WHILE_APP_SUSPEND
	   //blink_led[0] red,blink_led[1] green
       msm_pmic_led_config_while_app2sleep( STATUS_LED->blink_led[0].led.brightness,
                                            STATUS_LED->blink_led[1].led.brightness, STATUS_LED->blink_led[0].blink_flag, 
                                            STATUS_LED->blink_led[1].blink_flag, ZTE_PROC_COMM_CMD3_NLED_BLINK_ENABLE);
	   #endif

	   #if 0	/* chenchongbao.20111212 there was an interval before amss open the led */
       for (i = 0; i < 2; i++)
		led_classdev_suspend(&STATUS_LED->blink_led[i].led);
	   #endif

	return 0;
}

static int msm_pmic_led_resume(struct platform_device *dev)
{
	int i;

	led_not_suspend_flag = 1;	//ccb add
	   
	#ifdef CONFIG_ZTE_NLED_BLINK_WHILE_APP_SUSPEND
	   //msm_pmic_led_config_while_app2sleep( 0, 0, 0, 0, ZTE_PROC_COMM_CMD3_NLED_BLINK_DISABLE);	//chenchongbao.20111209 no need
	#endif
	for (i = 0; i < 2; i++)
		led_classdev_resume(&STATUS_LED->blink_led[i].led);
	

	return 0;
}
#else
#define msm_pmic_led_suspend NULL
#define msm_pmic_led_resume NULL
#endif

static struct platform_driver msm_pmic_led_driver = {
	.probe		= msm_pmic_led_probe,
	.remove		= __devexit_p(msm_pmic_led_remove),
	.suspend	= msm_pmic_led_suspend,
	.resume		= msm_pmic_led_resume,
	.driver		= {
		.name	= "pmic-leds",
		.owner	= THIS_MODULE,
	},
};

static int __init msm_pmic_led_init(void)
{
	return platform_driver_register(&msm_pmic_led_driver);
}
module_init(msm_pmic_led_init);

static void __exit msm_pmic_led_exit(void)
{
	platform_driver_unregister(&msm_pmic_led_driver);
}
module_exit(msm_pmic_led_exit);

MODULE_DESCRIPTION("MSM PMIC LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:pmic-leds");

#else
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>

#include <mach/pmic.h>

#define MAX_KEYPAD_BL_LEVEL	16

static void msm_keypad_bl_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	int ret;

	ret = pmic_set_led_intensity(LED_KEYPAD, value / MAX_KEYPAD_BL_LEVEL);
	if (ret)
		dev_err(led_cdev->dev, "can't set keypad backlight\n");
}

static struct led_classdev msm_kp_bl_led = {
	.name			= "keyboard-backlight",
	.brightness_set		= msm_keypad_bl_led_set,
	.brightness		= LED_OFF,
};

static int msm_pmic_led_probe(struct platform_device *pdev)
{
	int rc;

	rc = led_classdev_register(&pdev->dev, &msm_kp_bl_led);
	if (rc) {
		dev_err(&pdev->dev, "unable to register led class driver\n");
		return rc;
	}
	msm_keypad_bl_led_set(&msm_kp_bl_led, LED_OFF);
	return rc;
}

static int __devexit msm_pmic_led_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&msm_kp_bl_led);

	return 0;
}

#ifdef CONFIG_PM
static int msm_pmic_led_suspend(struct platform_device *dev,
		pm_message_t state)
{
	led_classdev_suspend(&msm_kp_bl_led);

	return 0;
}

static int msm_pmic_led_resume(struct platform_device *dev)
{
	led_classdev_resume(&msm_kp_bl_led);

	return 0;
}
#else
#define msm_pmic_led_suspend NULL
#define msm_pmic_led_resume NULL
#endif

static struct platform_driver msm_pmic_led_driver = {
	.probe		= msm_pmic_led_probe,
	.remove		= __devexit_p(msm_pmic_led_remove),
	.suspend	= msm_pmic_led_suspend,
	.resume		= msm_pmic_led_resume,
	.driver		= {
		.name	= "pmic-leds",
		.owner	= THIS_MODULE,
	},
};

static int __init msm_pmic_led_init(void)
{
	return platform_driver_register(&msm_pmic_led_driver);
}
module_init(msm_pmic_led_init);

static void __exit msm_pmic_led_exit(void)
{
	platform_driver_unregister(&msm_pmic_led_driver);
}
module_exit(msm_pmic_led_exit);

MODULE_DESCRIPTION("MSM PMIC LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:pmic-leds");
#endif
