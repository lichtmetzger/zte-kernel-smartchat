/*
 * =====================================================================================
 *
 *       Filename:  lcdc_panel_qvga_lead.c
 *
 *    Description:  Lead QVGA panel(240x320) driver which ic No. is 9325
 *
 *        Version:  1.0
 *        Created:  11/12/2009 02:05:46 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Li Xiongwei
 *        Company:  ZTE Corp.
 *
 * =====================================================================================
 */
/* ========================================================================================
when         who        what, where, why                                  comment tag
--------     ----       -----------------------------                --------------------------
2011-03-24   lht		   modify delay	 CRDB00629443        	ZTE_LCD_LUYA_20110324_001
2010-06-11   lht		   project mode display panel info         	ZTE_LCD_LHT_20100611_001
2010-02-21   luya		merge samsung IC for QVGA &pull down spi when sleep		ZTE_LCD_LUYA_20100221_001        
2010-02-01   luya    	turn off BKL when fb enter early suspend					 ZTE_LCD_LUYA_20100201_001
2009-12-29   wly               ajust max backlight level                     ZTE_KEYBOARD_WLY_CRDB00421949        
2009-11-27   lixiongwei        merge new driver for lead 9325c qvga          ZTE_LCD_LIXW_001
2009-12-21   luya			   do not do panel_init for the 1st time		 ZTE_LCD_LUYA_20091221_001
2009-12-26   luya			   merge nopanel(QVGA)driver to avoid the problem 
							   of bootup abnormally without lcd panel		 ZTE_LCD_LUYA_20091226_001

==========================================================================================*/


#include "msm_fb.h"
#include <asm/gpio.h>
#include <linux/module.h>
#include <linux/delay.h>



#define GPIO_LCD_BL_SC_OUT 97

static boolean is_firsttime = true;		///ZTE_LCD_fxy_20111124	
static boolean lcd_init_once = false;
uint32 gpio_ic_lead;			//ZTE_LCD_LUYA_20091226_001
static bool onewiremode = true;  //added by zte_gequn091966,20110428
static int spi_cs;
static int spi_sclk;
static int spi_sdi;
static int spi_sdo;
static int lcd_panel_reset;
static int lcd_bkl_ctl;
static int lcd_ic_id;
extern u32 LcdPanleID;    //ZTE_LCD_LHT_20100611_001

typedef enum
{
	LCD_PANEL_NONE = 0,
	LCD_PANEL_YASSY_QVGA,
	LCD_PANEL_LEAD_QVGA	
}LCD_PANEL_TYPE;

LCD_PANEL_TYPE g_lcd_panel_type = LCD_PANEL_NONE;
static void lcdc_yassy_init(void);
static void lcdc_yassy_sleep(void);



static struct msm_panel_common_pdata * lcdc_lead_pdata;
static void lcdc_lead_sleep(void);
static void lcdc_lead_init(void);
static void lcdc_set_bl(struct msm_fb_data_type *mfd);

static void spi_init(void);
static int lcdc_panel_on(struct platform_device *pdev);
static int lcdc_panel_off(struct platform_device *pdev);

static void HX8368_WriteReg(unsigned char SPI_COMMD)
{
	unsigned short SBit,SBuffer;
	unsigned char BitCounter;
	
	SBuffer=SPI_COMMD;
	gpio_direction_output(spi_cs, 0);	
	for(BitCounter=0;BitCounter<9;BitCounter++)
	{
		SBit = SBuffer&0x100;
		if(SBit)
			gpio_direction_output(spi_sdo, 1);
		else
			gpio_direction_output(spi_sdo, 0);
			
		gpio_direction_output(spi_sclk, 0);
		gpio_direction_output(spi_sclk, 1);
		SBuffer = SBuffer<<1;
	}
	gpio_direction_output(spi_cs, 1);
}
//***********************************************
//***********************************************
static void HX8368_WriteData(unsigned char SPI_DATA)
{
	unsigned short SBit,SBuffer;
	unsigned char BitCounter;
	
	SBuffer=SPI_DATA | 0x100;
	gpio_direction_output(spi_cs, 0);//Set_CS(0); //CLR CS
	
	for(BitCounter=0;BitCounter<9;BitCounter++)
	{
		SBit = SBuffer&0x100;
		if(SBit)
			gpio_direction_output(spi_sdo, 1);//Set_SDA(1);
		else
			gpio_direction_output(spi_sdo, 0);//Set_SDA(0);
			
		gpio_direction_output(spi_sclk, 0);//Set_SCK(0); //CLR SCL
		gpio_direction_output(spi_sclk, 1);//Set_SCK(1); //SET SCL
		SBuffer = SBuffer<<1;
	}
	gpio_direction_output(spi_cs, 1);//Set_CS(1); //SET CS
}
static void lcdc_yassy_sleep(void)
{
	HX8368_WriteReg(0x28);
	HX8368_WriteReg(0x10);
	mdelay(120);//120MS
}

static void lcdc_yassy_init(void)
{
	int aa1; 
	HX8368_WriteReg(0x11);	
	msleep(120); 

	HX8368_WriteReg(0xB9);
	HX8368_WriteData(0xFF);
	HX8368_WriteData(0x83);
	HX8368_WriteData(0x68);
	msleep(20);
	HX8368_WriteReg(0xB3);
	HX8368_WriteData(0x0E);
	HX8368_WriteReg(0x36);
	HX8368_WriteData(0X08);//c8

	HX8368_WriteReg(0x3A);
	HX8368_WriteData(0x60);

	HX8368_WriteReg(0xEA);
	HX8368_WriteData(0x02);
	msleep(20);
	HX8368_WriteReg(0x2D);
	for(aa1=0;aa1<32;aa1++)
	{
	HX8368_WriteData(2*aa1);
	}
	for(aa1=0;aa1<64;aa1++)
	{
	HX8368_WriteData(1*aa1);
	}
	for(aa1=0;aa1<64;aa1++)
	{
	HX8368_WriteData(2*aa1);
	}
	msleep(50);
	HX8368_WriteReg(0xC0);
	HX8368_WriteData(0x1B);
	HX8368_WriteData(0x05);
	HX8368_WriteData(0x08);
	HX8368_WriteData(0xEC);
	HX8368_WriteData(0x00);
	HX8368_WriteData(0x01);

	HX8368_WriteReg(0xE3);
	HX8368_WriteData(0x00);
	HX8368_WriteData(0x4F);
	HX8368_WriteData(0x00);
	HX8368_WriteData(0x4F);

	HX8368_WriteReg(0xB1); //POWER SETTING
	HX8368_WriteData(0x00);
	HX8368_WriteData(0x02);
	HX8368_WriteData(0x1E);
	HX8368_WriteData(0x04);
	HX8368_WriteData(0x22);
	HX8368_WriteData(0x11);
	HX8368_WriteData(0xD4);

	HX8368_WriteReg(0xB6);
	HX8368_WriteData(0x81); //71 81
	HX8368_WriteData(0x6F); //93 6F
	HX8368_WriteData(0x50); //74 50

	HX8368_WriteReg(0xB0);
	HX8368_WriteData(0x0F);
	
	HX8368_WriteReg(0xE0);
	HX8368_WriteData(0x00); //V63
	HX8368_WriteData(0x21); //V62
	HX8368_WriteData(0x20); //V61
	HX8368_WriteData(0x1F); //V2
	HX8368_WriteData(0x21); //V1
	HX8368_WriteData(0x3F); //V0
	HX8368_WriteData(0x10); //V55
	HX8368_WriteData(0x57); //V8
	HX8368_WriteData(0x02); //V60
	HX8368_WriteData(0x05); //V43
	HX8368_WriteData(0x08); //V32
	HX8368_WriteData(0x0D); //V20
	HX8368_WriteData(0x15); //V3
	HX8368_WriteData(0x00); //V0
	HX8368_WriteData(0x1E); //V1
	HX8368_WriteData(0x20); //V2
	HX8368_WriteData(0x1F); //V61
	HX8368_WriteData(0x1E); //V62
	HX8368_WriteData(0x3F); //V63
	HX8368_WriteData(0x28); //V8
	HX8368_WriteData(0x6F); //V55
	HX8368_WriteData(0x1a); //V3
	HX8368_WriteData(0x12); //V20
	HX8368_WriteData(0x17); //V32
	HX8368_WriteData(0x1A); //V43
	HX8368_WriteData(0x1D); //V60
	HX8368_WriteData(0xFF); //
	msleep(50);
	HX8368_WriteReg(0x35);//FMKAR ON
	HX8368_WriteData(0x00); //V63
	HX8368_WriteReg(0x29);
	msleep(50);
	HX8368_WriteReg(0x2C);
	printk("lcd module TFT YASSY init finish\n!");
}

static void lcd_panel_sleep(void)
{
	switch(g_lcd_panel_type)
		{
			case LCD_PANEL_YASSY_QVGA:
				lcdc_yassy_sleep();
				mdelay(10);
				break;
			case LCD_PANEL_LEAD_QVGA:
				lcdc_lead_sleep();
				mdelay(10);
				break;
			default:
				break;
		}	
}

static void lcd_panel_init(void)
{	
	gpio_direction_output(lcd_panel_reset, 1);
	mdelay(10);
	gpio_direction_output(lcd_panel_reset, 0);
	mdelay(20);
	gpio_direction_output(lcd_panel_reset, 1);
	msleep(30);
	switch(g_lcd_panel_type)
	{
		case LCD_PANEL_YASSY_QVGA:
			lcdc_yassy_init();
			break;
		case LCD_PANEL_LEAD_QVGA:
			lcdc_lead_init();
			break;				
		default:
			break;
	}
}


static int lcdc_panel_on(struct platform_device *pdev)
{

///ZTE_LCD_LUYA_20091221_001,start	
	if(!is_firsttime)
	{
		lcd_panel_init();		
		lcd_init_once = true;
	}
	else
	{
		is_firsttime = false;
	}
///ZTE_LCD_LUYA_20091221_001,end	
	return 0;
}

static void lcdc_lead_sleep(void)
{
	HX8368_WriteReg(0x28);
	HX8368_WriteReg(0x10);
	mdelay(120);//120MS
}

static void lcdc_lead_init(void)
{ 
		HX8368_WriteReg(0x11); 
		msleep(120); 
		HX8368_WriteReg(0xB9);		  // Set EXTC 
		HX8368_WriteData(0xFF);   
		HX8368_WriteData(0x83);   
		HX8368_WriteData(0x68);   
		msleep(5); 
		HX8368_WriteReg(0xE3); 
		HX8368_WriteData(0x00); 
		HX8368_WriteData(0x60); 
		HX8368_WriteData(0x00); 
		HX8368_WriteData(0x60); 
		msleep(5);						   
		HX8368_WriteReg(0xBB); // Set OTP 
		HX8368_WriteData(0x00); 
		HX8368_WriteData(0x00); 
		HX8368_WriteData(0x80); 
		msleep(5); 
		HX8368_WriteReg(0xEA);		  // Set SETMESSI 
		HX8368_WriteData(0x00);   
		msleep(5); 
		HX8368_WriteReg(0xB3); 
		HX8368_WriteData(0x0E);//0x0F 
		HX8368_WriteData(0x08); 
		HX8368_WriteData(0x02); 
		msleep(5); 
		HX8368_WriteReg(0xC0);		  // Set OPON 
		HX8368_WriteData(0x30); 
		HX8368_WriteData(0x05); 
		HX8368_WriteData(0x09); 
		HX8368_WriteData(0x82);   
		msleep(5);			 
		HX8368_WriteReg(0xB6); // Set VCOM 
		HX8368_WriteData(0x74); 
		HX8368_WriteData(0x34); 
		HX8368_WriteData(0x64); 
		// Set Gamma 2.2 
		HX8368_WriteReg(0xE0); // Set Gamma 
		HX8368_WriteData(0x00); 
		HX8368_WriteData(0x29); 
		HX8368_WriteData(0x28); 
		HX8368_WriteData(0x3F); 
		HX8368_WriteData(0x3F); 
		HX8368_WriteData(0x3F); 
		HX8368_WriteData(0x25); 
		HX8368_WriteData(0x7F); 
		HX8368_WriteData(0x09); 
		HX8368_WriteData(0x07); 
		HX8368_WriteData(0x09); 
		HX8368_WriteData(0x0F); 
		HX8368_WriteData(0x1F); 
		HX8368_WriteData(0x00); 
		HX8368_WriteData(0x00); 
		HX8368_WriteData(0x00); 
		HX8368_WriteData(0x17); 
		HX8368_WriteData(0x16); 
		HX8368_WriteData(0x3F); 
		HX8368_WriteData(0x00); 
		HX8368_WriteData(0x5A); 
		HX8368_WriteData(0x00); 
		HX8368_WriteData(0x10); 
		HX8368_WriteData(0x16); 
		HX8368_WriteData(0x18); 
		HX8368_WriteData(0x17); 
		HX8368_WriteData(0xFF); 

		HX8368_WriteReg(0x3a); /// 
		HX8368_WriteData(0x60); 		
		msleep(5); 
		HX8368_WriteReg(0x36); /// 
		HX8368_WriteData(0x08);   
		msleep(5); 
		HX8368_WriteReg(0x29); 
		
		printk("lcd module TFT LEAD init finish\n!"); 
}

static void select_1wire_mode(void)
{
	gpio_direction_output(lcd_bkl_ctl, 1);
	udelay(120);
	gpio_direction_output(lcd_bkl_ctl, 0);
	udelay(280);				////ZTE_LCD_LUYA_20100226_001
	gpio_direction_output(lcd_bkl_ctl, 1);
	udelay(650);				////ZTE_LCD_LUYA_20100226_001
	
}
static void send_bkl_address(void)
{
	unsigned int i,j;
	i = 0x72;
	gpio_direction_output(lcd_bkl_ctl, 1);
	udelay(10);
	//printk("[LY] send_bkl_address \n");
	for(j = 0; j < 8; j++)
	{
		if(i & 0x80)
		{
			gpio_direction_output(lcd_bkl_ctl, 0);
			udelay(10);
			gpio_direction_output(lcd_bkl_ctl, 1);
			udelay(180);
		}
		else
		{
			gpio_direction_output(lcd_bkl_ctl, 0);
			udelay(180);
			gpio_direction_output(lcd_bkl_ctl, 1);
			udelay(10);
		}
		i <<= 1;
	}
	gpio_direction_output(lcd_bkl_ctl, 0);
	udelay(10);
	gpio_direction_output(lcd_bkl_ctl, 1);

}

static void send_bkl_data(int level)
{
	unsigned int i,j;
	i = level & 0x1F;
	gpio_direction_output(lcd_bkl_ctl, 1);
	udelay(10);
	//printk("[LY] send_bkl_data \n");
	for(j = 0; j < 8; j++)
	{
		if(i & 0x80)
		{
			gpio_direction_output(lcd_bkl_ctl, 0);
			udelay(10);
			gpio_direction_output(lcd_bkl_ctl, 1);
			udelay(180);
		}
		else
		{
			gpio_direction_output(lcd_bkl_ctl, 0);
			udelay(180);
			gpio_direction_output(lcd_bkl_ctl, 1);
			udelay(10);
		}
		i <<= 1;
	}
	gpio_direction_output(lcd_bkl_ctl, 0);
	udelay(10);
	gpio_direction_output(lcd_bkl_ctl, 1);

}
static void lcdc_set_bl(struct msm_fb_data_type *mfd)
{
       /*value range is 1--32*/
    int current_lel = mfd->bl_level;
    unsigned long flags;


    printk("[ZYF] lcdc_set_bl level=%d, %d\n", 
		   current_lel , mfd->panel_power_on);

	
    if(!mfd->panel_power_on)
	{
    	gpio_direction_output(lcd_bkl_ctl, 0);			///ZTE_LCD_LUYA_20100201_001
	    return;
    }

    if(current_lel < 1)
    {
        current_lel = 0;
    }
    if(current_lel > 32)
    {
        current_lel = 32;
    }

    /*ZTE_BACKLIGHT_WLY_005,@2009-11-28, set backlight as 32 levels, end*/
    local_irq_save(flags);
    if(current_lel==0)
    {
    	gpio_direction_output(lcd_bkl_ctl, 0);
		mdelay(3);
		onewiremode = FALSE;
			
    }
    else 
	{
		if(!onewiremode)	//select 1 wire mode
		{
			printk("[LY] before select_1wire_mode\n");
			select_1wire_mode();
			onewiremode = TRUE;
		}
		send_bkl_address();
		send_bkl_data(current_lel-1);

	}
    local_irq_restore(flags);
}

static void spi_init(void)
{
	spi_sclk = *(lcdc_lead_pdata->gpio_num);
	spi_cs   = *(lcdc_lead_pdata->gpio_num + 1);
	spi_sdi  = *(lcdc_lead_pdata->gpio_num + 2);
	spi_sdo  = *(lcdc_lead_pdata->gpio_num + 3);
	lcd_panel_reset = *(lcdc_lead_pdata->gpio_num + 4);
	lcd_bkl_ctl= *(lcdc_lead_pdata->gpio_num + 5);
	lcd_ic_id= *(lcdc_lead_pdata->gpio_num + 6);
	gpio_set_value(spi_sclk, 1);
	gpio_set_value(spi_sdo, 1);
	gpio_set_value(spi_cs, 1);
	mdelay(10);
}
static int lcdc_panel_off(struct platform_device *pdev)
{

	lcd_panel_sleep();

	gpio_direction_output(lcd_panel_reset, 0);

	///ZTE_LCD_LUYA_20100221_001
	gpio_direction_output(spi_sclk, 0);
	gpio_direction_output(spi_sdi, 0);
	gpio_direction_output(spi_sdo, 0);
	gpio_direction_output(spi_cs, 0);
	
	msleep(20);

	return 0;
}




static struct msm_fb_panel_data lcd_lcdcpanel_panel_data = {
       .panel_info = {.bl_max = 32},
	.on = lcdc_panel_on,
	.off = lcdc_panel_off,
       .set_backlight = lcdc_set_bl,
};

static struct platform_device this_device = {
	.name   = "lcdc_panel_qvga",
	.id	= 1,
	.dev	= {
		.platform_data = &lcd_lcdcpanel_panel_data,
	}
};



static LCD_PANEL_TYPE lcd_panel_detect(void)
{
	gpio_direction_input(lcd_ic_id);
	if(gpio_get_value(lcd_ic_id))
	{
		printk("lead id is LCD_PANEL_LEAD_QVGA");
		return LCD_PANEL_LEAD_QVGA;
	}
	else
	{
		printk("yassy id is LCD_PANEL_YASSY_QVGA");
		return LCD_PANEL_YASSY_QVGA;
	}		

}

static int __devinit lcdc_panel_probe(struct platform_device *pdev)
{
	struct msm_panel_info *pinfo;
	int ret;

	if(pdev->id == 0) {
		lcdc_lead_pdata = pdev->dev.platform_data;
		lcdc_lead_pdata->panel_config_gpio(1);
		spi_init();	
	
		g_lcd_panel_type = lcd_panel_detect();

		pinfo = &lcd_lcdcpanel_panel_data.panel_info;
		pinfo->xres = 320;
		pinfo->yres = 240;
		pinfo->type = LCDC_PANEL;
		pinfo->pdest = DISPLAY_1;
		pinfo->wait_cycle = 0;
		pinfo->bpp = 18;
		pinfo->fb_num = 2;			 
		pinfo->clk_rate = 6144000;

		if (g_lcd_panel_type == LCD_PANEL_YASSY_QVGA)
		{
			LcdPanleID=(u32)LCD_PANEL_P726_HX8347D;
				pinfo->lcdc.h_back_porch = 20;
				pinfo->lcdc.h_front_porch = 40;
				pinfo->lcdc.h_pulse_width = 10;
				pinfo->lcdc.v_back_porch = 2;
				pinfo->lcdc.v_front_porch = 4;
				pinfo->lcdc.v_pulse_width = 2;
				pinfo->lcdc.border_clr = 0;	/* blk */
				pinfo->lcdc.underflow_clr = 0xffff;	/* blue */
				pinfo->lcdc.hsync_skew = 0;
			
			ret = platform_device_register(&this_device);		
			return 0;

		}
		else
		{
		        LcdPanleID=(u32)LCD_PANEL_P726_ILI9325C;  
				pinfo->lcdc.h_back_porch = 20;
				pinfo->lcdc.h_front_porch = 40;
				pinfo->lcdc.h_pulse_width = 10;
				pinfo->lcdc.v_back_porch = 2;
				pinfo->lcdc.v_front_porch = 4;
				pinfo->lcdc.v_pulse_width = 2;
				pinfo->lcdc.border_clr = 0;	/* blk */
				pinfo->lcdc.underflow_clr = 0xffff;	/* blue */
				pinfo->lcdc.hsync_skew = 0;
			
			ret = platform_device_register(&this_device);		
			return 0;
		}
		
	}
	msm_fb_add_device(pdev);

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = lcdc_panel_probe,
	.driver = {
		.name   = "lcdc_panel_qvga",
	},
};



static int __init lcdc_lead_panel_init(void)
{
	int ret;

	ret = platform_driver_register(&this_driver);	

	return ret;
}

module_init(lcdc_lead_panel_init);

