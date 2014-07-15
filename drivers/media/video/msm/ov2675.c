
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/leds.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include "ov2675.h"
#include <linux/slab.h>


#define FALSE 0
#define TRUE 1

static int ov2675_pwdn_gpio;
static int ov2675_reset_gpio;

static unsigned int ov2675_preview_exposure;
static unsigned int ov2675_gain;
static unsigned short ov2675_preview_maxlines;
static int m_60Hz = FALSE;

extern int32_t msm_camera_power_backend(enum msm_camera_pwr_mode_t pwr_mode);


static int ov2675_CSI_CONFIG = 0;

struct ov2675_work {
	struct work_struct work;
};
static struct ov2675_work *ov2675_sensorw;
static struct i2c_client    *ov2675_client;
static DECLARE_WAIT_QUEUE_HEAD(ov2675_wait_queue);
DEFINE_MUTEX(ov2675_mutex);
static u8 ov2675_i2c_buf[4];
static u8 ov2675_counter = 0;
//static int16_t ov2675_effect = CAMERA_EFFECT_OFF;
//static int is_autoflash = 0;
static int effect_value = 0;
static unsigned int SAT_U = 0x40;
static unsigned int SAT_V = 0x40;

struct __ov2675_ctrl 
{
	const struct msm_camera_sensor_info *sensordata;
	int sensormode;
	uint fps_divider; /* init to 1 * 0x00000400 */
	uint pict_fps_divider; /* init to 1 * 0x00000400 */
	u16 curr_step_pos;
	u16 curr_lens_pos;
	u16 init_curr_lens_pos;
	u16 my_reg_gain;
	u16 my_reg_line_count;
	enum msm_s_resolution prev_res;
	enum msm_s_resolution pict_res;
	enum msm_s_resolution curr_res;
	enum msm_s_test_mode  set_test;
};
static struct __ov2675_ctrl *ov2675_ctrl;
//static int afinit = 1;

extern struct rw_semaphore leds_list_lock;
extern struct list_head leds_list;

static int ov2675_i2c_remove(struct i2c_client *client);
static int ov2675_i2c_probe(struct i2c_client *client,const struct i2c_device_id *id);

static int ov2675_i2c_txdata(u16 saddr,u8 *txdata,int length)
{
	struct i2c_msg msg[] = {
		{
			.addr  = saddr,
				.flags = 0,
				.len = length,
				.buf = txdata,
		},
	};

	if (i2c_transfer(ov2675_client->adapter, msg, 1) < 0)    return -EIO;
	else return 0;
}

static int ov2675_i2c_write(unsigned short saddr, unsigned int waddr,
							unsigned short bdata,u8 trytimes)
{
	int rc = -EIO;
	ov2675_counter = 0;
	ov2675_i2c_buf[0] = (waddr & 0xFF00)>>8;
	ov2675_i2c_buf[1] = (waddr & 0x00FF);
	ov2675_i2c_buf[2] = (bdata & 0x00FF);

	while ((ov2675_counter < trytimes) &&(rc != 0))
	{
		rc = ov2675_i2c_txdata(saddr, ov2675_i2c_buf, 3);

		if (rc < 0)
		{
			ov2675_counter++;
			pr_err("***--CAMERA-- i2c_write_w failed,i2c addr=0x%x, command addr = 0x%x, val = 0x%x, s=%d, rc=%d!\n",saddr,waddr, bdata,ov2675_counter,rc);
			msleep(4);
		}
	}
	return rc;
}

static int ov2675_i2c_rxdata(unsigned short saddr,
							 unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
		{
			.addr   = saddr,
				.flags = 0,
				.len   = 2,
				.buf   = rxdata,
		},
		{
			.addr   = saddr,
				.flags = I2C_M_RD,
				.len   = length,
				.buf   = rxdata,
			},
	};

	if (i2c_transfer(ov2675_client->adapter, msgs, 2) < 0) {
		pr_err("ov2675_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}

static int32_t ov2675_i2c_read_byte(unsigned short   saddr,
									unsigned int raddr, unsigned int *rdata)
{
	int rc = 0;
	unsigned char buf[2];
	//pr_err("+ov2675_i2c_read_byte\n");
	memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00)>>8;
	buf[1] = (raddr & 0x00FF);

	rc = ov2675_i2c_rxdata(saddr, buf, 1);
	if (rc < 0) {
		pr_err("ov2675_i2c_read_byte failed!\n");
		return rc;
	}

	*rdata = buf[0];

	//pr_err("-ov2675_i2c_read_byte\n");
	return rc;
}
/*
static int32_t ov2675_i2c_read(unsigned short   saddr,
							   unsigned int raddr, unsigned int *rdata)
{
	int rc = 0;
	unsigned char buf[2];
	//pr_err("+ov2675_i2c_read\n");
	memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00)>>8;
	buf[1] = (raddr & 0x00FF);

	rc = ov2675_i2c_rxdata(saddr, buf, 2);
	if (rc < 0)	return rc;
	*rdata = buf[0] << 8 | buf[1];

	if (rc < 0)
		pr_err("ov2675_i2c_read failed!\n");
	//pr_err("-ov2675_i2c_read\n");
	return rc;
}
*/

static int32_t OV2675_WritePRegs(POV2675_WREG pTb,int32_t len)
{
	int32_t i, ret = 0;
	uint32_t regv;
	for (i = 0;i < len; i++)
	{
		if (0 == pTb[i].mask) {  
			ov2675_i2c_write(ov2675_client->addr,pTb[i].addr,pTb[i].data,10);
		} else {   
			ov2675_i2c_read_byte(ov2675_client->addr,pTb[i].addr, &regv);
			regv &= pTb[i].mask;
			regv |= (pTb[i].data & (~pTb[i].mask));
			ov2675_i2c_write(ov2675_client->addr, pTb[i].addr, regv, 10);
		}
	}
	return ret;
}

static void camera_sw_power_onoff(int v)
{
	if (v == 0) {
		pr_err("camera_sw_power_onoff: down\n");
		ov2675_i2c_write(ov2675_client->addr, 0x3008, 0x42, 10); //power down
	}
	else {
		pr_err("camera_sw_power_onoff: on\n");
		ov2675_i2c_write(ov2675_client->addr, 0x3008, 0x02, 10); //power on
	}
}

static void ov2675_power_off(void)
{
	pr_err("--CAMERA-- %s ... (Start...)\n",__func__);

	gpio_set_value(ov2675_pwdn_gpio, 1);
	//gpio_set_value(ov2675_reset_gpio, 0);

	//afinit = 0;

	pr_err("--CAMERA-- %s ... (End...)\n",__func__);
}

static void ov2675_power_on(void)
{
	pr_err("--CAMERA-- %s ... (Start...)\n",__func__);

	gpio_set_value(ov2675_pwdn_gpio, 0);

	//afinit = 1;

	pr_err("--CAMERA-- %s ... (End...)\n",__func__);
}

static void ov2675_power_reset(void)
{
	pr_err("--CAMERA-- %s ... (Start...)\n",__func__);
	//gpio_set_value(ov2675_reset_gpio, 1);   //reset camera reset pin
	mdelay(5);
	//gpio_set_value(ov2675_reset_gpio, 0);
	mdelay(5);
	gpio_set_value(ov2675_reset_gpio, 1);
	mdelay(1);

	pr_err("--CAMERA-- %s ... (End...)\n",__func__);
}

static int ov2675_probe_readID(const struct msm_camera_sensor_info *data)
{
	int rc = 0;    
	u32 device_id_high = 0;
	u32 device_id_low = 0;

	pr_err("--CAMERA-- %s (Start...)\n",__func__);
	pr_err("--CAMERA-- %s sensor poweron,begin to read ID!\n",__func__);

	//0x300A ,sensor ID register
	rc = ov2675_i2c_read_byte(ov2675_client->addr, 0x300A, &device_id_high);

	if (rc < 0)
	{
		pr_err("--CAMERA-- %s ok , readI2C failed, rc = 0x%x\r\n", __func__, rc);
		return rc;   
	}  
	pr_err("--CAMERA-- %s  readID high byte, data = 0x%x\r\n", __func__, device_id_high);

	//0x300B ,sensor ID register
	rc = ov2675_i2c_read_byte(ov2675_client->addr, 0x300B, &device_id_low);
	if (rc < 0)
	{
		pr_err("--CAMERA-- %s ok , readI2C failed,rc = 0x%x\r\n", __func__, rc);
		return rc;   
	}   

	pr_err("--CAMERA-- %s  readID low byte, data = 0x%x\r\n", __func__, device_id_low);
	pr_err("--CAMERA-- %s return ID :0x%x\n", __func__, (device_id_high<<8)+device_id_low);

	//0x5640, ov2675 chip id
	if((device_id_high<<8)+device_id_low != OV2675_SENSOR_ID)
	{
		pr_err("--CAMERA-- %s ok , device id error, should be 0x%x\r\n",
			__func__, OV2675_SENSOR_ID);
		return -EINVAL;
	}
	else
	{
		pr_err("--CAMERA-- %s ok , device id=0x%x\n",__func__,OV2675_SENSOR_ID);
		return 0;
	}
}



static int ov2675_video_config(void)
{
	int rc = 0;
	pr_err("--CAMERA-- ov2675_video_config\n");

	//pr_err("--CAMERA-- preview in, is_autoflash - 0x%x\n", is_autoflash);

	/* preview setting */
	rc = OV2675Core_WritePREG(ov2675_preview_tbl);   
	return rc;
}



static int ov2675_get_preview_exposure_gain(void)
{
     int rc = 0;
    unsigned int ret_l,ret_m,ret_h, temp_reg;
    unsigned int temp_AE_reg = 0;

    
 ov2675_i2c_read_byte(ov2675_client->addr,0x3400, &temp_AE_reg);
pr_err("--0x3400 = 0x%x\n", temp_AE_reg);
    //turn off AEC/AGC     
    ov2675_i2c_read_byte(ov2675_client->addr,0x3013, &temp_AE_reg);
    ov2675_i2c_write(ov2675_client->addr, 0x3013, (temp_AE_reg&(~0x05)), 10);  
            
    //get preview exp & gain
    ret_h = ret_m = ret_l = 0;
    ov2675_preview_exposure = 0;
    //shutter
    ov2675_i2c_read_byte(ov2675_client->addr,0x3002, &ret_h);
    ov2675_i2c_read_byte(ov2675_client->addr,0x3003, &ret_l);
    ov2675_preview_exposure = (ret_h << 8) | (ret_l & 0xff) ;
//    printk("preview_exposure=%d\n", ov2675_preview_exposure);
#if 0
    //dummy line
    ov2675_i2c_read_byte(ov2675_client->addr,0x302d, &ret_h);
    ov2675_i2c_read_byte(ov2675_client->addr,0x302e, &ret_l);
    ov2675_preview_exposure = ov2675_preview_exposure + (ret_h << 8) | (ret_l & 0xff) ;
//    printk("preview_exposure=%d\n", ov2675_preview_exposure);
#endif

    //vts
    ret_h = ret_m = ret_l = 0;
    ov2675_preview_maxlines = 0;
    ov2675_i2c_read_byte(ov2675_client->addr,0x302a, &ret_h);
    ov2675_i2c_read_byte(ov2675_client->addr,0x302b, &ret_l);
    ov2675_preview_maxlines = (ret_h << 8) | (ret_l & 0xff);
//    printk("Preview_Maxlines=%d\n", ov2675_preview_maxlines);

    //Read back AGC Gain for preview
    ov2675_gain = 0;
    temp_reg = 0;
    ov2675_i2c_read_byte(ov2675_client->addr,0x3000, &temp_reg);
 
    ov2675_gain=(16+(temp_reg&0x0F));
    
    if(temp_reg&0x10)
        ov2675_gain<<=1;
    if(temp_reg&0x20)
        ov2675_gain<<=1;
      
    if(temp_reg&0x40)
        ov2675_gain<<=1;
      
    if(temp_reg&0x80)
        ov2675_gain<<=1;
            
//    printk("Gain,0x350b=0x%x\n", ov2675_gain);

    return rc;
}

static int ov2675_set_capture_exposure_gain(void)
{
    int rc = 0;
   //calculate capture exp & gain
   
    unsigned char ExposureLow,ExposureHigh;
    unsigned int ret_l,ret_m,ret_h,Lines_10ms, temp_reg;
    unsigned short ulCapture_Exposure,iCapture_Gain;
    unsigned int ulCapture_Exposure_Gain,Capture_MaxLines;
    //unsigned int ulCapture_Exposure_Gain;

    ret_h = ret_m = ret_l = 0;
    ov2675_i2c_read_byte(ov2675_client->addr,0x302a, &ret_h);
    ov2675_i2c_read_byte(ov2675_client->addr,0x302b, &ret_l);
    Capture_MaxLines = (ret_h << 8) | (ret_l & 0xff);
    printk("Capture_MaxLines=%d\n", Capture_MaxLines);

    if(m_60Hz == TRUE)
    {
    Lines_10ms = Capture_Framerate * Capture_MaxLines/12000;
    }
    else
    {
    Lines_10ms = Capture_Framerate * Capture_MaxLines/10000;
    }
pr_err("Lines_10ms=%d\n", Lines_10ms);
    if(ov2675_preview_maxlines == 0)
    {
    ov2675_preview_maxlines = 1;
    }

#if 0
    ulCapture_Exposure = (ov2675_preview_exposure*(Capture_Framerate)*(Capture_MaxLines))/(((ov2675_preview_maxlines)*(g_Preview_FrameRate)));
	pr_err("ulCapture_Exposure=%d 1=%d,2=%d,3=%d,4=%d,5=%d,\n", ulCapture_Exposure,ov2675_preview_exposure,Capture_Framerate,ov2675_preview_maxlines,g_Preview_FrameRate,ov2675_gain);
    iCapture_Gain = ov2675_gain;
#endif
   // ulCapture_Exposure = (ov2675_preview_exposure*(Capture_Framerate)*(Capture_MaxLines))/(((ov2675_preview_maxlines)*(g_Preview_FrameRate)));
   // iCapture_Gain = (ov2675_gain*(Capture_Framerate)*(Capture_MaxLines))/(((ov2675_preview_maxlines)*(g_Preview_FrameRate)));
    iCapture_Gain = ov2675_gain*(2400)/(3600);
	pr_err("ulCapture_Exposure=%d 1=%d,2=%d,3=%d,4=%d,5=%d,\n", iCapture_Gain,ov2675_gain,Capture_Framerate,ov2675_preview_maxlines,g_Preview_FrameRate,ov2675_gain);
   // iCapture_Gain = ov2675_gain;
	//pr_err("ulCapture_Exposure=%d 1=%d,2=%d,3=%d,4=%d,5=%d,\n", ulCapture_Exposure,ov2675_preview_exposure,Capture_Framerate,ov2675_preview_maxlines,g_Preview_FrameRate,ov2675_gain);
    ulCapture_Exposure = ov2675_preview_exposure;

    ulCapture_Exposure_Gain = ulCapture_Exposure * iCapture_Gain; 

    if(ulCapture_Exposure_Gain < Capture_MaxLines*16)
    {
    ulCapture_Exposure = ulCapture_Exposure_Gain/16;

    if (ulCapture_Exposure > Lines_10ms)
    {
      ulCapture_Exposure /= Lines_10ms;
      ulCapture_Exposure *= Lines_10ms;
    }
    }
    else
    {
    ulCapture_Exposure = Capture_MaxLines;
    }

    if(ulCapture_Exposure == 0)
    {
    ulCapture_Exposure = 1;
    }

    iCapture_Gain = (ulCapture_Exposure_Gain/ulCapture_Exposure + 1)/2;

    ExposureLow = (unsigned char)(ulCapture_Exposure & 0xff);
    ExposureHigh = (unsigned char)((ulCapture_Exposure & 0xff00) >> 8);
  
    // set capture exp & gain
    temp_reg = 0;
    if (iCapture_Gain > 31)
    {
        temp_reg |= 0x10;
        iCapture_Gain = iCapture_Gain >> 1;
    }
    if (iCapture_Gain > 31)
    {
        temp_reg |= 0x20;
        iCapture_Gain = iCapture_Gain >> 1;
    }

    if (iCapture_Gain > 31)
    {
        temp_reg |= 0x40;
        iCapture_Gain = iCapture_Gain >> 1;
    }
    if (iCapture_Gain > 31)
    {
        temp_reg |= 0x80;
        iCapture_Gain = iCapture_Gain >> 1;
    }
    
    if (iCapture_Gain > 16)
    {
        temp_reg |= ((iCapture_Gain -16) & 0x0f);
    }   

	#if 1
    ov2675_i2c_write(ov2675_client->addr,0x3000, iCapture_Gain + 1, 10);
    msleep(140);//delay 1 frame    
    ov2675_i2c_write(ov2675_client->addr,0x3000, iCapture_Gain, 10);
	#endif

    ov2675_i2c_write(ov2675_client->addr,0x3003, ExposureLow, 10);
    ov2675_i2c_write(ov2675_client->addr,0x3002, ExposureHigh, 10);
  
//    printk("iCapture_Gain=%d\n", iCapture_Gain);
//    printk("ExposureLow=%d\n", ExposureLow);
//    printk("ExposureHigh=%d\n", ExposureHigh);

    msleep(270);

    return rc;
}

static int ov2675_snapshot_config(void)
{
	int rc = 0;
	int temp_1, temp_2, temp_3;
//	unsigned int tmp;
//	int capture_len = sizeof(ov2675_capture_tbl) / sizeof(ov2675_capture_tbl[0]);

	pr_err("--CAMERA-- SENSOR_SNAPSHOT_MODE\n");
	ov2675_get_preview_exposure_gain();//Step1: get preview exposure and gain
	
	ov2675_i2c_write(ov2675_client->addr, 0x330c, 0x3e, 10);	
   	ov2675_i2c_read_byte(ov2675_client->addr,0x330f, &temp_1);
   	ov2675_i2c_read_byte(ov2675_client->addr,0x3301, &temp_2);
	ov2675_i2c_write(ov2675_client->addr, 0x3301, ((temp_2)&0xbf), 10);
	temp_1 = temp_1 & 0x1f;
	temp_1 = temp_1 * 2;
	ov2675_i2c_read_byte(ov2675_client->addr,0x3391, &temp_3);
	ov2675_i2c_write(ov2675_client->addr, 0x3391, ((temp_3)|0x02), 10);	
	ov2675_i2c_write(ov2675_client->addr, 0x3394, temp_1, 10);	
	ov2675_i2c_write(ov2675_client->addr, 0x3395, temp_1, 10);	
	
	rc = OV2675Core_WritePREG(ov2675_capture_tbl);  //Step2: change to full size 
	ov2675_set_capture_exposure_gain();//Step3: calculate and set capture exposure and gain

	return rc;
}

static int ov2675_setting(enum msm_s_reg_update rupdate,enum msm_s_setting rt)
{
	int rc = -EINVAL;
	int flip=0,mirror=0;
//	unsigned int tmp;
	struct msm_camera_csi_params ov2675_csi_params;

	pr_err("--CAMERA-- %s (Start...), rupdate=%d \n",__func__,rupdate);

	switch (rupdate)
	{
	case S_UPDATE_PERIODIC:
		camera_sw_power_onoff(0); //standby

		msleep(20);

		ov2675_csi_params.lane_cnt = 1;
		ov2675_csi_params.data_format = CSI_8BIT;
		ov2675_csi_params.lane_assign = 0xe4;
		ov2675_csi_params.dpcm_scheme = 0;
		ov2675_csi_params.settle_cnt = 0x6;

		pr_err("%s: msm_camio_csi_config\n", __func__);

		rc = msm_camio_csi_config(&ov2675_csi_params);				
		ov2675_CSI_CONFIG = 1;

		msleep(20);

		if (S_RES_PREVIEW == rt) {
			rc = ov2675_video_config();
		} else if (S_RES_CAPTURE == rt) {
			rc = ov2675_snapshot_config();
		}
		camera_sw_power_onoff(1); //on

		msleep(20);

		break; /* UPDATE_PERIODIC */

	case S_REG_INIT:
		pr_err("--CAMERA-- S_REG_INIT (Start)\n");

		rc = ov2675_i2c_write(ov2675_client->addr, 0x3012, 0x80, 10);
		msleep(5);

		//set sensor init setting
		pr_err("set sensor init setting\n");
		rc = OV2675Core_WritePREG(ov2675_init_tbl);
		if (rc < 0) {
			pr_err("sensor init setting failed\n");
			break;
		}

		//set image quality setting
		pr_err(" %s: set image quality setting\n", __func__);
		rc = OV2675Core_WritePREG(ov2675_init_iq_tbl);
              rc = ov2675_i2c_read_byte(ov2675_client->addr, 0x307c, &flip);
	  	rc = ov2675_i2c_read_byte(ov2675_client->addr, 0x3090, &mirror);
		pr_err("TTT  0x307c= %x, 0x3090=%x \n", flip,mirror);
		/* reset fps_divider */
		ov2675_ctrl->fps_divider = 1 * 0x0400;
		pr_err("--CAMERA-- S_REG_INIT (End)\n");
		break; /* case REG_INIT: */

	default:
		break;
	} /* switch (rupdate) */

	pr_err("--CAMERA-- %s (End), rupdate=%d \n",__func__,rupdate);

	return rc;
}

static int ov2675_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int rc = -ENOMEM;
	pr_err("--CAMERA-- %s\n",__func__);

	ov2675_ctrl = kzalloc(sizeof(struct __ov2675_ctrl), GFP_KERNEL);
	if (!ov2675_ctrl)
	{
		pr_err("--CAMERA-- kzalloc ov2675_ctrl error !!\n");
		kfree(ov2675_ctrl);
		return rc;
	}
	ov2675_ctrl->fps_divider = 1 * 0x00000400;
	ov2675_ctrl->pict_fps_divider = 1 * 0x00000400;
	ov2675_ctrl->set_test = S_TEST_OFF;
	ov2675_ctrl->prev_res = S_QTR_SIZE;
	ov2675_ctrl->pict_res = S_FULL_SIZE;

	if (data)
		ov2675_ctrl->sensordata = data;

	ov2675_power_off();

	pr_err("%s: msm_camio_clk_rate_set\n", __func__);

	msm_camio_clk_rate_set(24000000);
	mdelay(5);

	ov2675_power_on();
	ov2675_power_reset();

	pr_err("%s: init sequence\n", __func__);

	if (ov2675_ctrl->prev_res == S_QTR_SIZE)
		rc = ov2675_setting(S_REG_INIT, S_RES_PREVIEW);
	else
		rc = ov2675_setting(S_REG_INIT, S_RES_CAPTURE);

	if (rc < 0)
	{
		pr_err("--CAMERA-- %s : ov2675_setting failed. rc = %d\n",__func__,rc);
		kfree(ov2675_ctrl);
		return rc;
	}

	pr_err("--CAMERA--re_init_sensor ok!!\n");
	return rc;
}

static int ov2675_sensor_release(void)
{
	pr_err("--CAMERA--ov2675_sensor_release!!\n");

	mutex_lock(&ov2675_mutex);

	ov2675_power_off();

	kfree(ov2675_ctrl);
	ov2675_ctrl = NULL;

	mutex_unlock(&ov2675_mutex);
	return 0;
}

static const struct i2c_device_id ov2675_i2c_id[] = {
	{"ov2675", 0},{}
};

static int ov2675_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static int ov2675_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&ov2675_wait_queue);
	return 0;
}

static long ov2675_set_effect(int mode, int effect)
{
	int rc = 0;
	pr_err("--CAMERA-- %s ...(Start)\n",__func__);

	switch (mode)
	{
	case SENSOR_PREVIEW_MODE:
		//Context A Special Effects /
		pr_err("--CAMERA-- %s ...SENSOR_PREVIEW_MODE\n",__func__);
		break;

	case SENSOR_SNAPSHOT_MODE:
		// Context B Special Effects /
		pr_err("--CAMERA-- %s ...SENSOR_SNAPSHOT_MODE\n",__func__);
		break;

	default:
		break;
	}

	effect_value = effect;    //By HT

	switch (effect) 
	{
	case CAMERA_EFFECT_OFF: 
		
			pr_err("--CAMERA-- %s ...CAMERA_EFFECT_OFF\n",__func__);
			rc = OV2675Core_WritePREG(ov2675_effect_normal_tbl);
		       break;
		
	case CAMERA_EFFECT_MONO: 
		
			pr_err("--CAMERA-- %s ...CAMERA_EFFECT_BW\n",__func__);
			rc = OV2675Core_WritePREG(ov2675_effect_bw_tbl);
			break;
		
	
	case CAMERA_EFFECT_SEPIA: 
		
			pr_err("--CAMERA-- %s ...CAMERA_EFFECT_SEPIA\n",__func__);
			rc = OV2675Core_WritePREG(ov2675_effect_sepia_tbl);		
			break;
		
	
	case CAMERA_EFFECT_NEGATIVE:
		
			pr_err("--CAMERA-- %s ...CAMERA_EFFECT_NEGATIVE\n",__func__);
			rc = OV2675Core_WritePREG(ov2675_effect_negative_tbl);			
			break;
	        
	default:
		
			pr_err("--CAMERA-- %s ...Default(Not Support)\n",__func__);
		       break;
	}
	//ov2675_effect = effect;  By HT
	//Refresh Sequencer /
	pr_err("--CAMERA-- %s ...(End)\n",__func__);
	return rc;
}

static int ov2675_set_brightness(int8_t brightness)
{
	int rc = 0;
	pr_err("--CAMERA-- %s ...(Start)\n",__func__);
	pr_err("--CAMERA-- %s ...brightness = %d\n",__func__ , brightness);
	
	switch (brightness)
	{
	
	case CAMERA_BRIGHTNESS_0:
		pr_err("--CAMERA--CAMERA_BRIGHTNESS_LV1\n");
		rc = OV2675Core_WritePREG(ov2675_brightness_lv1_tbl);  					
		break;

	case CAMERA_BRIGHTNESS_1:
		pr_err("--CAMERA--CAMERA_BRIGHTNESS_LV2\n");
		rc = OV2675Core_WritePREG(ov2675_brightness_lv2_tbl);					
		break;

	case CAMERA_BRIGHTNESS_2:
		pr_err("--CAMERA--CAMERA_BRIGHTNESS_LV3\n");
		rc = OV2675Core_WritePREG(ov2675_brightness_lv3_tbl);					
		break;

	case CAMERA_BRIGHTNESS_3:
		pr_err("--CAMERA--CAMERA_BRIGHTNESS_LV4\n");
		rc = OV2675Core_WritePREG(ov2675_brightness_default_lv4_tbl);		
		break;

	case CAMERA_BRIGHTNESS_4:
		pr_err("--CAMERA--CAMERA_BRIGHTNESS_LV5\n");
		rc = OV2675Core_WritePREG(ov2675_brightness_lv5_tbl);	
		break;

	case CAMERA_BRIGHTNESS_5:
		pr_err("--CAMERA--CAMERA_BRIGHTNESS_LV6\n");
		rc = OV2675Core_WritePREG(ov2675_brightness_lv6_tbl);	
		break;

	case CAMERA_BRIGHTNESS_6:
		pr_err("--CAMERA--CAMERA_BRIGHTNESS_LV7\n");
		rc = OV2675Core_WritePREG(ov2675_brightness_lv7_tbl);
		break;

	default:
		pr_err("--CAMERA--CAMERA_BRIGHTNESS_ERROR COMMAND\n");
		break;
	}
	pr_err("--CAMERA-- %s ...(End)\n",__func__);	
	return rc;
}

static int ov2675_set_contrast(int contrast)
{
	int rc = 0;
	pr_err("--CAMERA-- %s ...(Start)\n",__func__);
	pr_err("--CAMERA-- %s ...contrast = %d\n",__func__ , contrast);
#if 1
	if (effect_value == CAMERA_EFFECT_OFF)
	{
		switch (contrast)
		{
		#if 0
		case CAMERA_CONTRAST_0:
			pr_err("--CAMERA--CAMERA_CONTRAST_LV0\n");
			rc = OV2675Core_WritePREG(ov2675_contrast_lv0_tbl);
			break;
		case CAMERA_CONTRAST_1:
			pr_err("--CAMERA--CAMERA_CONTRAST_LV1\n");
			rc = OV2675Core_WritePREG(ov2675_contrast_lv1_tbl);
			break;
		#endif
		case CAMERA_CONTRAST_0:
			pr_err("--CAMERA--CAMERA_CONTRAST_LV2\n");
			rc = OV2675Core_WritePREG(ov2675_contrast_lv2_tbl);
			break;
		case CAMERA_CONTRAST_1:
			pr_err("--CAMERA--CAMERA_CONTRAST_LV3\n");
			rc = OV2675Core_WritePREG(ov2675_contrast_lv3_tbl);
			break;
		case CAMERA_CONTRAST_2:
			pr_err("--CAMERA--CAMERA_CONTRAST_LV4\n");
			rc = OV2675Core_WritePREG(ov2675_contrast_default_lv4_tbl);
			break;

		case CAMERA_CONTRAST_3:
			pr_err("--CAMERA--CAMERA_CONTRAST_LV5\n");
			rc = OV2675Core_WritePREG(ov2675_contrast_lv5_tbl);
			break;
		case CAMERA_CONTRAST_4:
			pr_err("--CAMERA--CAMERA_CONTRAST_LV6\n");
			rc = OV2675Core_WritePREG(ov2675_contrast_lv6_tbl);
			break;
		#if 0
		case CAMERA_CONTRAST_LV7:
			pr_err("--CAMERA--CAMERA_CONTRAST_LV7\n");
			rc = OV2675Core_WritePREG(ov2675_contrast_lv7_tbl);
			break;
		case CAMERA_CONTRAST_LV8:
			pr_err("--CAMERA--CAMERA_CONTRAST_LV8\n");
			rc = OV2675Core_WritePREG(ov2675_contrast_lv8_tbl);
			break;
		#endif
		default:
			pr_err("--CAMERA--CAMERA_CONTRAST_ERROR COMMAND\n");
			break;
		}
	}
	pr_err("--CAMERA-- %s ...(End)\n",__func__);
	#endif
	return rc;
}

static int ov2675_set_sharpness(int sharpness)
{
	int rc = 0;
	pr_err("--CAMERA-- %s ...(Start)\n",__func__);
	pr_err("--CAMERA-- %s ...sharpness = %d\n",__func__ , sharpness);
#if 1
	if (effect_value == CAMERA_EFFECT_OFF)    //By HT
	{
		switch(sharpness)
		{
		case CAMERA_SHARPNESS_0:
			pr_err("--CAMERA--CAMERA_SHARPNESS_LV0\n");
			rc = OV2675Core_WritePREG(ov2675_sharpness_lv0_tbl);			
			break;
		case CAMERA_SHARPNESS_1:
			pr_err("--CAMERA--CAMERA_SHARPNESS_LV1\n");
			rc = OV2675Core_WritePREG(ov2675_sharpness_lv1_tbl);			

			break;
		case CAMERA_SHARPNESS_2:
			pr_err("--CAMERA--CAMERA_SHARPNESS_LV2\n");
			rc = OV2675Core_WritePREG(ov2675_sharpness_default_lv2_tbl);			

			break;
		case CAMERA_SHARPNESS_3:
			pr_err("--CAMERA--CAMERA_SHARPNESS_LV3\n");
			rc = OV2675Core_WritePREG(ov2675_sharpness_lv3_tbl);			

			break;
		case CAMERA_SHARPNESS_4:
			pr_err("--CAMERA--CAMERA_SHARPNESS_LV4\n");
			rc = OV2675Core_WritePREG(ov2675_sharpness_lv4_tbl);			

			break;
			#if 0
		case CAMERA_SHARPNESS_LV5:
			pr_err("--CAMERA--CAMERA_SHARPNESS_LV5\n");
			rc = OV2675Core_WritePREG(ov2675_sharpness_lv5_tbl);			

			break;
		case CAMERA_SHARPNESS_LV6:
			pr_err("--CAMERA--CAMERA_SHARPNESS_LV6\n");
			rc = OV2675Core_WritePREG(ov2675_sharpness_lv6_tbl);			

			break;
		case CAMERA_SHARPNESS_LV7:
			pr_err("--CAMERA--CAMERA_SHARPNESS_LV7\n");
			rc = OV2675Core_WritePREG(ov2675_sharpness_lv7_tbl);			

			break;
		case CAMERA_SHARPNESS_LV8:
			pr_err("--CAMERA--CAMERA_SHARPNESS_LV8\n");
			rc = OV2675Core_WritePREG(ov2675_sharpness_lv8_tbl);			

			break;
			#endif
		default:
			pr_err("--CAMERA--CAMERA_SHARPNESS_ERROR COMMAND\n");
			break;
		}
	}

	pr_err("--CAMERA-- %s ...(End)\n",__func__);
	#endif
	return rc;
}

static int ov2675_set_saturation(int saturation)
{
	long rc = 0;
//	int i = 0;
	pr_err("--CAMERA-- %s ...(Start)\n",__func__);
	pr_err("--CAMERA-- %s ...saturation = %d\n",__func__ , saturation);
#if 1
	if (effect_value == CAMERA_EFFECT_OFF)
	{ 
		switch (saturation)
		{
			#if 0		
		case CAMERA_SATURATION_0:
			pr_err("--CAMERA--CAMERA_SATURATION_LV0\n");
			rc = OV2675Core_WritePREG(ov2675_saturation_lv0_tbl);
			break;
		case CAMERA_SATURATION_1:
			pr_err("--CAMERA--CAMERA_SATURATION_LV1\n");
			rc = OV2675Core_WritePREG(ov2675_saturation_lv1_tbl);

			break;
			#endif			
		case CAMERA_SATURATION_0:
			pr_err("--CAMERA--CAMERA_SATURATION_LV2\n");
			rc = OV2675Core_WritePREG(ov2675_saturation_lv2_tbl);

			break;
		case CAMERA_SATURATION_1:
			pr_err("--CAMERA--CAMERA_SATURATION_LV3\n");
			rc = OV2675Core_WritePREG(ov2675_saturation_lv3_tbl);

			break;
		case CAMERA_SATURATION_2:
			pr_err("--CAMERA--CAMERA_SATURATION_LV4\n");
			rc = OV2675Core_WritePREG(ov2675_saturation_default_lv4_tbl);

			break;

		case CAMERA_SATURATION_3:
			pr_err("--CAMERA--CAMERA_SATURATION_LV5\n");
			rc = OV2675Core_WritePREG(ov2675_saturation_lv5_tbl);

			break;
		case CAMERA_SATURATION_4:
			pr_err("--CAMERA--CAMERA_SATURATION_LV6\n");
			rc = OV2675Core_WritePREG(ov2675_saturation_lv6_tbl);

			break;
			#if 0			
		case CAMERA_SATURATION_LV7:
			pr_err("--CAMERA--CAMERA_SATURATION_LV7\n");
			rc = OV2675Core_WritePREG(ov2675_saturation_lv7_tbl);

			break;
		case CAMERA_SATURATION_LV8:
			pr_err("--CAMERA--CAMERA_SATURATION_LV8\n");
			rc = OV2675Core_WritePREG(ov2675_saturation_lv8_tbl);
			#endif

			break;		
		default:
			pr_err("--CAMERA--CAMERA_SATURATION_ERROR COMMAND\n");
			break;
		}
	}
	
	  //for recover saturation level when change special effect
	 # if 1
		switch (saturation)
		{
		#if 0
		case CAMERA_SATURATION_LV0:
			pr_err("--CAMERA--CAMERA_SATURATION_LV0\n");
			SAT_U = 0x00;
			SAT_V = 0x00;
			break;
		case CAMERA_SATURATION_LV1:
			pr_err("--CAMERA--CAMERA_SATURATION_LV1\n");
			SAT_U = 0x10;
			SAT_V = 0x10;
			break;
		#endif
		case CAMERA_SATURATION_0:
			pr_err("--CAMERA--CAMERA_SATURATION_LV2\n");
			SAT_U = 0x20;
			SAT_V = 0x20;
			break;
		case CAMERA_SATURATION_1:
			pr_err("--CAMERA--CAMERA_SATURATION_LV3\n");
			SAT_U = 0x30;
			SAT_V = 0x30;
			break;
		case CAMERA_SATURATION_2:
			pr_err("--CAMERA--CAMERA_SATURATION_LV4\n");
			SAT_U = 0x40;
			SAT_V = 0x40;			
			break;
		case CAMERA_SATURATION_3:
			pr_err("--CAMERA--CAMERA_SATURATION_LV5\n");
			SAT_U = 0x50;
			SAT_V = 0x50;			
			break;
		case CAMERA_SATURATION_4:
			pr_err("--CAMERA--CAMERA_SATURATION_LV6\n");
			SAT_U = 0x60;
			SAT_V = 0x60;
			break;
			#if 0
		case CAMERA_SATURATION_LV7:
			pr_err("--CAMERA--CAMERA_SATURATION_LV7\n");
			SAT_U = 0x70;
			SAT_V = 0x70;			
			break;
		case CAMERA_SATURATION_LV8:
			pr_err("--CAMERA--CAMERA_SATURATION_LV8\n");
			SAT_U = 0x80;
			SAT_V = 0x80;
			break;	
			#endif
		default:
			pr_err("--CAMERA--CAMERA_SATURATION_ERROR COMMAND\n");
			break;
		}
  	#endif
	pr_err("--CAMERA-- %s ...(End)\n",__func__);
	#endif
	return rc;
}

static long ov2675_set_antibanding(int antibanding)
{
	long rc = 0;
//	int i = 0;
	pr_err("--CAMERA-- %s ...(Start)\n", __func__);
	pr_err("--CAMERA-- %s ...antibanding = %d\n", __func__, antibanding);
#if  1
	switch (antibanding)
	{
	case CAMERA_ANTIBANDING_SET_OFF:
		pr_err("--CAMERA--CAMERA_ANTIBANDING_OFF\n");
		break;

	case CAMERA_ANTIBANDING_SET_60HZ:
		pr_err("--CAMERA--CAMERA_ANTIBANDING_60HZ\n");
		rc = OV2675Core_WritePREG(ov2675_antibanding_60z_tbl);
		break;

	case CAMERA_ANTIBANDING_SET_50HZ:
		pr_err("--CAMERA--CAMERA_ANTIBANDING_50HZ\n");
		rc = OV2675Core_WritePREG(ov2675_antibanding_50z_tbl);
		break;

	case CAMERA_ANTIBANDING_SET_AUTO:
		pr_err("--CAMERA--CAMERA_ANTIBANDING_AUTO\n");
		rc = OV2675Core_WritePREG(ov2675_antibanding_50z_tbl);
		rc = OV2675Core_WritePREG(ov2675_antibanding_auto_tbl);
		break;

	default:
		pr_err("--CAMERA--CAMERA_ANTIBANDING_ERROR COMMAND\n");
		break;
	}
	pr_err("--CAMERA-- %s ...(End)\n",__func__);
	#endif
	return rc;
}
#if 0
static long ov2675_set_exposure_mode(int mode)
{
	long rc = 0;
	//int i =0;
	pr_err("--CAMERA-- %s ...(Start)\n",__func__);
	pr_err("--CAMERA-- %s ...mode = %d\n",__func__ , mode);
#if 0	
	switch (mode)
	{
	case CAMERA_SETAE_AVERAGE:
		pr_err("--CAMERA--CAMERA_SETAE_AVERAGE\n");
		rc = OV2675Core_WritePREG(ov2675_ae_average_tbl);
		break;
	case CAMERA_SETAE_CENWEIGHT:
		pr_err("--CAMERA--CAMERA_SETAE_CENWEIGHT\n");
		rc = OV2675Core_WritePREG(ov2675_ae_centerweight_tbl);		
		break;
	default:
		pr_err("--CAMERA--ERROR COMMAND OR NOT SUPPORT\n");
		break;
	}
#endif
	pr_err("--CAMERA-- %s ...(End)\n",__func__);
	return rc;
}
#endif
static int32_t ov2675_lens_shading_enable(uint8_t is_enable)
{
	int32_t rc = 0;
	pr_err("--CAMERA--%s: ...(Start). enable = %d\n", __func__, is_enable);

	if(is_enable)
	{
		pr_err("%s: enable~!!\n", __func__);
		rc = OV2675Core_WritePREG(ov2675_lens_shading_on_tbl);
	}
	else
	{
		pr_err("%s: disable~!!\n", __func__);
		rc = OV2675Core_WritePREG(ov2675_lens_shading_off_tbl);
	}
	pr_err("--CAMERA--%s: ...(End). rc = %d\n", __func__, rc);
	return rc;
}

static int ov2675_set_iso(int iso_val)
	/*
{
    int32_t rc = 0;
    pr_err("%s: entry: iso_val=%d\n", __func__, iso_val);	
   return rc;
}
*/
{
	long rc = 0;
	//int i = 0;
	pr_err("--CAMERA-- %s ...(Start)\n",__func__);
	pr_err("--CAMERA-- %s ...iso = %d\n",__func__ , iso_val);
#if 1
	{ 
	switch (iso_val)
	{
		case CAMERA_ISO_SET_AUTO:
		{
			pr_err("--CAMERA--CAMERA_ISO_SET_AUTO\n");
			//rc = ov2675_i2c_write(0x3015 ,0x02);
			rc = ov2675_i2c_write(ov2675_client->addr, 0x3015, 0x02, 10);
			if (rc < 0)
			{
			   return rc;
			}
       	}
			 break;
			 
		case CAMERA_ISO_SET_100:
		{
			pr_err("--CAMERA--CAMERA_ISO_SET_100\n");
			//rc = ov2675_i2c_write(0x3015 ,0x01);
			rc = ov2675_i2c_write(ov2675_client->addr, 0x3015, 0x01, 10);
			if (rc < 0)
			{
			   return rc;
			}
       	}
			break;
		
		case CAMERA_ISO_SET_200:
		{
			pr_err("--CAMERA--CAMERA_ISO_SET_200\n");
			//rc = ov2675_i2c_write(0x3015 ,0x02);
			rc = ov2675_i2c_write(ov2675_client->addr, 0x3015, 0x02, 10);
			if (rc < 0)
			{
			   return rc;
			}
      		 }
			break;
		
		case CAMERA_ISO_SET_400:
		{
			pr_err("--CAMERA--CAMERA_ISO_SET_400\n");
			// rc = ov2675_i2c_write(0x3A18 ,0x03);
			rc = ov2675_i2c_write(ov2675_client->addr, 0x3015, 0x03, 10);
			if (rc < 0)
			{
			   return rc;
			}
      		 }
			break;
		
		case CAMERA_ISO_SET_800:
		{
			pr_err("--CAMERA--CAMERA_ISO_SET_800\n");
			//rc = ov2675_i2c_write(0x3015 ,0x04);
			rc = ov2675_i2c_write(ov2675_client->addr, 0x3015, 0x04, 10);
			if (rc < 0)
			{
			   return rc;
			}
      		 }
			break;
		
		default:
			pr_err("--CAMERA--CAMERA_SATURATION_ERROR COMMAND\n");
			break;
		}
	}	

 #endif 	
	pr_err("--CAMERA-- %s ...(End)\n",__func__);

	return rc;
}

static int ov2675_set_sensor_mode(int mode, int res)
{
	int rc = 0;

	pr_err("--CAMERA-- ov2675_set_sensor_mode mode = %d, res = %d\n", mode, res);

	switch (mode)
	{
	case SENSOR_PREVIEW_MODE:
		pr_err("--CAMERA-- SENSOR_PREVIEW_MODE\n");
		rc = ov2675_setting(S_UPDATE_PERIODIC, S_RES_PREVIEW);
		break;

	case SENSOR_SNAPSHOT_MODE:
		pr_err("--CAMERA-- SENSOR_SNAPSHOT_MODE\n");
		rc = ov2675_setting(S_UPDATE_PERIODIC, S_RES_CAPTURE);
		break;

	case SENSOR_RAW_SNAPSHOT_MODE:
	default:
		pr_err("--CAMERA--ov2675_set_sensor_mode no support\n");
		rc = -EINVAL;
		break;
	}

	return rc;
}

#if 1
static int ov2675_set_wb_oem(uint8_t param)
{
	int rc = 0;

	pr_err("[kylin] %s \r\n",__func__ );
//    unsigned int tmp2;

#if 1
	switch(param)
	{
	case CAMERA_WB_MODE_AWB:

		pr_err("--CAMERA--CAMERA_WB_AUTO\n");
		rc = OV2675Core_WritePREG(ov2675_wb_def);
		break;

	case CAMERA_WB_MODE_SUNLIGHT:
		pr_err("--CAMERA--CAMERA_WB_CUSTOM\n");
		rc = OV2675Core_WritePREG(ov2675_wb_daylight);   

		break;
	case CAMERA_WB_MODE_INCANDESCENT:
		pr_err("--CAMERA--CAMERA_WB_INCANDESCENT\n");
		rc = OV2675Core_WritePREG(ov2675_wb_inc);   	
		break;
	case CAMERA_WB_MODE_FLUORESCENT:
		pr_err("--CAMERA--CAMERA_WB_DAYLIGHT\n");
		rc = OV2675Core_WritePREG(ov2675_wb_custom);   	
		break;
	case CAMERA_WB_MODE_CLOUDY:
		pr_err("--CAMERA--CAMERA_WB_CLOUDY_DAYLIGHT\n");
		rc = OV2675Core_WritePREG(ov2675_wb_cloudy);   	
		break;
	default:
		break;
	}
	#endif
	return rc;
}

#endif


//QRD
static int ov2675_set_exposure_compensation(int compensation)
{
	long rc = 0;
//	int i = 0;

	pr_err("--CAMERA-- %s ...(Start)\n",__func__);

	pr_err("--CAMERA-- %s ...exposure_compensation = %d\n",__func__ , compensation);

	switch(compensation)
	{
case CAMERA_EXPOSURE_0:
	pr_err("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV0\n");
	rc = OV2675Core_WritePREG(ov2675_exposure_compensation_lv4_tbl);   		
	break;
case CAMERA_EXPOSURE_1:
	pr_err("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV1\n");
	rc = OV2675Core_WritePREG(ov2675_exposure_compensation_lv3_tbl);   		
	break;
case CAMERA_EXPOSURE_2:
	pr_err("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV2\n");
	rc = OV2675Core_WritePREG(ov2675_exposure_compensation_lv2_default_tbl);   		
	break;
case CAMERA_EXPOSURE_3:
	pr_err("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV3\n");
	rc = OV2675Core_WritePREG(ov2675_exposure_compensation_lv1_tbl);   		
	break;
case CAMERA_EXPOSURE_4:
	pr_err("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV3\n");
	rc = OV2675Core_WritePREG(ov2675_exposure_compensation_lv0_tbl);   		
	break;
default:
	pr_err("--CAMERA--ERROR CAMERA_EXPOSURE_COMPENSATION\n");
	break;
	}

	pr_err("--CAMERA-- %s ...(End)\n",__func__);

	return rc;
}


static int ov2675_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long rc = 0;

	if (copy_from_user(&cdata, (void *)argp, sizeof(struct sensor_cfg_data))) 
		return -EFAULT;

	pr_err("--CAMERA-- %s %d\n",__func__,cdata.cfgtype);

	mutex_lock(&ov2675_mutex);
	switch (cdata.cfgtype)
	{
	case CFG_SET_MODE:   // 0
		rc =ov2675_set_sensor_mode(cdata.mode, cdata.rs);
		break;
	case CFG_SET_EFFECT: // 1
		pr_err("--CAMERA-- CFG_SET_EFFECT mode=%d, effect = %d !!\n",cdata.mode, cdata.cfg.effect);
		rc = ov2675_set_effect(cdata.mode, cdata.cfg.effect);
		break;
	case CFG_START:      // 2
		pr_err("--CAMERA-- CFG_START (Not Support) !!\n");
		// Not Support
		break;
	case CFG_PWR_UP:     // 3
		pr_err("--CAMERA-- CFG_PWR_UP (Not Support) !!\n");
		// Not Support
		break;
	case CFG_PWR_DOWN:   // 4
		pr_err("--CAMERA-- CFG_PWR_DOWN (Not Support) \n");
		ov2675_power_off();
		break;
	case CFG_SET_DEFAULT_FOCUS:  // 06
		pr_err("--CAMERA-- CFG_SET_DEFAULT_FOCUS (Not Implement) !!\n");
		break;        
	case CFG_MOVE_FOCUS:     //  07
		pr_err("--CAMERA-- CFG_MOVE_FOCUS (Not Implement) !!\n");
		break;
	case CFG_SET_BRIGHTNESS:     //  12
		pr_err("--CAMERA-- CFG_SET_BRIGHTNESS  !!\n");
		rc = ov2675_set_brightness(cdata.cfg.brightness);
		break;
	case CFG_SET_CONTRAST:     //  13
		pr_err("--CAMERA-- CFG_SET_CONTRAST  !!\n");
		rc = ov2675_set_contrast(cdata.cfg.contrast);
		break;            
	case CFG_SET_EXPOSURE_MODE:     //  15
	#if 0
		pr_err("--CAMERA-- CFG_SET_EXPOSURE_MODE !!\n");
		rc = ov2675_set_exposure_mode(cdata.cfg.ae_mode);
	#endif
		break;
	case CFG_SET_ANTIBANDING:     //  17
		pr_err("--CAMERA-- CFG_SET_ANTIBANDING antibanding = %d!!\n", cdata.cfg.antibanding);
		rc = ov2675_set_antibanding(cdata.cfg.antibanding);
		break;
	case CFG_SET_LENS_SHADING:     //  20
		pr_err("--CAMERA-- CFG_SET_LENS_SHADING !!\n");
		rc = ov2675_lens_shading_enable(cdata.cfg.lens_shading);
		break;
	case CFG_SET_SATURATION:     //  30
		pr_err("--CAMERA-- CFG_SET_SATURATION !!\n");
		rc = ov2675_set_saturation(cdata.cfg.saturation);
		break;
	case CFG_SET_SHARPNESS:     //  31
		pr_err("--CAMERA-- CFG_SET_SHARPNESS !!\n");
		rc = ov2675_set_sharpness(cdata.cfg.sharpness);
		break;
	case CFG_SET_WB:
		pr_err("--CAMERA-- CFG_SET_WB!!\n");
		rc = ov2675_set_wb_oem(cdata.cfg.wb_mode);
		break;		
	case CFG_SET_ISO:
		pr_err("--CAMERA-- CFG_SET_EXPOSURE_COMPENSATION !\n");
		rc = ov2675_set_iso(cdata.cfg.iso_val);
		break;
	case CFG_SET_EXPOSURE_COMPENSATION:
		pr_err("--CAMERA-- CFG_SET_EXPOSURE_COMPENSATION !\n");
		rc = ov2675_set_exposure_compensation(cdata.cfg.exp_compensation);
		break;
#if 0
	case CFG_SET_TOUCHAEC:
		pr_err("--CAMERA-- CFG_SET_TOUCHAEC!!(Not Support)\n");
		rc = 0 ;
		break;

	case CFG_SET_AUTO_FOCUS:
		pr_err("--CAMERA-- CFG_SET_AUTO_FOCUS !(Not Support)\n");
		rc = 0;
		break;
	case CFG_SET_AUTOFLASH:
		pr_err("--CAMERA-- CFG_SET_AUTOFLASH !(Not Support)\n");
		rc = 0;
		break;

	case CFG_SET_EXPOSURE_COMPENSATION:
		pr_err("--CAMERA-- CFG_SET_EXPOSURE_COMPENSATION !\n");
		rc = 0;
		break;
#endif
	default:
		pr_err("--CAMERA-- %s: Command=%d (Not Implement)!!\n",__func__,cdata.cfgtype);
		rc = -EINVAL;
		break;    
	}
	mutex_unlock(&ov2675_mutex);
	return rc;    
}

static struct i2c_driver ov2675_i2c_driver = {
	.id_table = ov2675_i2c_id,
	.probe  = ov2675_i2c_probe,
	.remove = ov2675_i2c_remove,
	.driver = {
		.name = "ov2675",
	},
};

static int ov2675_probe_init_gpio(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	pr_err("--CAMERA-- %s Open CAMIO CLK,set to default clk rate!\n",__func__);

	ov2675_pwdn_gpio = data->sensor_pwd;
	ov2675_reset_gpio = data->sensor_reset;

	pr_err("--CAMERA-- %s : sensor_pwd_pin=%d, sensor_reset_pin=%d\n",__func__,data->sensor_pwd,data->sensor_reset);

	rc = gpio_request(data->sensor_pwd, "ov2675");
	pr_err("--CAMERA-- %s : gpio_request=%d, result is %d",__func__,data->sensor_pwd, rc);
	if (rc < 0) {
		goto gpio_error;
	}
#if 0
	rc = gpio_tlmm_config(GPIO_CFG(data->sensor_pwd, 0,
                            GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP,
                            GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	
	pr_err("--CAMERA-- %s : gpio_tlmm_config=%d, result is %d", __func__, data->sensor_pwd, rc);
	if (rc < 0) {
		goto gpio_error;
	}
#endif
	rc |= gpio_request(data->sensor_reset, "ov2675");
	pr_err("--CAMERA-- %s : gpio_request=%d, result is %d",__func__,data->sensor_reset, rc);
#if 0
	if (rc < 0) {
		goto gpio_error;
	}
    rc = gpio_tlmm_config(GPIO_CFG(data->sensor_reset, 0,
                            GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN,
                            GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	pr_err("--CAMERA-- %s : gpio_tlmm_config=%d, result is %d", __func__, data->sensor_reset, rc);
	if (rc < 0) {
		goto gpio_error;
	}
#endif

	gpio_direction_output(data->sensor_reset, 1);
	gpio_direction_output(data->sensor_pwd, 1);

	return rc;
gpio_error:
	gpio_free(data->sensor_pwd);
	gpio_free(data->sensor_reset);

    return rc;
}

static void ov2675_probe_free_gpio(void)
{
	gpio_free(ov2675_pwdn_gpio);
	gpio_free(ov2675_reset_gpio);
	ov2675_pwdn_gpio = 0xFF;
	ov2675_reset_gpio = 0xFF;
}

static int ov2675_sensor_probe(const struct msm_camera_sensor_info *info,struct msm_sensor_ctrl *s)
{
	int rc = -ENOTSUPP;
	pr_err("--CAMERA-- %s (Start...)\n",__func__);
	rc = i2c_add_driver(&ov2675_i2c_driver);
	pr_err("--CAMERA-- i2c_add_driver ret:0x%x,ov2675_client=0x%x\n",
		rc, (unsigned int)ov2675_client);
	if ((rc < 0 ) || (ov2675_client == NULL))
	{
		pr_err("--CAMERA-- i2c_add_driver FAILS!!\n");
		return rc;
	}

	rc = ov2675_probe_init_gpio(info);

	msleep(5);
	rc = msm_camera_power_backend(MSM_CAMERA_PWRUP_MODE);
	mdelay(1);
	ov2675_power_off();

	/* SENSOR NEED MCLK TO DO I2C COMMUNICTION, OPEN CLK FIRST*/
	msm_camio_clk_rate_set(24000000);

	mdelay(5);

	ov2675_power_on();
	ov2675_power_reset();

	rc |= ov2675_probe_readID(info);

	if (rc < 0)
	{ 
		pr_err("--CAMERA--ov2675_probe_readID Fail !!~~~~!!\n");
		pr_err("--CAMERA-- %s, unregister\n",__func__);

		i2c_del_driver(&ov2675_i2c_driver);
		ov2675_power_off();
		ov2675_probe_free_gpio();

		return rc;
	}

	s->s_init = ov2675_sensor_open_init;
	s->s_release = ov2675_sensor_release;
	s->s_config  = ov2675_sensor_config;
	//s->s_AF = ov2675_sensor_set_af;
	//camera_init_flag = true;

	s->s_camera_type = BACK_CAMERA_2D;
	s->s_mount_angle = info->sensor_platform_info->mount_angle;

	ov2675_power_off();

	pr_err("--CAMERA-- %s (End...)\n",__func__);
	return rc;
}

static int ov2675_i2c_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	pr_err("--CAMERA-- %s ... (Start...)\n",__func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		pr_err("--CAMERA--i2c_check_functionality failed\n");
		return -ENOMEM;
	}

	ov2675_sensorw = kzalloc(sizeof(struct ov2675_work), GFP_KERNEL);
	if (!ov2675_sensorw)
	{
		pr_err("--CAMERA--kzalloc failed\n");
		return -ENOMEM;
	}
	i2c_set_clientdata(client, ov2675_sensorw);
	ov2675_init_client(client);
	ov2675_client = client;

	pr_err("--CAMERA-- %s ... (End...)\n",__func__);
	return 0;
}

static int __ov2675_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, ov2675_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __ov2675_probe,
	.driver = {
		.name = "msm_camera_ov2675",
		.owner = THIS_MODULE,
	},
};

static int __init ov2675_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(ov2675_init);

MODULE_DESCRIPTION("ov2675 YUV MIPI sensor driver");
MODULE_LICENSE("GPL v2");
