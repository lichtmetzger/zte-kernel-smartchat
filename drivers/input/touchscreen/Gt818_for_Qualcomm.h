/*
 * include/linux/goodix_touch.h
 *
 * Copyright (C) 2011 Goodix, Inc.
 * 
 * Author: Felix
 * Date: 2011.04.28
 */

#ifndef 	_LINUX_GOODIX_TOUCH_H
#define		_LINUX_GOODIX_TOUCH_H

#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>

//*************************TouchScreen Work Part*****************************
#define GOODIX_I2C_NAME "Goodix-TS"
#define GT801_PLUS
#define GT801_NUVOTON
#define GUITAR_UPDATE_STATE 0x02
//#define NO_DEFAULT_ID
//define resolution of the touchscreen
#if 1
//#define SELF_DEF
#define TOUCH_MAX_HEIGHT 	530
#define TOUCH_MAX_WIDTH		320
#endif
//define resolution of the LCD
//#define STOP_IRQ_TYPE                     // if define then   no stop irq in irq_handle   kuuga add 1202S
#define REFRESH 0     //0~0x64   Scan rate = 10000/(100+REFRESH)//define resolution of the LCD
//#define SCREEN_MAX_HEIGHT	7680				
//#define SCREEN_MAX_WIDTH	5120

/*
#define SHUTDOWN_PORT 	S3C64XX_GPF(3)			//SHUTDOWN管脚号
#define INT_PORT  	S3C64XX_GPL(10)//S3C64XX_GPN(15)						//Int IO port
#ifdef INT_PORT
	#define TS_INT 		gpio_to_irq(INT_PORT)			//Interrupt Number,EINT18(119)
	#define INT_CFG    	S3C_GPIO_SFN(3)//S3C_GPIO_SFN(2)					//IO configer as EINT
#else
	#define TS_INT	0
#endif	
*/
/*
#define GPIO_TS_RESET	20
#define GPIO_TS_INT	21
#define GPIO_TS_SCL    122
#define GPIO_TS_SDA    123
*/
#define SHUTDOWN_PORT	107
#define GPIO_TS_RESET	12
#define GPIO_TS_INT		82
//#define GPIO_TS_SCL    122
//#define GPIO_TS_SDA    123


//#define FLAG_UP		0
//#define FLAG_DOWN		1
//set GT801 PLUS trigger mode,只能设置0或1 
//#define INT_TRIGGER		0	
#define POLL_TIME		10	//actual query spacing interval:POLL_TIME+6

//#define GOODIX_MULTI_TOUCH
//#ifdef GOODIX_MULTI_TOUCH
	#define MAX_FINGER_NUM	5	
//#else
//	#define MAX_FINGER_NUM	1	
//#endif

/////////////////////////////// UPDATE STEP 5 START ///////////////////////////////////////////////////////////////// 
#define TPD_CHIP_VERSION_C_FIRMWARE_BASE 0x5A
#define TPD_CHIP_VERSION_D1_FIRMWARE_BASE 0x7A
#define TPD_CHIP_VERSION_E_FIRMWARE_BASE 0x9A
#define TPD_CHIP_VERSION_D2_FIRMWARE_BASE 0xBA

enum
{
	TPD_GT818_VERSION_B,    
	TPD_GT818_VERSION_C,   
	TPD_GT818_VERSION_D1,      
	TPD_GT818_VERSION_E,       
	TPD_GT818_VERSION_D2
};
/////////////////////////////// UPDATE STEP 5 END /////////////////////////////////////////////////////////////////


//#define swap(x, y) do { typeof(x) z = x; x = y; y = z; } while (0)

#define READ_TOUCH_ADDR_H 0x07
#define READ_TOUCH_ADDR_L 0x12
#define READ_KEY_ADDR_H 0x07
#define READ_KEY_ADDR_L 0x21
#define READ_COOR_ADDR_H 0x07
#define READ_COOR_ADDR_L 0x22
#define READ_ID_ADDR_H 0x00
#define READ_ID_ADDR_L 0xff


struct goodix_ts_data {
	uint16_t addr;
	uint8_t bad_data;
	struct i2c_client *client;
	struct input_dev *input_dev;
	int use_reset;		//use RESET flag
	int use_irq;		//use EINT flag
	int read_mode;		//read moudle mode,20110221 by andrew
	struct hrtimer timer;
	struct work_struct  work;
	char phys[32];
	int retry;
	int irq;
	spinlock_t				irq_lock;      //add by kuuga
//	int 				 irq_is_disable; /* 0: irq enable */ //add by kuuga
	uint16_t abs_x_max;
	uint16_t abs_y_max;
	uint8_t max_touch_num;
	uint8_t int_trigger_type;
	uint8_t btn_state;                    // key states
	int sensor_id;	//xiayc
/////////////////////////////// UPDATE STEP 6 START /////////////////////////////////////////////////////////////////
       unsigned int version;
/////////////////////////////// UPDATE STEP 6 END /////////////////////////////////////////////////////////////////
	struct early_suspend early_suspend;
	int (*power)(struct goodix_ts_data * ts, int on);
};

//*****************************End of Part I *********************************

//*************************Touchkey Surpport Part*****************************
#define HAVE_TOUCH_KEY
#ifdef HAVE_TOUCH_KEY
	const uint16_t touch_key_array[]={
										KEY_MENU,
										KEY_HOME,				//MENU
										KEY_BACK,				//HOME
										KEY_SEARCH				//CALL
									 }; 
	#define MAX_KEY_NUM	 (sizeof(touch_key_array)/sizeof(touch_key_array[0]))
#endif
//*****************************End of Part II*********************************

/////////////////////////////// UPDATE STEP 7 START /////////////////////////////////////////////////////////////////
//*************************Firmware Update part*******************************          
//#define CREATE_WR_NODE     //add gt818 tool (cfg,version,raw_data,diff_data)
#define CONFIG_TOUCHSCREEN_GOODIX_IAP        
#ifdef CONFIG_TOUCHSCREEN_GOODIX_IAP
//static int goodix_update_write(struct file *filp, const char __user *buff, unsigned long len, void *data);
//static int goodix_update_read( char *page, char **start, off_t off, int count, int *eof, void *data );

#define PACK_SIZE 					64					//update file package size
//#define MAX_TIMEOUT					30000				//update time out conut
//#define MAX_I2C_RETRIES				10					//i2c retry times

//write cmd
#define APK_UPDATE_TP               1
#define APK_READ_FUN                 10
#define APK_WRITE_CFG               11

//fun cmd
//#define CMD_DISABLE_TP             0
//#define CMD_ENABLE_TP              1
#define CMD_READ_VER               2
#define CMD_READ_RAW               3
#define CMD_READ_DIF               4
#define CMD_READ_CFG               5
#define CMD_READ_CHIP_TYPE         6
//#define CMD_SYS_REBOOT             101

//read mode
#define MODE_RD_VER                1
#define MODE_RD_RAW                2
#define MODE_RD_DIF                3
#define MODE_RD_CFG                4
#define MODE_RD_CHIP_TYPE          5

struct tpd_firmware_info_t
{
    int magic_number_1;
    int magic_number_2;
    unsigned short version;
    unsigned short length;    
    unsigned short checksum;
    unsigned char data;
};

#define  NVRAM_LEN               0x0FF0   //	nvram total space
#define  NVRAM_BOOT_SECTOR_LEN	 0x0100	// boot sector 
#define  NVRAM_UPDATE_START_ADDR 0x4100

#define  BIT_NVRAM_STROE	    0
#define  BIT_NVRAM_RECALL	    1
#define BIT_NVRAM_LOCK 2
#define  REG_NVRCS_H 0X12
#define  REG_NVRCS_L 0X01

//error no
#define ERROR_NO_FILE				2//ENOENT
#define ERROR_FILE_READ				23//ENFILE
#define ERROR_FILE_TYPE				21//EISDIR
#define ERROR_GPIO_REQUEST			4//EINTR
#define ERROR_I2C_TRANSFER			5//EIO
#define ERROR_NO_RESPONSE			16//EBUSY
#define ERROR_TIMEOUT				110//ETIMEDOUT


#endif
//*****************************End of Part III********************************
/////////////////////////////// UPDATE STEP 7 END /////////////////////////////////////////////////////////////////
 
struct goodix_i2c_rmi_platform_data {
	uint32_t version;	/* Use this entry for panels with */
	//reservation
};

#endif /* _LINUX_GOODIX_TOUCH_H */
