/******************************************************************
*File Name: 	ltr.c 	                                           *
*Description:	Linux device driver for Taos ambient light and    *
*			proximity sensors.                                          *                                
*******************************************************************
Geschichte:	                                                                        
Wenn               Wer          Was                                                                        Tag
2011-02-19    wangzy     Update ALS und prox source     
2012-04-27    wlg        test ltr558                                   
******************************************************************/
// includes
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/timer.h>
#include <asm/uaccess.h>
#include <asm/errno.h>
#include <asm/delay.h>
#include <linux/irq.h>
#include <asm/gpio.h>
#include <linux/i2c/taos_common.h>
#include <linux/input.h>
#include <linux/miscdevice.h>

#define LTR_ID_NAME_SIZE		10

#define LTR_MAX_NUM_DEVICES		3

/*************************************************/
// LTR558 Register Definitions
#define LTR_MAX_DEVICE_REGS 25
#define LTRCHIP_VENDOR      0x0001
/* PART_ID */
#define LTRCHIP_PART		0x80  /* needs to be set to the correct part id */
#define LTRCHIP_PART_MASK	0x0FF   
#define LTRCHIP_MANUFAC		0x05  /* needs to be set to the correct part id */
#define LTRCHIP_MANUFAC_MASK	0x0FF   

/* Operating modes for both */
#define LTRCHIP_STANDBY		0x00
#define LTRCHIP_PS_ONESHOT	0x01
#define LTRCHIP_PS_REPEAT	0x02

#define LTRCHIP_TRIG_MEAS	0x01


/* Interrupt control */
#define LTRCHIP_INT_LEDS_INT_HI	(1 << 1)
#define LTRCHIP_INT_LEDS_INT_LO	(1 << 0)
#define LTRCHIP_INT_ALS_INT_HI	(1 << 2) 
#define LTRCHIP_INT_ALS_INT_LO  (1 << 3)
#define LTRCHIP_INT_EXTERNAL    (1 << 4)

#define LTRCHIP_DISABLE		0
#define LTRCHIP_ENABLE		3

/*10 : light on*/
/*01 : prox on*/
/*20 : light off*/
/*02 : prox off*/
#define Light_turn_on	0x10
#define Prox_turn_on 	0x01
#define Light_turn_off	0x20
#define Prox_turn_off 	0x02

enum ltr_regs {
	 LTRCHIP_ALS_CONTR,
	 LTRCHIP_PS_CONTR,
	 LTRCHIP_PS_LED,
	 LTRCHIP_PS_N_PULSES,
	 LTRCHIP_PS_MEAS_RATE,	 
	 LTRCHIP_ALS_MEAS_RATE,	 	 
	 LTRCHIP_PART_ID,
	 LTRCHIP_MANUFAC_ID,
	 LTRCHIP_ALS_DATA_CH1_Low,
	 LTRCHIP_ALS_DATA_CH1_High,       
	 LTRCHIP_ALS_DATA_CH0_Low,
	 LTRCHIP_ALS_DATA_CH0_High,       
	 LTRCHIP_ALS_PS_STATUS,
	 LTRCHIP_PS_DATA_Low,
	 LTRCHIP_PS_DATA_High,       
	 LTRCHIP_INT_CONFIG,
	 LTRCHIP_PS_TH_UP_Low,
	 LTRCHIP_PS_TH_UP_High,
	 LTRCHIP_PS_TH_LO_Low,
	 LTRCHIP_PS_TH_LO_High,
	 LTRCHIP_ALS_TH_UP_Low,
	 LTRCHIP_ALS_TH_UP_High,
	 LTRCHIP_ALS_TH_LO_Low,
	 LTRCHIP_ALS_TH_LO_High,
	 LTRCHIP_INTERRUPT_PERSIST,
};

static const char ltr_regmap[]={
	 [LTRCHIP_ALS_CONTR] = 0x80, /* ALS Gain Reset mode*/
	 [LTRCHIP_PS_CONTR] = 0x81, /* PS Gain mode*/
	 [LTRCHIP_PS_LED] = 0x82, /* Set PS LED Freq Duty Current */
	 [LTRCHIP_PS_N_PULSES] = 0x83, /* Sets PS N pulses to emitted */
	 [LTRCHIP_PS_MEAS_RATE] = 0x84, /* PS measurement intergration time */
	 [LTRCHIP_ALS_MEAS_RATE] = 0x85, /* ALS measurement intergration time */
	 [LTRCHIP_PART_ID] = 0x86, /* Part number and revision ID */
	 [LTRCHIP_MANUFAC_ID] = 0x87, /* Part number and revision ID */
	 [LTRCHIP_ALS_DATA_CH1_Low] = 0x88, /*  */
	 [LTRCHIP_ALS_DATA_CH1_High] = 0x89, /*  */
	 [LTRCHIP_ALS_DATA_CH0_Low] = 0x8A, /*  */
	 [LTRCHIP_ALS_DATA_CH0_High] = 0x8B, /*  */
	 [LTRCHIP_ALS_PS_STATUS] = 0x8C, /* ALS PS data status and interrupt status */
	 [LTRCHIP_PS_DATA_Low] = 0x8D, /*  */
	 [LTRCHIP_PS_DATA_High] = 0x8E, /*  */
	 [LTRCHIP_INT_CONFIG] = 0x8F, /*  */	 
	 [LTRCHIP_PS_TH_UP_Low] = 0x90, /*  */
	 [LTRCHIP_PS_TH_UP_High] = 0x91, /*  */
	 [LTRCHIP_PS_TH_LO_Low] = 0x92, /*  */
	 [LTRCHIP_PS_TH_LO_High] = 0x93, /*  */
	 [LTRCHIP_ALS_TH_UP_Low] = 0x97, /*  */
	 [LTRCHIP_ALS_TH_UP_High] = 0x98, /*  */
	 [LTRCHIP_ALS_TH_LO_Low] = 0x99, /*  */
	 [LTRCHIP_ALS_TH_LO_High] = 0x9A, /*  */
	 [LTRCHIP_INTERRUPT_PERSIST] = 0x9E /*  */
};

/* ALS Gain Reset mode*/
#define LTRCHIP_ALS_Gain_Mask       0x08
#define LTRCHIP_ALS_Gain_Range2     0x00 //2lux to 64klun
#define LTRCHIP_ALS_Gain_Range1     0x08 //0.01lux to 320lux

#define LTRCHIP_ALS_SW_Reset_Mask      0x04
#define LTRCHIP_ALS_SW_Reset_Started   0x04

#define LTRCHIP_ALS_Mode_Mask        0x03
#define LTRCHIP_ALS_Mode_Standby     0x00 //or 0x01
#define LTRCHIP_ALS_Mode_Active      0x02 //or 0x11

/* PS Gain mode*/
#define LTRCHIP_PS_Gain_Mask       0x0C
#define LTRCHIP_PS_Gain_1           0x00 //1 gain
#define LTRCHIP_PS_Gain_4           0x04 //4 gain
#define LTRCHIP_PS_Gain_8           0x08 //8 gain
#define LTRCHIP_PS_Gain_16          0x0C //16 gain

#define LTRCHIP_PS_Mode_Mask        0x03
#define LTRCHIP_PS_Mode_Standby     0x00 //or 0x01
#define LTRCHIP_PS_Mode_Active      0x02 //or 0x11

/* Set PS LED Freq Duty Current */
#define LTRCHIP_PS_LED_Freq_Mask     0xE0
#define LTRCHIP_PS_LED_Freq_30KHz    0x00
#define LTRCHIP_PS_LED_Freq_40KHz    0x20
#define LTRCHIP_PS_LED_Freq_50KHz    0x40
#define LTRCHIP_PS_LED_Freq_60KHz    0x60
#define LTRCHIP_PS_LED_Freq_70KHz    0x80
#define LTRCHIP_PS_LED_Freq_80KHz    0xA0
#define LTRCHIP_PS_LED_Freq_90KHz    0xC0
#define LTRCHIP_PS_LED_Freq_100KHz   0xE0

#define LTRCHIP_PS_LED_Duty_Mask     0x18
#define LTRCHIP_PS_LED_Duty_25Per     0x00
#define LTRCHIP_PS_LED_Duty_50Per     0x08
#define LTRCHIP_PS_LED_Duty_75Per     0x10
#define LTRCHIP_PS_LED_Duty_100Per    0x18

#define LTRCHIP_PS_LED_Current_Mask  0x07
#define LTRCHIP_PS_LED_Current_5mA      0x00
#define LTRCHIP_PS_LED_Current_10mA     0x01
#define LTRCHIP_PS_LED_Current_20mA     0x02
#define LTRCHIP_PS_LED_Current_50mA     0x03
#define LTRCHIP_PS_LED_Current_100mA    0x04

/* Sets PS N pulses to emitted */
#define LTRCHIP_PS_N_PULSES_Mask  0xFF //max is 255 min is 0 default is 8

/* PS measurement intergration time */
#define LTRCHIP_PS_MEAS_RATE_Mask  0x0F
#define LTRCHIP_PS_MEAS_RATE_50ms   0x00
#define LTRCHIP_PS_MEAS_RATE_70ms   0x01
#define LTRCHIP_PS_MEAS_RATE_100ms  0x02
#define LTRCHIP_PS_MEAS_RATE_200ms  0x03
#define LTRCHIP_PS_MEAS_RATE_500ms  0x04
#define LTRCHIP_PS_MEAS_RATE_1000ms 0x05
#define LTRCHIP_PS_MEAS_RATE_2000ms 0x06

/* ALS measurement intergration time and Repeqt Rate*/
#define LTRCHIP_ALS_Inte_Time_Mask  0x18
#define LTRCHIP_ALS_Inte_Time_100ms  0x00
#define LTRCHIP_ALS_Inte_Time_50ms   0x08
#define LTRCHIP_ALS_Inte_Time_200ms  0x10
#define LTRCHIP_ALS_Inte_Time_400ms  0x18

#define LTRCHIP_ALS_MEAS_RATE_Mask  0x07
#define LTRCHIP_ALS_MEAS_RATE_50ms    0x00
#define LTRCHIP_ALS_MEAS_RATE_100ms   0x01
#define LTRCHIP_ALS_MEAS_RATE_200ms   0x02
#define LTRCHIP_ALS_MEAS_RATE_500ms   0x03
#define LTRCHIP_ALS_MEAS_RATE_1000ms  0x04
#define LTRCHIP_ALS_MEAS_RATE_2000ms  0x05

/* ALS PS data status and interrupt status */
#define LTRCHIP_ALS_Gain_STATUS_Mask  0x10
#define LTRCHIP_ALS_INT_STATUS_Mask   0x08
#define LTRCHIP_ALS_Data_STATUS_Mask  0x04
#define LTRCHIP_PS_INT_STATUS_Mask    0x02
#define LTRCHIP_PS_Data_STATUS_Mask   0x01

#define LTRCHIP_PS_DATA_Low_Mask   0xFF
#define LTRCHIP_PS_DATA_High_Mask  0x03

/* interrupt mode */
#define LTRCHIP_INT_Output_mode_Mask  0x08
#define LTRCHIP_INT_Output_mode_latch   0x00
#define LTRCHIP_INT_Output_mode_update  0x08

#define LTRCHIP_INT_Pin_polarity_Mask  0x04
#define LTRCHIP_INT_Pin_polarity_0  0x00
#define LTRCHIP_INT_Pin_polarity_1  0x04

#define LTRCHIP_INT_Mode_Mask  0x03
#define LTRCHIP_INT_Mode_No_trig  0x00
#define LTRCHIP_INT_Mode_PS_trig  0x01
#define LTRCHIP_INT_Mode_ALS_trig  0x02
#define LTRCHIP_INT_Mode_Both_trig  0x03

#define LTRCHIP_INTERRUPT_PERSIST_PS_Mask   0xF0
#define LTRCHIP_INTERRUPT_PERSIST_PS_0      0x00
#define LTRCHIP_INTERRUPT_PERSIST_PS_1      0x10
#define LTRCHIP_INTERRUPT_PERSIST_PS_2      0x20
#define LTRCHIP_INTERRUPT_PERSIST_PS_3      0x30
#define LTRCHIP_INTERRUPT_PERSIST_PS_4      0x40
#define LTRCHIP_INTERRUPT_PERSIST_PS_5      0x50
#define LTRCHIP_INTERRUPT_PERSIST_PS_6      0x60
#define LTRCHIP_INTERRUPT_PERSIST_PS_7      0x70
#define LTRCHIP_INTERRUPT_PERSIST_PS_8      0x80
#define LTRCHIP_INTERRUPT_PERSIST_PS_9      0x90
#define LTRCHIP_INTERRUPT_PERSIST_PS_10     0xA0
#define LTRCHIP_INTERRUPT_PERSIST_PS_11     0xB0
#define LTRCHIP_INTERRUPT_PERSIST_PS_12     0xE0
#define LTRCHIP_INTERRUPT_PERSIST_PS_13     0xD0
#define LTRCHIP_INTERRUPT_PERSIST_PS_14     0xE0
#define LTRCHIP_INTERRUPT_PERSIST_PS_15     0xF0

#define LTRCHIP_INTERRUPT_PERSIST_ALS_Mask  0x0F
#define LTRCHIP_INTERRUPT_PERSIST_ALS_0      0x00
#define LTRCHIP_INTERRUPT_PERSIST_ALS_1      0x01
#define LTRCHIP_INTERRUPT_PERSIST_ALS_2      0x02
#define LTRCHIP_INTERRUPT_PERSIST_ALS_3      0x03
#define LTRCHIP_INTERRUPT_PERSIST_ALS_4      0x04
#define LTRCHIP_INTERRUPT_PERSIST_ALS_5      0x05
#define LTRCHIP_INTERRUPT_PERSIST_ALS_6      0x06
#define LTRCHIP_INTERRUPT_PERSIST_ALS_7      0x07
#define LTRCHIP_INTERRUPT_PERSIST_ALS_8      0x08
#define LTRCHIP_INTERRUPT_PERSIST_ALS_9      0x09
#define LTRCHIP_INTERRUPT_PERSIST_ALS_10     0x0A
#define LTRCHIP_INTERRUPT_PERSIST_ALS_11     0x0B
#define LTRCHIP_INTERRUPT_PERSIST_ALS_12     0x0E
#define LTRCHIP_INTERRUPT_PERSIST_ALS_13     0x0D
#define LTRCHIP_INTERRUPT_PERSIST_ALS_14     0x0E
#define LTRCHIP_INTERRUPT_PERSIST_ALS_15     0x0F

//#define LTR558_INT_CONFIG    0x01 // interrupt pin active till cleared, active high
#define LTR558_INT_CONFIG    0x00 // interrupt pin active till cleared, active low
#define LTR558_PWM_SENS      0x00 // Standard: 0x00=standard, 0x01=1/2, 0x10=1/4, 0x11=1/8
#define LTR558_PWM_RES       0x01 // 8bit: 0x00=7bit, 0x01=7bit, 0x10=9bit, 0x11=10bit 
#define LTR558_PWM_ENABLE    0x00 // not enabled.
#define LTR558_PWM_POLARITY  0x01 // Positive: 0x00=neg, 0x01=pos
#define LTR558_PWM_TYPE      0x01 // log: 0x00=lin,0x01=log
#define LTR558_ALS_HYST_TRIGGER (1<<4) // Upper: 0x00=Lowwer, 0x01=Upper
#define LTR558_ALS_HYST_ENABLE  0x00 // Disabled 0x00=Disabled, 0x01=Enabled
#define LTR558_ALS_INTEGRATION_TIME  0X04 //100ms: 0x000=6.25ms, 0x001=12.5ms,0x010=25ms,0x011=50ms,0x100=200ms,0x110=400,0x111=800ms
#define LTR558_ALS_THRES_LO	0
#define LTR558_ALS_THRES_UP	65535
#define LTR558_ALS_CONTROL   0x02  // Repeat Mode 0x01=One Shot
#define LTR558_ALS_INTERVAL  500 // Valid values 0 - 3150 in steps of 50.

#define LTR558_PS_FILTER_M 1 // 1 - 15
#define LTR558_PS_FILTER_N (1<<4) // 1 - 15
#define LTR558_PS_INTEGRATION_TIME 0x20 // 300us: 0x00=75us, 0x01=150us, 0x10=300us, 0x11=600us
#define LTR558_PS_HYST_TRIGGER (1<<4) // Upper: 0x00=Lowwer, 0x01=Upper
#define LTR558_PS_HYST_ENABLE  0x00 // Disabled 0x00=Disabled, 0x01=Enabled
#define LTR558_PS_THRES_LO	1200
#define LTR558_PS_THRES_UP	1500
#define LTR558_PS_INTERVAL  100 // 100ms: Valid values 0 - 3150 in steps of 50.
#define LTR558_PS_LED_CURRENT 0x10 // 85ma  LTR558_PS_LED_CURRENT * 5 + 5
#define LTR558_PS_MODE LTRCHIP_PS_REPEAT 


//---------------------------------------


#define LTR_INT_GPIO 17
#define LTR_TAG        "[ltr]"

// device name/id/address/counts
#define LTR_DEVICE_NAME		"ltr"
//#define LTR_DEVICE_ID			"tritonFN"
#define LTR_DEVICE_ID                "ltr"
#define LTR_ID_NAME_SIZE		10
#define LTR_TRITON_CHIPIDVAL   	0x00
#define LTR_TRITON_MAXREGS     	32
#define LTR_DEVICE_ADDR1		0x29
#define LTR_DEVICE_ADDR2       	0x39
#define LTR_DEVICE_ADDR3       	0x49
#define LTR_MAX_NUM_DEVICES		3
#define I2C_MAX_ADAPTERS		8

// TRITON register offsets
#define LTR_TRITON_CNTRL 		0x00
#define LTR_TRITON_ALS_TIME 		0X01
#define LTR_TRITON_PRX_TIME		0x02
#define LTR_TRITON_WAIT_TIME		0x03
#define LTR_TRITON_ALS_MINTHRESHLO	0X04
#define LTR_TRITON_ALS_MINTHRESHHI 	0X05
#define LTR_TRITON_ALS_MAXTHRESHLO	0X06
#define LTR_TRITON_ALS_MAXTHRESHHI	0X07
#define LTR_TRITON_PRX_MINTHRESHLO 	0X08
#define LTR_TRITON_PRX_MINTHRESHHI 	0X09
#define LTR_TRITON_PRX_MAXTHRESHLO 	0X0A
#define LTR_TRITON_PRX_MAXTHRESHHI 	0X0B
#define LTR_TRITON_INTERRUPT		0x0C
#define LTR_TRITON_PRX_CFG		0x0D
#define LTR_TRITON_PRX_COUNT		0x0E
#define LTR_TRITON_GAIN		0x0F
#define LTR_TRITON_REVID		0x11
#define LTR_TRITON_CHIPID      	0x12
#define LTR_TRITON_STATUS		0x13
#define LTR_TRITON_ALS_CHAN0LO		0x14
#define LTR_TRITON_ALS_CHAN0HI		0x15
#define LTR_TRITON_ALS_CHAN1LO		0x16
#define LTR_TRITON_ALS_CHAN1HI		0x17
#define LTR_TRITON_PRX_LO		0x18
#define LTR_TRITON_PRX_HI		0x19
#define LTR_TRITON_TEST_STATUS		0x1F

// Triton cmd reg masks
#define LTR_TRITON_CMD_REG		0X80
#define LTR_TRITON_CMD_BYTE_RW		0x00
#define LTR_TRITON_CMD_WORD_BLK_RW	0x20
#define LTR_TRITON_CMD_SPL_FN		0x60
#define LTR_TRITON_CMD_PROX_INTCLR	0X05
#define LTR_TRITON_CMD_ALS_INTCLR	0X06
#define LTR_TRITON_CMD_PROXALS_INTCLR 	0X07
#define LTR_TRITON_CMD_TST_REG		0X08
#define LTR_TRITON_CMD_USER_REG	0X09

// Triton cntrl reg masks
#define LTR_TRITON_CNTL_PROX_INT_ENBL	0X20
#define LTR_TRITON_CNTL_ALS_INT_ENBL	0X10
#define LTR_TRITON_CNTL_WAIT_TMR_ENBL	0X08
#define LTR_TRITON_CNTL_PROX_DET_ENBL	0X04
#define LTR_TRITON_CNTL_ADC_ENBL	0x02
#define LTR_TRITON_CNTL_PWRON		0x01

// Triton status reg masks
#define LTR_TRITON_STATUS_ADCVALID	0x01
#define LTR_TRITON_STATUS_PRXVALID	0x02
#define LTR_TRITON_STATUS_ADCINTR	0x10
#define LTR_TRITON_STATUS_PRXINTR	0x20

// lux constants
//[sensor wlg 20110729]ALS thereshold modify
//#define	LTR_MAX_LUX			65535000
#define	LTR_MAX_LUX			65535
#define LTR_SCALE_MILLILUX		3
#define LTR_FILTER_DEPTH		3
#define THRES_LO_TO_HI_RATIO  4/5
// forward declarations
static int ltr_probe(struct i2c_client *clientp, const struct i2c_device_id *idp);
static int ltr_remove(struct i2c_client *client);
static int ltr_open(struct inode *inode, struct file *file);
static int ltr_release(struct inode *inode, struct file *file);
static long ltr_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int ltr_read(struct file *file, char *buf, size_t count, loff_t *ppos);
static int ltr_write(struct file *file, const char *buf, size_t count, loff_t *ppos);
static loff_t ltr_llseek(struct file *file, loff_t offset, int orig);
static int ltr_get_lux(void);
static int ltr_lux_filter(int raw_lux);
static int ltr_device_name(unsigned char *bufp, char **device_name);
static int ltr_prox_poll(struct taos_prox_info *prxp);
//static void ltr_prox_poll_timer_func(unsigned long param);
//static void ltr_prox_poll_timer_start(void);
//static void ltr_als_work(struct work_struct *w);
static void do_ltr_work(struct work_struct *w);
static void ltr_report_value(int mask);
static int ltr_calc_distance(int value);
static int enable_light_and_proximity(int mask);	
//static void ltr_chip_diff_settings(void);
static int light_on=0;  
static int prox_on = 0;

enum ltr_chip_type {
	TSL2771 = 0,
	TMD2771,	
};

struct alsltr_prox_data {
	struct input_dev *input_dev;
};

static struct alsltr_prox_data *light;
static struct alsltr_prox_data *proximity;
// first device number
static dev_t ltr_dev_number;

// class structure for this device
struct class *ltr_class;

// module device table
static struct i2c_device_id ltr_idtable[] = {
        {LTR_DEVICE_ID, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, ltr_idtable);

// board and address info
struct i2c_board_info ltr_board_info[] = {
	{I2C_BOARD_INFO(LTR_DEVICE_ID, LTR_DEVICE_ADDR1),},
	{I2C_BOARD_INFO(LTR_DEVICE_ID, LTR_DEVICE_ADDR2),},
	{I2C_BOARD_INFO(LTR_DEVICE_ID, LTR_DEVICE_ADDR3),},
};
unsigned short const ltr_addr_list[4] = {LTR_DEVICE_ADDR1, LTR_DEVICE_ADDR2, LTR_DEVICE_ADDR3, I2C_CLIENT_END};

// client and device
struct i2c_client *ltr_my_clientp;
struct i2c_client *ltr_bad_clientp[LTR_MAX_NUM_DEVICES];
//static int num_bad = 0;
//static int device_found = 0;

// driver definition
static struct i2c_driver ltr_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "ltr",
	 },
	.id_table = ltr_idtable,
	.probe = ltr_probe,
	.remove = __devexit_p(ltr_remove),
};

struct ltr_intr_data {
    int int_gpio;
    int irq;
};

// per-device data
struct ltr_data {
	struct i2c_client *client;
	struct cdev cdev;
	unsigned int addr;
	char ltr_id;
	char ltr_name[LTR_ID_NAME_SIZE];
	struct semaphore update_lock;
	char valid;
	unsigned long last_updated;
    	struct ltr_intr_data *pdata;
	struct work_struct  ltr_work;
	enum ltr_chip_type chip_type;
	struct mutex proximity_calibrating;
} *ltr_datap;

static struct ltr_intr_data ltr_irq= {
    .int_gpio = LTR_INT_GPIO,
    .irq = MSM_GPIO_TO_INT(LTR_INT_GPIO),
};


// file operations
static struct file_operations ltr_fops = {
	.owner = THIS_MODULE,
	.open = ltr_open,
	.release = ltr_release,
	.read = ltr_read,
	.write = ltr_write,
	.llseek = ltr_llseek,
	.unlocked_ioctl = ltr_ioctl,
};

// device configuration
struct taos_cfg *ltr_cfgp;
static u32 ltr_calibrate_target_param = 300000;
static u16 ltr_als_time_param = 27;
static u16 ltr_scale_factor_param = 1;
static u8 ltr_filter_history_param = 3;
static u8 ltr_filter_count_param = 1;
static u8 ltr_gain_param = 1;

static u16 ltr_gain_trim_param = 2; //this value is set according to specific device

static u16 ltr_prox_threshold_hi_param = 240; 
static u16 ltr_prox_threshold_lo_param = 190;
static u8 ltr_prox_int_time_param = 0xF6;
static u8 ltr_prox_adc_time_param = 0xFF;
static u8 ltr_prox_wait_time_param = 0xFF;
static u8 ltr_prox_intr_filter_param = 0x00;
static u8 ltr_prox_config_param = 0x00;

static u8 ltr_prox_pulse_cnt_param = 0x08;
static u8 ltr_prox_ltr_gain_param = 0x20;

// device reg init values
u8 ltr_triton_reg_init[16] = {0x00,0xFF,0XFF,0XFF,0X00,0X00,0XFF,0XFF,0X00,0X00,0XFF,0XFF,0X00,0X00,0X00,0X00};

//
static u16 ltr_als_intr_threshold_hi_param = 0;
static u16 ltr_als_intr_threshold_lo_param = 0;
int ltr_g_nlux = 0;
//static int AlsorProx=0;
		

// prox info
struct taos_prox_info ltr_prox_cal_info[20];
struct taos_prox_info ltr_prox_cur_info;
struct taos_prox_info *ltr_prox_cur_infop = &ltr_prox_cur_info;
//static u8 prox_history_hi = 0;
//static u8 prox_history_lo = 0;
//static struct timer_list prox_poll_timer;

static int ltr_device_released = 0;
static u16 ltr_sat_als = 0;
static u16 ltr_sat_prox = 0x07FF;

// lux time scale
struct ltr_time_scale_factor  {
	u16	numerator;
	u16	denominator;
	u16	saturation;
};
struct ltr_time_scale_factor ltr_TritonTime = {1, 0, 0};
struct ltr_time_scale_factor *ltr_lux_timep = &ltr_TritonTime;

// gain table
u8 ltr_triton_gain_table[] = {1, 8, 16, 120};

// lux data
struct ltr_lux_data {
	u16	ratio;
	long	ir;
	long	clear;
};
struct ltr_lux_data TritonFN_ltr_lux_data[] = {
        { 115, -11059, 17743 },
        { 163, 13363,  37725 },
        { 217, 1690,   16900 },
        { 0,   0,     0     }
};
struct ltr_lux_data *ltr_lux_tablep = TritonFN_ltr_lux_data;
static int ltr_lux_history[LTR_FILTER_DEPTH] = {-ENODATA, -ENODATA, -ENODATA};

//prox data
struct ltr_prox_data {
	u16	ratio;
	u16	hi;
	u16	lo;
};
struct ltr_prox_data TritonFN_ltr_prox_data[] = {
        { 10, 14,  12 },      
        { 0,  0,   0 }
};
struct ltr_prox_data *ltr_prox_tablep = TritonFN_ltr_prox_data;

/* ----------------------*
* config gpio for intr utility        *
*-----------------------*/
int ltr_config_int_gpio(int int_gpio)
{
    int rc=0;
    uint32_t  gpio_config_data = GPIO_CFG(int_gpio,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA);

    rc = gpio_request(int_gpio, "gpio_sensor");
    if (rc) {
        printk(LTR_TAG "%s: gpio_request(%#x)=%d\n",
                __func__, int_gpio, rc);
        return rc;
    }

    rc = gpio_tlmm_config(gpio_config_data, GPIO_CFG_ENABLE);
    if (rc) {
        printk(LTR_TAG "%s: gpio_tlmm_config(%#x)=%d\n",
                __func__, gpio_config_data, rc);
        return rc;
    }

    mdelay(1);

    rc = gpio_direction_input(int_gpio);
    if (rc) {
        printk(LTR_TAG "%s: gpio_direction_input(%#x)=%d\n",
                __func__, int_gpio, rc);
        return rc;
    }

    return 0;
}

/* ----------------------*
* ltr interrupt function         *
*-----------------------*/
static irqreturn_t ltr_interrupt(int irq, void *data)
{
    //printk(LTR_TAG "ltr_interrupt\n");	
    disable_irq_nosync(ltr_datap->pdata->irq);
    schedule_work(&ltr_datap->ltr_work);
    
    return IRQ_HANDLED;
}

static void do_ltr_work(struct work_struct *w)
{

    int ret =0;	
    //int prx_hi, prx_lo;
    u16 status = 0;	
	
    status = i2c_smbus_read_byte_data(ltr_datap->client,ltr_regmap[LTRCHIP_ALS_PS_STATUS]);//add error ctl
    printk(LTR_TAG "LTR_interrupt status = %x\n",status);	
    if(status<0)
	goto read_reg_fail;

    //als interrupt
    if(((status & LTRCHIP_ALS_INT_STATUS_Mask) != 0)/*&&((status & NOACHIP_INT_EXTERNAL) != 0)*/)
    {
	 ret = ltr_get_lux();
	 if(ret >= 0){
	    ltr_g_nlux = ret;
	 }
	 printk(LTR_TAG "ltr_als_intr_threshold_hi_param=%x,ltr_als_intr_threshold_lo_param=%x\n",ltr_als_intr_threshold_hi_param,ltr_als_intr_threshold_lo_param);
 	 if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_TH_UP_Low], ltr_als_intr_threshold_hi_param))) < 0) {        	        
		pr_crit(LTR_TAG "i2c_smbus_write_byte() to LTR_TRITON_ALS_MAXTHRESHLO\n");                	
       	}           
 	 if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_TH_LO_Low], ltr_als_intr_threshold_lo_param))) < 0) {        	        
		pr_crit(LTR_TAG "i2c_smbus_write_byte() to LTR_TRITON_ALS_MAXTHRESHLO\n");                	
       	}           
     ltr_report_value(0);
    }	

    //prox interrupt
    if(((status & (LTRCHIP_PS_INT_STATUS_Mask+LTRCHIP_PS_Data_STATUS_Mask)) != 0)/*&&((status & NOACHIP_INT_EXTERNAL) != 0)*/)
    { 
//    	printk(LTR_TAG "ps ltr_interrupt\n");	
	    if((ret = ltr_prox_poll(ltr_prox_cur_infop))<0){
		  printk(KERN_CRIT "LTR: get prox poll  failed in  ltr interrupt()\n");  
		if(ret==-ENODATA)
			goto turn_screen_on;
		}
	    if(ltr_prox_cur_infop->prox_data > ltr_cfgp->prox_threshold_hi)       
	    {           		
	 	if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_TH_UP_Low], ltr_sat_prox))) < 0) {        	        
			pr_crit(LTR_TAG "i2c_smbus_write_byte() to LTR_TRITON_PRX_MAXTHRESHLO\n");                	
        	}           
	 	if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_TH_LO_Low], ltr_cfgp->prox_threshold_lo))) < 0) {        	        
			pr_crit(LTR_TAG "i2c_smbus_write_byte() to LTR_TRITON_PRX_MAXTHRESHLO\n");                	
        	}           

		ltr_prox_cur_infop->prox_event = 1;           
		//pr_crit(LTR_TAG "screen off:ltr_prox_cur_infop->prox_data=%d\n",ltr_prox_cur_infop->prox_data);            
	} 
	else if(ltr_prox_cur_infop->prox_data < ltr_cfgp->prox_threshold_lo)        
turn_screen_on:
	{            
	 	if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_TH_UP_Low], ltr_cfgp->prox_threshold_hi))) < 0) {        	        
			pr_crit(LTR_TAG "i2c_smbus_write_byte() to LTR_TRITON_PRX_MAXTHRESHLO\n");                	
        	}           
	 	if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_TH_LO_Low], 0))) < 0) {        	        
			pr_crit(LTR_TAG "i2c_smbus_write_byte() to LTR_TRITON_PRX_MAXTHRESHLO\n");                	
        	}  		
		ltr_prox_cur_infop->prox_event = 0;             
		//pr_crit(LTR_TAG "screen on:ltr_prox_cur_infop->prox_data=%d\n",ltr_prox_cur_infop->prox_data);                     
	}   	
	ltr_report_value(1);
	
    }
	
read_reg_fail:
    enable_irq(ltr_datap->pdata->irq);
    return;

}	
		
/*ltr_calc_distance, using prox value*/
static int ltr_calc_distance(int value)
{
	int temp=0;
	if(ltr_prox_cur_infop->prox_event == 1)
		temp=1;
	else if(ltr_prox_cur_infop->prox_event == 0)
		temp=5;
	return temp;
}

/*report als and prox data to input sub system, 
for data poll utility in hardware\alsprox.c
*/
static void ltr_report_value(int mask)
{
	struct taos_prox_info *val = ltr_prox_cur_infop;
	int lux_val=ltr_g_nlux;
	int  dist;
	lux_val*=ltr_cfgp->gain_trim;	

	if (mask==0) {
		input_report_abs(light->input_dev, ABS_MISC, lux_val);
//[sensor wlg 20110729]ALS thereshold modify add log
 		printk(KERN_CRIT "LTR: wlg als_interrupt lux_val(%d)=ltr_g_nlux(%d)/ltr_cfgp->gain_trim(%d)\n", lux_val, ltr_g_nlux, ltr_cfgp->gain_trim);
		input_sync(light->input_dev);
	}

	if (mask==1) {
		dist=ltr_calc_distance(val->prox_data);
		if( (dist < 5) && (lux_val > 10000)){
		    dist = 6;
		}
		//input_report_als(alsprox->input_dev, ALS_PROX, val->prox_clear);
		input_report_abs(proximity->input_dev, ABS_DISTANCE, dist);
		//input_report_als(alsprox->input_dev, ALS_PROX, val->prox_event);
		printk(KERN_CRIT "LTR: wlg prox_interrupt =%d, distance=%d\n",  val->prox_data,dist);
		input_sync(proximity->input_dev);
	}

	//input_sync(alsprox->input_dev);	
	//enable_irq(ltr_datap->pdata->irq);
}



/* ------------*
* driver init        *
*------------*/
static int __init ltr_init(void) {
	int ret = 0;
	//struct i2c_adapter *my_adap;
	printk(KERN_ERR "LTR: comes into ltr_init\n");
	if ((ret = (alloc_chrdev_region(&ltr_dev_number, 0, LTR_MAX_NUM_DEVICES, LTR_DEVICE_NAME))) < 0) {
		printk(KERN_ERR "LTR: alloc_chrdev_region() failed in ltr_init()\n");
                return (ret);
	}
        ltr_class = class_create(THIS_MODULE, LTR_DEVICE_NAME);
        ltr_datap = kmalloc(sizeof(struct ltr_data), GFP_KERNEL);
        if (!ltr_datap) {
		printk(KERN_ERR "LTR: kmalloc for struct ltr_data failed in ltr_init()\n");
                return -ENOMEM;
	}
        memset(ltr_datap, 0, sizeof(struct ltr_data));
        cdev_init(&ltr_datap->cdev, &ltr_fops);
        ltr_datap->cdev.owner = THIS_MODULE;
        if ((ret = (cdev_add(&ltr_datap->cdev, ltr_dev_number, 1))) < 0) {
		printk(KERN_ERR "LTR: cdev_add() failed in ltr_init()\n");
                return (ret);
	}
	//device_create(ltr_class, NULL, MKDEV(MAJOR(ltr_dev_number), 0), &ltr_driver ,"ltr");
        ret = i2c_add_driver(&ltr_driver);
	if(ret){
		printk(KERN_ERR "LTR: i2c_add_driver() failed in ltr_init(),%d\n",ret);
                return (ret);
	}
    	//pr_crit(LTR_TAG "%s:%d\n",__func__,ret);
        return (ret);
}



// driver exit
static void __exit ltr_exit(void) {
/*	if (ltr_my_clientp)
		i2c_unregister_device(ltr_my_clientp);
	*/
        i2c_del_driver(&ltr_driver);
        unregister_chrdev_region(ltr_dev_number, LTR_MAX_NUM_DEVICES);
	device_destroy(ltr_class, MKDEV(MAJOR(ltr_dev_number), 0));
	cdev_del(&ltr_datap->cdev);
	class_destroy(ltr_class);
	mutex_destroy(&ltr_datap->proximity_calibrating);
        kfree(ltr_datap);
}


//***************************************************
/*static struct file_operations ltr_device_fops = {
	.owner = THIS_MODULE,
	.open = ltr_open,
	.release = ltr_release,
	.unlocked_ioctl = ltr_ioctl,
};


static struct miscdevice ltr_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ltr",
	.fops = &ltr_device_fops,
};
*/

// client probe
static int ltr_probe(struct i2c_client *clientp, const struct i2c_device_id *idp) {
	int ret = 0;
	int i = 0;
	unsigned char buf[LTR_MAX_DEVICE_REGS];
	char *device_name;

//	if (device_found)
//		return -ENODEV;
	if (!i2c_check_functionality(clientp->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		printk(KERN_ERR "LTR: ltr_probe() - i2c smbus byte data functions unsupported\n");
		return -EOPNOTSUPP;
		}
    if (!i2c_check_functionality(clientp->adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		printk(KERN_ERR "LTR: ltr_probe() - i2c smbus word data functions unsupported\n");
        }
    if (!i2c_check_functionality(clientp->adapter, I2C_FUNC_SMBUS_BLOCK_DATA)) {
		printk(KERN_ERR "LTR: ltr_probe() - i2c smbus block data functions unsupported\n");
        }
	ltr_datap->client = clientp;
	i2c_set_clientdata(clientp, ltr_datap);
	
	//write bytes to address control registers
    for(i = 0; i < LTR_MAX_DEVICE_REGS; i++) {
		buf[i] = i2c_smbus_read_byte_data(ltr_datap->client, ltr_regmap[i]);
		if(buf[i] < 0)
		{
			printk(KERN_ERR "LTR: i2c_smbus_read_byte_data(%d) failed in LTR_probe()\n",ltr_regmap[i]);
		}
//		else
//		{
//			printk(KERN_ERR "LTR: i2c_smbus_read_byte_data(%d) = %d in LTR_probe()\n", ltr_regmap[i], buf[i]);
//	    }	
	}	
	
	//check device ID "tritonFN"
	if ((ret = ltr_device_name(buf, &device_name)) == 0) {
		printk(KERN_ERR "LTR: chip id that was read found mismatched by ltr_device_name(), in ltr_probe()\n");
 		return -ENODEV;
        }
	if (strcmp(device_name, LTR_DEVICE_ID)) {
		printk(KERN_ERR "LTR: chip id that was read does not match expected id in ltr_probe()\n");
		return -ENODEV;
        }
	else{
		printk(KERN_ERR "LTR: chip id of %s that was read matches expected id in ltr_probe()\n", device_name);
		//device_found = 1;
	}
	device_create(ltr_class, NULL, MKDEV(MAJOR(ltr_dev_number), 0), &ltr_driver ,"ltr");

	printk(KERN_ERR "LTR: LTR_probe() init regs.\n");
//	noa3402_reset(chip, 0x01);
//	 	if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[NOACHIP_RESET], 0x01))) < 0) {        	        
//			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[NOACHIP_RESET],0x01);                	
//            return(ret);
//			}           
//init PS
//	noa3402_ps_mode(chip, NOACHIP_STANDBY);
	 	if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_CONTR], (LTRCHIP_PS_Mode_Standby+LTRCHIP_PS_Gain_4)))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_PS_CONTR],(LTRCHIP_PS_Mode_Standby+LTRCHIP_PS_Gain_4));                	
            return(ret);
			}           
	 	if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_CONTR], (LTRCHIP_ALS_Mode_Standby+LTRCHIP_ALS_Gain_Range1)))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_ALS_CONTR],(LTRCHIP_ALS_Mode_Standby+LTRCHIP_ALS_Gain_Range1));                	
            return(ret);
			}           

	 	if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_LED], (LTRCHIP_PS_LED_Freq_60KHz+LTRCHIP_PS_LED_Duty_100Per+LTRCHIP_PS_LED_Current_100mA)))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_PS_LED],(LTRCHIP_PS_LED_Freq_60KHz+LTRCHIP_PS_LED_Duty_100Per+LTRCHIP_PS_LED_Current_100mA));                	
            return(ret);
			}           

    	if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_N_PULSES], 200))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_PS_N_PULSES],200);                	
            return(ret);
			}           

	 	if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_MEAS_RATE], LTRCHIP_PS_MEAS_RATE_100ms))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_PS_MEAS_RATE],LTRCHIP_PS_MEAS_RATE_100ms);                	
            return(ret);
			}           

		if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_MEAS_RATE], (LTRCHIP_ALS_Inte_Time_400ms+LTRCHIP_ALS_MEAS_RATE_500ms)))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_ALS_MEAS_RATE],(LTRCHIP_ALS_Inte_Time_400ms+LTRCHIP_ALS_MEAS_RATE_500ms));                	
            return(ret);
			}           

			
//inter
		if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_TH_UP_Low], 0x0))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_PS_TH_UP_Low],0x0);                	
            return(ret);
			}           
	 	if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_TH_LO_Low], 1))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_PS_TH_LO_Low],1);                	
            return(ret);
			}           
			
		if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_TH_UP_Low], 0x0))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_ALS_TH_UP_Low],0x0);                	
            return(ret);
			}           
	 	if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_TH_LO_Low], 1))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_ALS_TH_LO_Low],1);                	
            return(ret);
			}           

		if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_INT_CONFIG], (LTRCHIP_INT_Output_mode_update+LTRCHIP_INT_Pin_polarity_0+LTRCHIP_INT_Mode_Both_trig)))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_INT_CONFIG],(LTRCHIP_INT_Output_mode_update+LTRCHIP_INT_Pin_polarity_0+LTRCHIP_INT_Mode_Both_trig));                	
            return(ret);
			}           
	 	if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_INTERRUPT_PERSIST], 0x00))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_INTERRUPT_PERSIST],0x00);                	
            return(ret);
			}           

	//write bytes to address control registers
//    for(i = 0; i < LTR_MAX_DEVICE_REGS; i++) {
//		buf[i] = i2c_smbus_read_byte_data(ltr_datap->client, ltr_regmap[i]);
//		if(buf[i] < 0)
//		{
//			printk(KERN_ERR "LTR: i2c_smbus_read_byte_data(%d) failed in LTR_probe()\n",ltr_regmap[i]);
//		}
//		else
//		{
//			printk(KERN_ERR "LTR: i2c_smbus_read_byte_data(%d) = %d in LTR_probe()\n", ltr_regmap[i], buf[i]);
//	    }	
//	}	
	
	INIT_WORK(&ltr_datap->ltr_work, do_ltr_work); 
	mutex_init(&ltr_datap->proximity_calibrating);
	ltr_datap->pdata = &ltr_irq;
	if(clientp->irq){		
		ltr_datap->pdata->irq=ltr_datap->client->irq;
		ltr_datap->pdata->int_gpio=INT_TO_MSM_GPIO(ltr_datap->pdata->irq);
		}
	printk(KERN_CRIT "ltr use gpio %d\n",ltr_datap->pdata->int_gpio);
    	ret=ltr_config_int_gpio(ltr_datap->pdata->int_gpio);
    	if (ret) {
		printk(KERN_CRIT "ltr configure int_gpio%d failed\n",
                ltr_datap->pdata->int_gpio);
        	return ret;
    	}

    	ret = request_irq(ltr_datap->pdata->irq, ltr_interrupt, IRQF_TRIGGER_FALLING, 
			ltr_datap->ltr_name, ltr_prox_cur_infop);
    	if (ret) {
		printk(KERN_CRIT "ltr request interrupt failed\n");
        	return ret;
    	}
	
	strlcpy(clientp->name, LTR_DEVICE_ID, I2C_NAME_SIZE);
	strlcpy(ltr_datap->ltr_name, LTR_DEVICE_ID, LTR_ID_NAME_SIZE);
	ltr_datap->valid = 0;
	
	init_MUTEX(&ltr_datap->update_lock);
	if (!(ltr_cfgp = kmalloc(sizeof(struct taos_cfg), GFP_KERNEL))) {
		printk(KERN_ERR "LTR: kmalloc for struct taos_cfg failed in ltr_probe()\n");
		return -ENOMEM;
	}	

	//update settings
	//this should behind ltr_device_name	
	ltr_cfgp->calibrate_target = ltr_calibrate_target_param;
	ltr_cfgp->als_time = ltr_als_time_param;
	ltr_cfgp->scale_factor = ltr_scale_factor_param;
	ltr_cfgp->gain_trim = ltr_gain_trim_param;
	ltr_cfgp->filter_history = ltr_filter_history_param;
	ltr_cfgp->filter_count = ltr_filter_count_param;
	ltr_cfgp->gain = ltr_gain_param;
	ltr_cfgp->prox_threshold_hi = ltr_prox_threshold_hi_param;
	ltr_cfgp->prox_threshold_lo = ltr_prox_threshold_lo_param;
	ltr_cfgp->prox_int_time = ltr_prox_int_time_param;
	ltr_cfgp->prox_adc_time = ltr_prox_adc_time_param;
	ltr_cfgp->prox_wait_time = ltr_prox_wait_time_param;
	ltr_cfgp->prox_intr_filter = ltr_prox_intr_filter_param;
	ltr_cfgp->prox_config = ltr_prox_config_param;
	ltr_cfgp->prox_pulse_cnt = ltr_prox_pulse_cnt_param;
	ltr_cfgp->prox_gain = ltr_prox_ltr_gain_param;
	ltr_sat_als = (256 - ltr_cfgp->prox_int_time) << 10; //10240
	//ltr_sat_prox = (256 - ltr_cfgp->prox_adc_time) << 10; //1024

	light = kzalloc(sizeof(struct alsltr_prox_data), GFP_KERNEL);
	if (!light) {
		ret = -ENOMEM;
		goto exit_alloc_data_failed;
	}
	proximity= kzalloc(sizeof(struct alsltr_prox_data), GFP_KERNEL);
	if (!proximity) {
		ret = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	light->input_dev = input_allocate_device();
	if (!light->input_dev) {
		ret = -ENOMEM;
		printk(KERN_ERR "ltr_probe: Failed to allocate light input device\n");
		goto exit_input_dev_alloc_failed;
	}
	proximity->input_dev = input_allocate_device();
	if (!proximity->input_dev) {
		ret = -ENOMEM;
		printk(KERN_ERR "ltr_probe: Failed to allocate prox input device\n");
		goto exit_input_dev_alloc_failed;
	}
	
	/* lux */
	set_bit(EV_ABS, light->input_dev->evbit);
	input_set_abs_params(light->input_dev,  ABS_MISC, 0, 65535, 0, 0);
	light->input_dev->name = "light";
	/* prox */
	set_bit(EV_ABS, proximity->input_dev->evbit);
	input_set_abs_params(proximity->input_dev, ABS_DISTANCE, 0, 65535, 0, 0);
	proximity->input_dev->name = "proximity";
	
	ret = input_register_device(light->input_dev);
	if (ret) {
		printk("ltr_probe: Unable to register input device: %s\n", light->input_dev->name);
		goto exit_input_register_device_failed;
	}
	ret = input_register_device(proximity->input_dev);
	if (ret) {
		printk("ltr_probe: Unable to register input device: %s\n", proximity->input_dev->name);
		goto exit_input_register_device_failed;
	}
	
	return (ret);
//exit_ltr_device_register_failed:	
exit_input_register_device_failed:
	if(light->input_dev)
		input_free_device(light->input_dev);
	if(proximity->input_dev)
		input_free_device(proximity->input_dev);
exit_input_dev_alloc_failed:
exit_alloc_data_failed:
	if(light)
		kfree(light);
	if(proximity)
		kfree(proximity);
	return (ret);		
}


// client remove
static int __devexit ltr_remove(struct i2c_client *client) {
	int ret = 0;

	return (ret);
}

// open
static int ltr_open(struct inode *inode, struct file *file) {
	struct ltr_data *ltr_datap;
	int ret = 0;
	ltr_device_released = 0;
	ltr_datap = container_of(inode->i_cdev, struct ltr_data, cdev);
	if (strcmp(ltr_datap->ltr_name, LTR_DEVICE_ID) != 0) {
		printk(KERN_ERR "LTR: device name incorrect during ltr_open(), get %s\n", ltr_datap->ltr_name);
		ret = -ENODEV;
	}
	return (ret);
}

// release
static int ltr_release(struct inode *inode, struct file *file) {
	struct ltr_data *ltr_datap;
	int ret = 0;

	ltr_device_released = 1;
	ltr_datap = container_of(inode->i_cdev, struct ltr_data, cdev);	
	if (strcmp(ltr_datap->ltr_name, LTR_DEVICE_ID) != 0) {
		printk(KERN_ERR "LTR: device name incorrect during ltr_release(), get %s\n", ltr_datap->ltr_name);
		ret = -ENODEV;
	}
	return (ret);
}

// read
static int ltr_read(struct file *file, char *buf, size_t count, loff_t *ppos) {
	struct ltr_data *ltr_datap;
	u8 i = 0, xfrd = 0, reg = 0;
	u8 my_buf[LTR_MAX_DEVICE_REGS];
	int ret = 0;

	//if (prox_on)
		//del_timer(&prox_poll_timer);
        if ((*ppos < 0) || (*ppos >= LTR_MAX_DEVICE_REGS)  || ((*ppos + count) > LTR_MAX_DEVICE_REGS)) {
		printk(KERN_ERR "LTR: reg limit check failed in ltr_read()\n");
		return -EINVAL;
	}
	reg = (u8)*ppos;
	ltr_datap = container_of(file->f_dentry->d_inode->i_cdev, struct ltr_data, cdev);
	while (xfrd < count) {
		    if ((ret = (i2c_smbus_read_byte_data(ltr_datap->client, ltr_regmap[reg]))) < 0) {
                printk(KERN_ERR "LTR: i2c_smbus_write_byte() to als/prox data reg failed in ltr_prox_poll()\n");
                return (ret);
            }

      		my_buf[i++] = ret;
			reg++;
			xfrd++;
        }
        if ((ret = copy_to_user(buf, my_buf, xfrd))){
		printk(KERN_ERR "LTR: copy_to_user failed in ltr_read()\n");
		return -ENODATA;
	}
	//if (prox_on)
		//ltr_prox_poll_timer_start();
        return ((int)xfrd);
}

// write
static int ltr_write(struct file *file, const char *buf, size_t count, loff_t *ppos) {
	struct ltr_data *ltr_datap;
	u8 i = 0, xfrd = 0, reg = 0;
	u8 my_buf[LTR_MAX_DEVICE_REGS];
	int ret = 0;

	//if (prox_on)
		//del_timer(&prox_poll_timer);
        if ((*ppos < 0) || (*ppos >= LTR_MAX_DEVICE_REGS) || ((*ppos + count) > LTR_MAX_DEVICE_REGS)) {
		printk(KERN_ERR "LTR: reg limit check failed in ltr_write()\n");
		return -EINVAL;
	}
	reg = (u8)*ppos;
        if ((ret =  copy_from_user(my_buf, buf, count))) {
		printk(KERN_ERR "LTR: copy_to_user failed in ltr_write()\n");
 		return -ENODATA;
	}
        ltr_datap = container_of(file->f_dentry->d_inode->i_cdev, struct ltr_data, cdev);
        while (xfrd < count) {
                if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[reg], my_buf[i++]))) < 0) {
                        printk(KERN_ERR "LTR: i2c_smbus_write_byte_data failed in ltr_write()\n");
                        return (ret);
                }
                reg++;
                xfrd++;
        }
	//if (prox_on)
		//ltr_prox_poll_timer_start();
        return ((int)xfrd);
}

// llseek
static loff_t ltr_llseek(struct file *file, loff_t offset, int orig) {
	int ret = 0;
	loff_t new_pos = 0;

	//if (prox_on)
		//del_timer(&prox_poll_timer);
	if ((offset >= LTR_MAX_DEVICE_REGS) || (orig < 0) || (orig > 1)) {
                printk(KERN_ERR "LTR: offset param limit or origin limit check failed in ltr_llseek()\n");
                return -EINVAL;
        }
        switch (orig) {
        	case 0:
                	new_pos = offset;
                	break;
        	case 1:
                	new_pos = file->f_pos + offset;
	                break;
		default:
			return -EINVAL;
			break;
	}
	if ((new_pos < 0) || (new_pos >= LTR_MAX_DEVICE_REGS) || (ret < 0)) {
		printk(KERN_ERR "LTR: new offset limit or origin limit check failed in ltr_llseek()\n");
		return -EINVAL;
	}
	file->f_pos = new_pos;
       // if (prox_on)
              //  ltr_prox_poll_timer_start();
	return new_pos;
}

/*enable_light_and_proximity, mask values' indication*/
/*10 : light on*/
/*01 : prox on*/
/*20 : light off*/
/*02 : prox off*/
#define Light_turn_on	0x10
#define Prox_turn_on 	0x01
#define Light_turn_off	0x20
#define Prox_turn_off 	0x02
static int enable_light_and_proximity(int mask)
{
	u8 ret = 0;//reg_val = 0; //itime = 0;

	if(mask==Prox_turn_on) 
	{
		pr_crit(LTR_TAG "prox on\n");
		if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_TH_UP_Low], 0x0))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_PS_TH_UP_Low],0x0);                	
            return(ret);
			}           
	 	if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_TH_LO_Low], 1))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_PS_TH_LO_Low],1);                	
            return(ret);
			}           

	 	if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_CONTR], (LTRCHIP_PS_Mode_Active+LTRCHIP_PS_Gain_4)))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_PS_CONTR],(LTRCHIP_PS_Mode_Standby+LTRCHIP_PS_Gain_4));                	
            return(ret);
			}           
        
		return ret;
	}	
	if(mask==Light_turn_on)
	{
		pr_crit(LTR_TAG "light on\n");
	 	if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_CONTR], (LTRCHIP_ALS_Mode_Active+LTRCHIP_ALS_Gain_Range1)))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_ALS_CONTR],(LTRCHIP_ALS_Mode_Standby+LTRCHIP_ALS_Gain_Range1));                	
            return(ret);
			}           
		if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_TH_UP_Low], 0x0))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_ALS_TH_UP_Low],0);                	
            return(ret);
			}           
	 	if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_TH_LO_Low], 1))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_ALS_TH_LO_Low],1);                	
            return(ret);
			}           

		return ret;
	}	
	if(mask==Light_turn_off)
	{
		pr_crit(LTR_TAG "light off\n");
		if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_TH_UP_Low], 0xFFFF))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_ALS_TH_UP_Low],0xFFFF);                	
            return(ret);
			}           
	 	if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_TH_LO_Low], 0))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_ALS_TH_LO_Low],0);                	
            return(ret);
			}           

	 	if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_CONTR], (LTRCHIP_ALS_Mode_Standby+LTRCHIP_ALS_Gain_Range1)))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_ALS_CONTR],(LTRCHIP_ALS_Mode_Standby+LTRCHIP_ALS_Gain_Range1));                	
            return(ret);
			}           

		return ret;
	}
	if(mask==Prox_turn_off)
	{
		pr_crit(LTR_TAG "prox off\n");
		if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_TH_UP_Low], 0x07FF))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_PS_TH_UP_Low],0x07FF);                	
            return(ret);
			}           
	 	if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_TH_LO_Low], 0))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_PS_TH_LO_Low],0);                	
            return(ret);
			}           

		if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_CONTR], (LTRCHIP_PS_Mode_Standby+LTRCHIP_PS_Gain_4)))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_PS_CONTR],(LTRCHIP_PS_Mode_Standby+LTRCHIP_PS_Gain_4));                	
            return(ret);
			}           
		return ret;
	}
	return -1;
}

// ioctls
static long ltr_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	struct ltr_data *ltr_datap;
	int prox_sum = 0, prox_mean = 0, prox_max = 0, prox_min = 0x07FF;
	int lux_val = 0,  i = 0, tmp = 0;
	long ret = 0;
//	u16 gain_trim_val = 0;
//	u8 reg_val = 0, prox_pulse=0, prox_gain=0;
	int count=0;
	struct ltr_prox_data *prox_pt;
	u16 ratio;
	ltr_datap = container_of(file->f_dentry->d_inode->i_cdev, struct ltr_data, cdev);
//[sensor wlg 20110729]ALS thereshold modify add log
//printk(KERN_ERR "LTR_wlg: ltr_ioctl() cmd=%d\n", cmd);
	switch (cmd) {
		case TAOS_IOCTL_ALS_ON:
		    if(1 == prox_on){
			    light_on=1;
				pr_crit(LTR_TAG "TAOS_IOCTL_ALS_ON,when PS on.\n");
			}
			else{
			if(1 == light_on){
			    pr_crit(LTR_TAG "TAOS_IOCTL_ALS_ON: light is already on.\n");
				break;
			}
			ret=enable_light_and_proximity(Light_turn_on);
			    if(ret>=0)
			    {
				    light_on=1;	
            	    pr_crit(LTR_TAG "TAOS_IOCTL_ALS_ON,lux=%d\n", ltr_g_nlux);
			    }
            }			
			return (ret);
			break;
        case TAOS_IOCTL_ALS_OFF:	
		    if(1 == prox_on){
			    light_on=0;
				pr_crit(LTR_TAG "TAOS_IOCTL_ALS_OFF,when PS on.\n");
			}else{
    		if(0 == light_on){
			    pr_crit(LTR_TAG "TAOS_IOCTL_ALS_OFF: light is already off.\n");
				break;
			}
            for (i = 0; i < LTR_FILTER_DEPTH; i++)
                ltr_lux_history[i] = -ENODATA;
			ret=enable_light_and_proximity(Light_turn_off);
			if(ret>=0)
			{
				light_on=0;
				//ltr_g_nlux=0;
            	pr_crit(LTR_TAG"TAOS_IOCTL_ALS_OFF\n");
			}		
            }			
			return (ret);
            break;
		case TAOS_IOCTL_ALS_DATA:
			if ((lux_val = ltr_get_lux()) < 0)
				printk(KERN_ERR "LTR: call to ltr_get_lux() returned error %d in ioctl als_data\n", lux_val);
			lux_val = ltr_lux_filter(lux_val);
			return (lux_val);
			break;
		case TAOS_IOCTL_ALS_CALIBRATE:
			return -ENODATA;
			break;
			
		case TAOS_IOCTL_CONFIG_GET:
			ret = copy_to_user((struct taos_cfg *)arg, ltr_cfgp, sizeof(struct taos_cfg));
			if (ret) {
				printk(KERN_ERR "LTR: copy_to_user failed in ioctl config_get\n");
				return -ENODATA;
			}
			return (ret);
			break;
        case TAOS_IOCTL_CONFIG_SET:
            ret = copy_from_user(ltr_cfgp, (struct taos_cfg *)arg, sizeof(struct taos_cfg));
			if (ret) {
				printk(KERN_ERR "LTR: copy_from_user failed in ioctl config_set\n");
                                return -ENODATA;
			}
    		if(ltr_cfgp->als_time < 50)
		       	ltr_cfgp->als_time = 50;
			if(ltr_cfgp->als_time > 650)
		       	ltr_cfgp->als_time = 650;
			tmp = (ltr_cfgp->als_time + 25)/50;
        		ltr_cfgp->als_time = tmp*50;
		        ltr_sat_als = (256 - ltr_cfgp->prox_int_time) << 10;
	       		//ltr_sat_prox = (256 - ltr_cfgp->prox_adc_time) << 10;
			if(!ltr_cfgp->prox_pulse_cnt)
				ltr_cfgp->prox_pulse_cnt=ltr_prox_pulse_cnt_param;
            break;
		case TAOS_IOCTL_PROX_ON:				
			enable_light_and_proximity(Light_turn_on);
		    ret=enable_light_and_proximity(Prox_turn_on);
			if(ret>=0)
			{
				prox_on = 1;	
            	pr_crit(LTR_TAG "TAOS_IOCTL_PROX_ON\n");
			}	
			return ret;	
			break;
        case TAOS_IOCTL_PROX_OFF:
		    if(0==light_on){
			    enable_light_and_proximity(Light_turn_off);
			}
			ret=enable_light_and_proximity(Prox_turn_off);
			if(ret>=0)
			{
				prox_on = 0;	
        		pr_crit(LTR_TAG"TAOS_IOCTL_PROX_OFF\n");
			}	
			return ret;	
			break;
		case TAOS_IOCTL_PROX_DATA:
            ret = copy_to_user((struct taos_prox_info *)arg, ltr_prox_cur_infop, sizeof(struct taos_prox_info));
            if (ret) {
                printk(KERN_ERR "LTR: copy_to_user failed in ioctl ltr_prox_data\n");
                return -ENODATA;
            }
            return (ret);
            break;
        case TAOS_IOCTL_PROX_EVENT:
 			return (ltr_prox_cur_infop->prox_event);
            break;
		case TAOS_IOCTL_PROX_CALIBRATE:
			if (!prox_on)			
			{
				printk(KERN_ERR "LTR: ioctl prox_calibrate was called before ioctl prox_on was called\n");
				return -EPERM;
			}
			
			mutex_lock(&ltr_datap->proximity_calibrating);
	//write bytes to address control registers
			count=0;
//disable init and als
	 	if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_CONTR], (LTRCHIP_ALS_Mode_Standby+LTRCHIP_ALS_Gain_Range1)))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_ALS_CONTR],(LTRCHIP_ALS_Mode_Standby+LTRCHIP_ALS_Gain_Range1));                	
            goto Prox_Calib_error;
			}  			
	 	if ((ret = (i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_INT_CONFIG], (LTRCHIP_INT_Output_mode_update+LTRCHIP_INT_Pin_polarity_0+LTRCHIP_INT_Mode_ALS_trig)))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_INT_CONFIG],(LTRCHIP_INT_Output_mode_update+LTRCHIP_INT_Pin_polarity_0+LTRCHIP_INT_Mode_ALS_trig));                	
            goto Prox_Calib_error;
			}           
			msleep(100);
			if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_TH_UP_Low], 0xFFFF))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_PS_TH_UP_Low],0xFFFF);                	
            goto Prox_Calib_error;
			}           
	 	if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_TH_LO_Low], 0))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_PS_TH_LO_Low],0);                	
            goto Prox_Calib_error;
			} 			

		if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_TH_UP_Low], 0x07FF))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_PS_TH_UP_Low],0x07FF);                	
            goto Prox_Calib_error;
			}           
	 	if ((ret = (i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_TH_LO_Low], 0))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_PS_TH_LO_Low],0);                	
            goto Prox_Calib_error;
			} 			
			msleep(100);
			
			do{			
				prox_sum = 0;
				prox_max = 0;
				prox_min = 0x07FF;
				prox_mean = 0;
				count++;
				for (i = 0; i < 5; i++) {
	                if ((ret = ltr_prox_poll(&ltr_prox_cal_info[i])) < 0) {
        	            printk(KERN_ERR "LTR: call to prox_poll failed in ioctl prox_calibrate\n");
					    goto Prox_Calib_error;
                    }					
					prox_sum += ltr_prox_cal_info[i].prox_data;
					if (ltr_prox_cal_info[i].prox_data > prox_max){
						prox_max = ltr_prox_cal_info[i].prox_data;
						}
					if (ltr_prox_cal_info[i].prox_data < prox_min){
						prox_min = ltr_prox_cal_info[i].prox_data;
						}
					msleep(150);
				}

				prox_mean = prox_sum/5;
				if(0 != prox_mean){
				    if((100*(prox_max - prox_min))/prox_mean < 5){
				        count = 0xFF;
				    }
				}
			}while(count<10); 
			 if(count != 0xFF){
				    printk(KERN_ERR "LTR: dlt too big :prox_max = %d prox_min = %d dlt = %d\n",prox_max,prox_min,((100*(prox_max - prox_min))/prox_mean < 5));
                	goto Prox_Calib_error;			     
			 }
			    if(prox_mean < 100){
				    ltr_cfgp->prox_threshold_hi = 500;
				}else{
				    ratio = 10*prox_mean/ltr_sat_prox;				
				    for (prox_pt = ltr_prox_tablep; prox_pt->ratio && prox_pt->ratio <= ratio; prox_pt++);
        		    if(!prox_pt->ratio){
				        printk(KERN_ERR "LTR: ltr_prox_tablep get error ratio ratio = %d prox_mean = %d ltr_sat_prox = %d\n",ratio,prox_mean,ltr_sat_prox);
                	    goto Prox_Calib_error;
					} 
				    ltr_cfgp->prox_threshold_hi = (prox_mean*prox_pt->hi)/10;
                }
				
				if(ltr_cfgp->prox_threshold_hi>=ltr_sat_prox){
				    ltr_cfgp->prox_threshold_hi = ltr_sat_prox;
				    printk(KERN_ERR "ltr:ltr_prox_threshold_hi=%d is set as ltr_sat_prox %d\n",ltr_cfgp->prox_threshold_hi,ltr_sat_prox);
					}	
				//if(ltr_cfgp->prox_threshold_hi<=(ltr_sat_prox*2/5)){
				//	}
				ltr_cfgp->prox_threshold_lo = ltr_cfgp->prox_threshold_hi*THRES_LO_TO_HI_RATIO;	
				printk(KERN_ERR "ltr:ltr_prox_threshold_hi=%d  ltr_prox_threshold_hi=%d\n",ltr_cfgp->prox_threshold_hi,ltr_cfgp->prox_threshold_lo);
				msleep(50);	
			//}while(count<10);

			
//turn als and irq on
Prox_Calib_error:
		if (((i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_CONTR], (LTRCHIP_PS_Mode_Standby+LTRCHIP_PS_Gain_4)))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_PS_CONTR],(LTRCHIP_PS_Mode_Standby+LTRCHIP_PS_Gain_4));                	
                ret = -EPERM;
                goto Prox_Calib_exit;
			}           
		msleep(50);
		if (((i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_TH_UP_Low], 0x0))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_PS_TH_UP_Low],0x0);                	
                ret = -EPERM;
                goto Prox_Calib_exit;
			}           
	 	if (((i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_TH_LO_Low], 1))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_PS_TH_LO_Low],1);                	
                ret = -EPERM;
                goto Prox_Calib_exit;
			}           
			
		if (((i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_TH_UP_Low], 0x0))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_ALS_TH_UP_Low],0x0);                	
                ret = -EPERM;
                goto Prox_Calib_exit;
			}           
	 	if (((i2c_smbus_write_word_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_TH_LO_Low], 1))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_word(%d) val %d\n",ltr_regmap[LTRCHIP_ALS_TH_LO_Low],1);                	
                ret = -EPERM;
                goto Prox_Calib_exit;
			}           

	 	if (((i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_INT_CONFIG], (LTRCHIP_INT_Output_mode_update+LTRCHIP_INT_Pin_polarity_0+LTRCHIP_INT_Mode_Both_trig)))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_INT_CONFIG],(LTRCHIP_INT_Output_mode_update+LTRCHIP_INT_Pin_polarity_0+LTRCHIP_INT_Mode_Both_trig));                	
                ret = -EPERM;
                goto Prox_Calib_exit;
			}           

		if (((i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_CONTR], (LTRCHIP_PS_Mode_Active+LTRCHIP_PS_Gain_4)))) < 0) {        	        
			pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_PS_CONTR],(LTRCHIP_PS_Mode_Active+LTRCHIP_PS_Gain_4));                	
                ret = -EPERM;
                goto Prox_Calib_exit;
			}           

		if(1 ==light_on){
	 	    if (((i2c_smbus_write_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_CONTR], (LTRCHIP_ALS_Mode_Active+LTRCHIP_ALS_Gain_Range1)))) < 0) {        	        
			    pr_crit(LTR_TAG "error: i2c_smbus_write_byte(%d) val %d\n",ltr_regmap[LTRCHIP_ALS_CONTR],(LTRCHIP_ALS_Mode_Active+LTRCHIP_ALS_Gain_Range1));                	
                ret = -EPERM;
                goto Prox_Calib_exit;
			    }
        }				

Prox_Calib_exit:	
            tmp = i2c_smbus_read_byte_data(ltr_datap->client,ltr_regmap[LTRCHIP_ALS_PS_STATUS]);//add error ctl
            printk(LTR_TAG "LTR_interrupt status = %x\n",tmp);	
		
			mutex_unlock(&ltr_datap->proximity_calibrating);
			pr_crit(KERN_ERR "ltr prox_cal_threshold_hi=%d,prox_cal_threshold_lo=%d\n",ltr_cfgp->prox_threshold_hi,ltr_cfgp->prox_threshold_lo);
			return (ret);
			break;
		case TAOS_IOCTL_PROX_GET_ENABLED:
			return put_user(prox_on, (unsigned long __user *)arg);
			break;	
		case TAOS_IOCTL_ALS_GET_ENABLED:
			return put_user(light_on, (unsigned long __user *)arg);
			break;
			
		default:
			return -EINVAL;
			break;
	}
	return (ret);
}

// read and calculate lux value
static int ltr_get_lux(void) {
    u16 raw_clear = 0, raw_ir = 0;// raw_lux = 0;
	u32 lux = 0;
	u32 ratio = 0;
//	u8 dev_gain = 0;
	struct ltr_lux_data *p;
	int ret = 0;
	u8 chdata[4];
	int i = 0;
    	//pr_crit(LTR_TAG "ltr start to calc lux value\n");

	for (i = 0; i < 4; i++) {
		    if ((ret = (i2c_smbus_read_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_ALS_DATA_CH1_Low + i]))) < 0) {
                printk(KERN_ERR "LTR: i2c_smbus_write_byte() to als/prox data reg failed in ltr_prox_poll()\n");
                return (ret);
            }
            chdata[i] = ret;
    		printk(KERN_ERR "LTR_wlg: i2c_smbus_read_byte() i=%d ret=%d \n", i, chdata[i]);
		}
//ch0		
	raw_clear = chdata[3];
	raw_clear <<= 8;
	raw_clear |= chdata[2];
//ch1
	raw_ir    = chdata[1];
	raw_ir    <<= 8;
	raw_ir    |= chdata[0];
//[sensor wlg 20110729]ALS thereshold modify	
    if( raw_clear < ((LTR_MAX_LUX*4)/5))
    {
    	ltr_als_intr_threshold_hi_param = raw_clear + (raw_clear>>2);
    	ltr_als_intr_threshold_lo_param = raw_clear - (raw_clear>>2);
    }
    else
    {
    	ltr_als_intr_threshold_hi_param = LTR_MAX_LUX;
    	ltr_als_intr_threshold_lo_param = LTR_MAX_LUX - (LTR_MAX_LUX>>2);
    }
    if( ltr_als_intr_threshold_hi_param < 25)
    {
    	ltr_als_intr_threshold_hi_param = 25;
    	ltr_als_intr_threshold_lo_param = 0;
		//printk(KERN_ERR "LTR: lux=0 ch0=%d ch1=%d\n", lux, raw_clear, raw_ir);
	    return(lux);
    }


	ratio = (raw_ir<<8)/(raw_clear + raw_ir);
	for (p = ltr_lux_tablep; p->ratio && p->ratio < ratio; p++){
	}
	printk(KERN_ERR "LTR: p->ratio=%d\n", p->ratio);
    if(p->ratio <= 0){
        return -1;
	}
    if(p->clear == 0){
        return -1;
	}
	
	lux = ((raw_clear * p->clear) - (raw_ir * p->ir))/10000;
    printk(KERN_ERR "LTR: lux=%d ch0=%d ch1=%d\n", lux, raw_clear, raw_ir);
//[sensor wlg 20110729]ALS thereshold modify
//    if(lux > (LTR_MAX_LUX)/(ltr_cfgp->gain_trim))
//        lux = (LTR_MAX_LUX)/(ltr_cfgp->gain_trim);
    if(lux < 25)
        lux = 0;

	return(lux);
}

static int ltr_lux_filter(int lux)
{
        static u8 middle[] = {1,0,2,0,0,2,0,1};
        int index;

        ltr_lux_history[2] = ltr_lux_history[1];
        ltr_lux_history[1] = ltr_lux_history[0];
        ltr_lux_history[0] = lux;
        if((ltr_lux_history[2] < 0) || (ltr_lux_history[1] < 0) || (ltr_lux_history[0] < 0))
		return -ENODATA;
        index = 0;
        if( ltr_lux_history[0] > ltr_lux_history[1] ) index += 4;
        if( ltr_lux_history[1] > ltr_lux_history[2] ) index += 2;
        if( ltr_lux_history[0] > ltr_lux_history[2] ) index++;
        return(ltr_lux_history[middle[index]]);
}

// verify device
static int ltr_device_name(unsigned char *bufp, char **device_name) {
        if( (bufp[LTRCHIP_PART_ID]&LTRCHIP_PART_MASK)!=LTRCHIP_PART)
		    return(0);
        if( (bufp[LTRCHIP_MANUFAC_ID]&LTRCHIP_MANUFAC_MASK)!=LTRCHIP_MANUFAC)
		    return(0);
		*device_name="ltr";
		return(1);
}

// proximity poll
static int ltr_prox_poll(struct taos_prox_info *prxp) {
	//static int event = 0;
        //u16 status = 0;
        int i = 0, ret = 0;//wait_count = 0
        u8 chdata[2];

        for (i = 0; i < 2; i++) {
		    if ((ret = (i2c_smbus_read_byte_data(ltr_datap->client, ltr_regmap[LTRCHIP_PS_DATA_Low + i]))) < 0) {
                printk(KERN_ERR "LTR: i2c_smbus_write_byte() to als/prox data reg failed in ltr_prox_poll()\n");
                return (ret);
            }
            chdata[i] = ret;
        }
        prxp->prox_data = chdata[1] & 7;
        prxp->prox_data <<= 8;
        prxp->prox_data |= chdata[0];
//		printk(KERN_ERR "LTR: prxp->prox_clear =%d\n",prxp->prox_data);
//        if (prxp->prox_clear > ((ltr_sat_als*80)/100))
//                return -ENODATA;

        return (ret);
}

/* prox poll timer function
static void ltr_prox_poll_timer_func(unsigned long param) {
	int ret = 0;

	if (!ltr_device_released) {
		if ((ret = ltr_prox_poll(ltr_prox_cur_infop)) < 0) {
			printk(KERN_ERR "LTR: call to prox_poll failed in ltr_prox_poll_timer_func()\n");
			return;
		}
		ltr_prox_poll_timer_start();
	}
	return;
}

// start prox poll timer
static void ltr_prox_poll_timer_start(void) {
        init_timer(&prox_poll_timer);
        prox_poll_timer.expires = jiffies + (HZ/10);
        prox_poll_timer.function = ltr_prox_poll_timer_func;
        add_timer(&prox_poll_timer);

        return;
}*/


MODULE_AUTHOR("John Koshi - Surya Software");
MODULE_DESCRIPTION("LTR ambient light and proximity sensor driver");
MODULE_LICENSE("GPL");

module_init(ltr_init);
module_exit(ltr_exit);

