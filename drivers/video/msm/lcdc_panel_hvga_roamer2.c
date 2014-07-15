/*
 * =====================================================================================
 *
 *       Filename:  lcdc_panel_qvga_samsung.c
 *
 *    Description:  Lead QVGA panel(240x320) driver which ic No. is 9325
 *
 *        Version:  1.0
 *        Created:  01/18/2010 02:05:46 PM
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
2010-06-11   lht		project mode display panel info         	ZTE_LCD_LHT_20100611_001
==========================================================================================*/


#include "msm_fb.h"
#include <asm/gpio.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <mach/pmic.h>
#ifdef CONFIG_ZTE_PLATFORM
#include <mach/zte_memlog.h>
#endif

#define GPIO_LCD_BL_SC_OUT 57    //97     //changed by zte_gequn091966,20110406

static int lcdc_samsung_regist = 1;
static boolean is_firsttime = true;
static int spi_cs;
static int spi_sclk;
static int spi_sdi;
static int spi_sdo;
static int lcd_panel_reset;
static int lcd_bkl_ctl;
static int lcd_ic_id;
//static int lcd_panel_id;
extern u32 LcdPanleID;   //ZTE_LCD_LHT_20100611_001
static bool onewiremode = true;  //added by zte_gequn091966,20110428

typedef enum
{
	LCD_PANEL_NONE = 0,
	LCD_PANEL_BOE_HVGA,
	LCD_PANEL_TRULY_HVGA,   //added by zte_gequn091966,20110429
	LCD_PANEL_TRULY_HVGA_VER2,   //added by zte_yichangming10136032,20120116
	LCD_PANEL_TFT_LEAD_HX8357,
	LCD_PANEL_TFT_TRULY_HX8357,
	LCD_PANEL_TFT_YS_HX8357C,
	LCD_PANEL_TFT_BAOLONGDA_HX8357,
}LCD_PANEL_TYPE;

LCD_PANEL_TYPE g_lcd_panel_type = LCD_PANEL_NONE;

//static __u32 last_bl_level = 6;

//static DEFINE_SPINLOCK(tps61061_lock);

static struct msm_panel_common_pdata * lcd_panel_pdata;
static void lcd_panel_sleep(void);
static void lcd_panel_wakeup(void);
static void lcd_panel_init(void);
static void lcdc_set_bl(struct msm_fb_data_type *mfd);

static void spi_init(void);
static int lcdc_panel_on(struct platform_device *pdev);
static int lcdc_panel_off(struct platform_device *pdev);

static boolean bSleepWhenSuspend = false;

void myudelay(unsigned int usec)
{
udelay(usec);
#if 0
	volatile unsigned int i,j;
    for(i=0;i<usec;i++)
	{
		for(j=0;j<100;j++)
		;
	}
#endif
#if 0
	unsigned int i,n;
	i=usec/6+1;
	for(n=0;n<i;n++)
	{
		gpio_direction_output(spi_sdi, 1);  //about 3us
		gpio_direction_output(spi_sdi, 0);
	}
#endif
}

//added by zte_gequn091966,20110428
static void select_1wire_mode(void)
{
	gpio_direction_output(lcd_bkl_ctl, 1);
	myudelay(120);
	gpio_direction_output(lcd_bkl_ctl, 0);
	myudelay(280);				////ZTE_LCD_LUYA_20100226_001
	gpio_direction_output(lcd_bkl_ctl, 1);
	myudelay(650);				////ZTE_LCD_LUYA_20100226_001
	
}
static void send_bkl_address(void)
{
	unsigned int i,j;
	i = 0x72;
	gpio_direction_output(lcd_bkl_ctl, 1);
	myudelay(10);
	//printk("[LY] send_bkl_address \n");
	for(j = 0; j < 8; j++)
	{
		if(i & 0x80)
		{
			gpio_direction_output(lcd_bkl_ctl, 0);
			myudelay(10);
			gpio_direction_output(lcd_bkl_ctl, 1);
			myudelay(180);
		}
		else
		{
			gpio_direction_output(lcd_bkl_ctl, 0);
			myudelay(180);
			gpio_direction_output(lcd_bkl_ctl, 1);
			myudelay(10);
		}
		i <<= 1;
	}
	gpio_direction_output(lcd_bkl_ctl, 0);
	myudelay(10);
	gpio_direction_output(lcd_bkl_ctl, 1);

}

static void send_bkl_data(int level)
{
	unsigned int i,j;
	i = level & 0x1F;
	gpio_direction_output(lcd_bkl_ctl, 1);
	myudelay(10);
	//printk("[LY] send_bkl_data \n");
	for(j = 0; j < 8; j++)
	{
		if(i & 0x80)
		{
			gpio_direction_output(lcd_bkl_ctl, 0);
			myudelay(10);
			gpio_direction_output(lcd_bkl_ctl, 1);
			myudelay(180);
		}
		else
		{
			gpio_direction_output(lcd_bkl_ctl, 0);
			myudelay(180);
			gpio_direction_output(lcd_bkl_ctl, 1);
			myudelay(10);
		}
		i <<= 1;
	}
	gpio_direction_output(lcd_bkl_ctl, 0);
	myudelay(10);
	gpio_direction_output(lcd_bkl_ctl, 1);

}
#if 0
static void ILI9481_WriteReg(unsigned char SPI_COMMD)
{
	unsigned short SBit,SBuffer;
	unsigned char BitCounter;
	
	SBuffer=SPI_COMMD;
	gpio_direction_output(spi_cs, 0);	//Set_CS(0); //CLR CS
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
//***********************************************
//***********************************************
static void ILI9481_WriteData(unsigned char SPI_DATA)
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
#endif
/***added by zte_gequn091966,20110429***/
static void R61581B_WriteReg(unsigned char SPI_COMMD)
{
	unsigned short SBit,SBuffer;
	unsigned char BitCounter;
	
	SBuffer=SPI_COMMD;
	gpio_direction_output(spi_cs, 0);	//Set_CS(0); //CLR CS
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

static void R61581B_WriteData(unsigned char SPI_DATA)
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

static void gpio_lcd_truly_R61581B_emuspi_read_one_index(unsigned char SPI_COMMD, unsigned int *data)
{
     unsigned short SBit,SBuffer;
     unsigned char BitCounter;
     int j;
     volatile int i;
     unsigned int dbit,bits1;

    gpio_direction_output(spi_cs, 0);
	
    SBuffer=SPI_COMMD;  
    
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

       gpio_direction_input(spi_sdi);

  bits1 = 0;

     for (j = 0; j < 8; j++)
   	{
	    gpio_direction_output(spi_sclk, 0); 
	    for(i= 0; i < 33;i++)
		{
				
		}  
	   gpio_direction_output(spi_sclk, 1);
	   gpio_get_value(spi_sdi);
	   for(i= 0; i < 33;i++)
		{
				
		}
	
       }

     for (j = 0; j < 8; j++)
   	{
	    gpio_direction_output(spi_sclk, 0); 
	    for(i= 0; i < 33;i++)
		{
				
		}  
	   gpio_direction_output(spi_sclk, 1);
	   gpio_get_value(spi_sdi);
	   for(i= 0; i < 33;i++)
		{
				
		}
	
       }

     for (j = 0; j < 8; j++)
   	{
	    gpio_direction_output(spi_sclk, 0); 
	    for(i= 0; i < 33;i++)
		{
				
		}  
	   gpio_direction_output(spi_sclk, 1);
	   dbit=gpio_get_value(spi_sdi);
	   for(i= 0; i < 33;i++)
{
				
		}
	
	  bits1 = 2*bits1+dbit;
}
   *data =  bits1;
    gpio_direction_output(spi_cs, 1);  
    printk("read R61581B PID***lkej***:0x%x\n!",bits1);
}
static void gpio_lcd_lead_himax_write_one_index(unsigned short addr)
{
	unsigned int i;
	int j;
	i = addr | 0x7000;
	gpio_direction_output(spi_cs, 0);
       for (j = 0; j < 9; j++) {

		if (i & 0x0100)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	

		gpio_direction_output(spi_sclk, 0);	
		/*udelay(4);*/
		gpio_direction_output(spi_sclk, 1);	
		/*udelay(4);*/
		i <<= 1;
	}
      gpio_direction_output(spi_cs, 1);

}

static void gpio_lcd_lead_himax_write_one_data(unsigned short data)
{
	unsigned int i;
	int j;

	i = data | 0x7100;
	gpio_direction_output(spi_cs, 0);

	for (j = 0; j < 9; j++) {

		if (i & 0x0100)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	

		gpio_direction_output(spi_sclk, 0);	
		/*udelay(4);*/
		gpio_direction_output(spi_sclk, 1);	
		/*udelay(4);*/
		i <<= 1;
	}
  	gpio_direction_output(spi_cs, 1);

}
static void gpio_lcd_lead_himax_read_one_index(unsigned short addr, unsigned int *data1)					
{
			
			int j,i;
			unsigned int dbit,bits1;		
			i = addr;
			
			gpio_direction_output(spi_cs, 0);
			for (j = 0; j < 9; j++) {
		
				if (i & 0x100)
					gpio_direction_output(spi_sdo, 1); 
				else
					gpio_direction_output(spi_sdo, 0); 
		
				gpio_direction_output(spi_sclk, 0); 
				gpio_direction_output(spi_sclk, 1);
				i <<= 1;
			}
			bits1=0;	
			for (j = 0; j < 32; j++) {//read 4 byte
	
				gpio_direction_output(spi_sclk, 0); 
				dbit=gpio_get_value(spi_sdi);
				gpio_direction_output(spi_sclk, 1);
				bits1 = 2*bits1+dbit;
			}
			*data1 =  bits1;
			gpio_direction_output(spi_cs, 1);
			printk("gpio_lcd_read_para PID:0x%x\n!",bits1);
		
}
/***end added by zte_gequn091966,20110429***/
void lcdc_lead_himax_sleep(void)
{
#if 0
   gpio_lcd_lead_himax_write_one_index(0x10);
   mdelay(120);
   gpio_lcd_lead_himax_write_one_index(0x28);
#endif 
   gpio_lcd_lead_himax_write_one_index(0x28);
   mdelay(5);
   gpio_lcd_lead_himax_write_one_index(0x10);
      mdelay(120);
}
static void lcd_panel_sleep(void)
{
	switch(g_lcd_panel_type)
		{
			case LCD_PANEL_TRULY_HVGA:
				R61581B_WriteReg(0x10);
				mdelay(10);
				break;
			case LCD_PANEL_BOE_HVGA:
				R61581B_WriteReg(0x10);
				mdelay(10);
				break;
			case LCD_PANEL_TFT_LEAD_HX8357:
			case LCD_PANEL_TFT_BAOLONGDA_HX8357:
			case LCD_PANEL_TFT_YS_HX8357C:
				lcdc_lead_himax_sleep();
				break;
			default:
				break;
		}	
}

static void lcd_panel_wakeup(void)
{

	switch(g_lcd_panel_type)
		{
			case LCD_PANEL_TRULY_HVGA:
				R61581B_WriteData(0x11);
				mdelay(120);
				break;
			case LCD_PANEL_BOE_HVGA:
				R61581B_WriteData(0x11);
				mdelay(120);
				break;
			default:
				break;
		}
	
}


static void lcdc_truly_R61581B_init(void)
{
	
	R61581B_WriteReg(0xB0);//{setc, [107], W, 0x000B0}
	R61581B_WriteData(0x00);//{setp, [104], W, 0x00000}

	R61581B_WriteReg(0xB3);
	R61581B_WriteData(0x02);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x00);

	R61581B_WriteReg(0xB4);
	R61581B_WriteData(0x11);

	R61581B_WriteReg(0xC0);
	R61581B_WriteData(0x16);
	R61581B_WriteData(0x3B);//480
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x00);//BLV=0,PTL=0
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x01);
	R61581B_WriteData(0x00);//
	R61581B_WriteData(0x77);

	R61581B_WriteReg(0xC1);
	R61581B_WriteData(0x08);// BC=1 //div=0
	R61581B_WriteData(0x19);//CLOCK
	R61581B_WriteData(0x08);
	R61581B_WriteData(0x08);

	R61581B_WriteReg(0xC4);
	R61581B_WriteData(0x11);
	R61581B_WriteData(0x07);
	R61581B_WriteData(0x03);
	R61581B_WriteData(0x03);

	R61581B_WriteReg(0xC6);
	R61581B_WriteData(0x1B);//����ƽ̨����DE��HS��VS��PCLK

	R61581B_WriteReg(0xC8);//GAMMA
	R61581B_WriteData(0x04);
	R61581B_WriteData(0x0C);
	R61581B_WriteData(0x0A);
	R61581B_WriteData(0x59);
	R61581B_WriteData(0x06);
	R61581B_WriteData(0x08);
	R61581B_WriteData(0x0f);
	R61581B_WriteData(0x07);

	R61581B_WriteData(0x00);
	R61581B_WriteData(0x32);

	R61581B_WriteData(0x07);
	R61581B_WriteData(0x0f);
	R61581B_WriteData(0x08);
	R61581B_WriteData(0x56);//43/55
	R61581B_WriteData(0x09);
	R61581B_WriteData(0x0A);
	R61581B_WriteData(0x0C);
	R61581B_WriteData(0x04);

	R61581B_WriteData(0x32);
	R61581B_WriteData(0x00);

	R61581B_WriteReg(0x2A);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x01);
	R61581B_WriteData(0x3F);//320

	R61581B_WriteReg(0x2B);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x01);
	R61581B_WriteData(0xDF);//480

	R61581B_WriteReg(0x35);
	R61581B_WriteData(0x00);

	R61581B_WriteReg(0x36);
	R61581B_WriteData(0x00);

	R61581B_WriteReg(0x3A);
	R61581B_WriteData(0x66);

	R61581B_WriteReg(0x44);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x01);

	R61581B_WriteReg(0x11);
	msleep(150);

	R61581B_WriteReg(0xD0);
	R61581B_WriteData(0x07);
	R61581B_WriteData(0x07);
	R61581B_WriteData(0x1E); //
	R61581B_WriteData(0x33);


	R61581B_WriteReg(0xD1);
	R61581B_WriteData(0x03);
	R61581B_WriteData(0x33);//VCM40 33
	R61581B_WriteData(0x10);//VDV

	R61581B_WriteReg(0xD2);
	R61581B_WriteData(0x03);
	R61581B_WriteData(0x04);//0X24
	R61581B_WriteData(0x04);

	R61581B_WriteReg(0x29);
	msleep(10);  

	//R61581B_WriteReg(0xB4);
	//R61581B_WriteData(0x11);
	R61581B_WriteReg(0x2C);
	msleep(20);  
	printk("lcd module truly 61581 init exit\n!");
}

static void lcdc_new_truly_R61581B_init(void)
{
	
	R61581B_WriteReg(0xB0);//{setc, [107], W, 0x000B0}
	R61581B_WriteData(0x00);//{setp, [104], W, 0x00000}

	R61581B_WriteReg(0xB3);
	R61581B_WriteData(0x02);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x00);

	R61581B_WriteReg(0xB4);
	R61581B_WriteData(0x11);

	R61581B_WriteReg(0xC0);
	R61581B_WriteData(0x16);
	R61581B_WriteData(0x3B);//480
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x00);//BLV=0,PTL=0
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x01);
	R61581B_WriteData(0x00);//
	R61581B_WriteData(0x77);

	R61581B_WriteReg(0xC1);
	R61581B_WriteData(0x08);// BC=1 //div=0
	R61581B_WriteData(0x19);//CLOCK
	R61581B_WriteData(0x08);
	R61581B_WriteData(0x08);

	R61581B_WriteReg(0xC4);
	R61581B_WriteData(0x11);
	R61581B_WriteData(0x07);
	R61581B_WriteData(0x03);
	R61581B_WriteData(0x03);

	R61581B_WriteReg(0xC6);
	R61581B_WriteData(0x1B);//����ƽ̨����DE��HS��VS��PCLK

	R61581B_WriteReg(0xC8);//GAMMA
	R61581B_WriteData(0x04);
	R61581B_WriteData(0x0C);
	R61581B_WriteData(0x0A);
	R61581B_WriteData(0x59);
	R61581B_WriteData(0x06);
	R61581B_WriteData(0x08);
	R61581B_WriteData(0x0f);
	R61581B_WriteData(0x07);

	R61581B_WriteData(0x00);
	R61581B_WriteData(0x32);

	R61581B_WriteData(0x07);
	R61581B_WriteData(0x0f);
	R61581B_WriteData(0x08);
	R61581B_WriteData(0x56);//43/55
	R61581B_WriteData(0x09);
	R61581B_WriteData(0x0A);
	R61581B_WriteData(0x0C);
	R61581B_WriteData(0x04);

	R61581B_WriteData(0x32);
	R61581B_WriteData(0x00);

	R61581B_WriteReg(0x2A);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x01);
	R61581B_WriteData(0x3F);//320

	R61581B_WriteReg(0x2B);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x01);
	R61581B_WriteData(0xDF);//480

	R61581B_WriteReg(0x35);
	R61581B_WriteData(0x00);

	R61581B_WriteReg(0x36);
	R61581B_WriteData(0x00);

	R61581B_WriteReg(0x3A);
	R61581B_WriteData(0x66);

	R61581B_WriteReg(0x44);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x01);

	R61581B_WriteReg(0x11);
	msleep(150);

	R61581B_WriteReg(0xD0);
	R61581B_WriteData(0x07);
	R61581B_WriteData(0x07);
	R61581B_WriteData(0x1E); //
	R61581B_WriteData(0x33);


	R61581B_WriteReg(0xD1);
	R61581B_WriteData(0x03);
	R61581B_WriteData(0x42);//VCM40
	R61581B_WriteData(0x10);//VDV

	R61581B_WriteReg(0xD2);
	R61581B_WriteData(0x03);
	R61581B_WriteData(0x04);//0X24
	R61581B_WriteData(0x04);

	R61581B_WriteReg(0x29);
	msleep(10);  

	//R61581B_WriteReg(0xB4);
	//R61581B_WriteData(0x11);
	R61581B_WriteReg(0x2C);
	msleep(20);  
	printk("lcd module truly 61581 init exit\n!");
}


/***added by zte_gequn091966,20110429***/
static void lcdc_boe_R61581_init(void)
{	
	
	R61581B_WriteReg(0xFF);
	R61581B_WriteReg(0xFF);
	msleep(5);  
	R61581B_WriteReg(0xFF);
	R61581B_WriteReg(0xFF);
	R61581B_WriteReg(0xFF);
	R61581B_WriteReg(0xFF);
	msleep(10); 

	
	R61581B_WriteReg(0xB0);
	R61581B_WriteData(0x00);

	R61581B_WriteReg(0x11);
	msleep(120);
	R61581B_WriteReg(0xB3);
	R61581B_WriteData(0x02);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x10);

	R61581B_WriteReg(0xC0);	//Panel Driving Setting
	R61581B_WriteData(0x16); //13
	R61581B_WriteData(0x3B);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x01);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x43);

	R61581B_WriteReg(0xC1);
	R61581B_WriteData(0x08);
	R61581B_WriteData(0x16);  //0x15
	R61581B_WriteData(0x04);
	R61581B_WriteData(0x04);
	
	R61581B_WriteReg(0xC4);
	R61581B_WriteData(0x15);
	R61581B_WriteData(0x03);
	R61581B_WriteData(0x03);
	R61581B_WriteData(0x01);
	
	R61581B_WriteReg(0xC6);
	R61581B_WriteData(0x33);//0x02


	R61581B_WriteReg(0xC8);
	R61581B_WriteData(0x01);//0c  R0  1        4 
	R61581B_WriteData(0x06);  //R1     9
	R61581B_WriteData(0x0b);//0b  R2  8      
	R61581B_WriteData(0x0c);//48  0b R3 R4  9
	R61581B_WriteData(0x05);//04  08  R5    8 
	R61581B_WriteData(0x0f);  //06    R6 b   
	R61581B_WriteData(0x13);  //15   R7       10
	R61581B_WriteData(0x10);  //R8
	
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x31);
	
	R61581B_WriteData(0x10);   //R8
	R61581B_WriteData(0x10);  //15 R7
	R61581B_WriteData(0x0B);  //06  R6
	R61581B_WriteData(0x62);  //64 R5 R4
	R61581B_WriteData(0x0b); //R3    b
	R61581B_WriteData(0x06); //R2   6
	R61581B_WriteData(0x09);  //R1
	R61581B_WriteData(0x05);//0c R0
	
	R61581B_WriteData(0x31);
	R61581B_WriteData(0x00);

	R61581B_WriteReg(0x35);
	R61581B_WriteData(0x00);//00
	R61581B_WriteReg(0x36);
	R61581B_WriteData(0x00); //set_address_modeZTE_LCD_LKEJ_20110225_001,ZTE_LCD_LKEJ_20110307_001

	R61581B_WriteReg(0x3A);
	R61581B_WriteData(0x66); //18bit
	R61581B_WriteReg(0x44);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x01); //01

	R61581B_WriteReg(0xD0);
	R61581B_WriteData(0x07);
	R61581B_WriteData(0x07);
	R61581B_WriteData(0x14);
	R61581B_WriteData(0xA2);//05

	R61581B_WriteReg(0xD1);
	R61581B_WriteData(0x03);
	R61581B_WriteData(0x29);   //0x39  33
	R61581B_WriteData(0x05);  //0a
	
	R61581B_WriteReg(0xD2);
	R61581B_WriteData(0x03);
	R61581B_WriteData(0x04);
	R61581B_WriteData(0x04);

	R61581B_WriteReg(0x29);
	msleep(10);

	R61581B_WriteReg(0x2A);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x01);
	R61581B_WriteData(0x3F);
	R61581B_WriteReg(0x2B);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x00);
	R61581B_WriteData(0x01);
	R61581B_WriteData(0xDF);

	R61581B_WriteReg(0xB4);
	R61581B_WriteData(0x11);

	msleep(10);

	R61581B_WriteReg(0x2C);

	msleep(100);

	printk("lcd module BOE init exit\n!");
}
/***end added by zte_gequn091966,20110429***/

static void lcdc_baolongda_himax_init(void)
{ 
gpio_lcd_lead_himax_write_one_index(0xB9); //EXTC
gpio_lcd_lead_himax_write_one_data(0xFF); //EXTC
gpio_lcd_lead_himax_write_one_data(0x83); //EXTC
gpio_lcd_lead_himax_write_one_data(0x57); //EXTC
msleep(5);

gpio_lcd_lead_himax_write_one_index(0xB6); //
gpio_lcd_lead_himax_write_one_data(0x55); //6f VCOMDC//56

gpio_lcd_lead_himax_write_one_index(0x11); // SLPOUT
msleep(150);
gpio_lcd_lead_himax_write_one_index(0x35); // TE ON

gpio_lcd_lead_himax_write_one_index(0xB0);
//gpio_lcd_lead_himax_write_one_data(0x68); //80Hz
gpio_lcd_lead_himax_write_one_data(0x60); //80Hz
gpio_lcd_lead_himax_write_one_data(0x01); 

gpio_lcd_lead_himax_write_one_index(0x36);//set address mode
gpio_lcd_lead_himax_write_one_data(0X00);//0x40
gpio_lcd_lead_himax_write_one_index(0xCC); // Set Panel
gpio_lcd_lead_himax_write_one_data(0x05); //

gpio_lcd_lead_himax_write_one_index(0xB1); //
gpio_lcd_lead_himax_write_one_data(0x00); //
gpio_lcd_lead_himax_write_one_data(0x15); //BT
gpio_lcd_lead_himax_write_one_data(0x1E); //1c VSPR
gpio_lcd_lead_himax_write_one_data(0x1E); //1c VSNR
gpio_lcd_lead_himax_write_one_data(0x85); //83 AP  83
gpio_lcd_lead_himax_write_one_data(0x48); //FS

gpio_lcd_lead_himax_write_one_index(0xB4); //
gpio_lcd_lead_himax_write_one_data(0x01); //NW
gpio_lcd_lead_himax_write_one_data(0x40); //RTN
gpio_lcd_lead_himax_write_one_data(0x00); //DIV
gpio_lcd_lead_himax_write_one_data(0x2A); //DUM
gpio_lcd_lead_himax_write_one_data(0x2A); //DUM
gpio_lcd_lead_himax_write_one_data(0x03); //0D GDON
gpio_lcd_lead_himax_write_one_data(0x4c); //GDOFF

gpio_lcd_lead_himax_write_one_index(0xB5); 
gpio_lcd_lead_himax_write_one_data(0x01); 
gpio_lcd_lead_himax_write_one_data(0x01); 
gpio_lcd_lead_himax_write_one_data(0x67); 

gpio_lcd_lead_himax_write_one_index(0xB3); //COLOR FORMAT
gpio_lcd_lead_himax_write_one_data(0xc3); //SDO_EN,BYPASS,EPF[1:0],0,0,RM,DM 43
gpio_lcd_lead_himax_write_one_data(0x0E); //DPL,HSPL,VSPL,EPL
gpio_lcd_lead_himax_write_one_data(0x06); //RCM, HPL[5:0]
gpio_lcd_lead_himax_write_one_data(0x06); //VPL[5:0]

gpio_lcd_lead_himax_write_one_index(0xE3); 
gpio_lcd_lead_himax_write_one_data(0x37); 
gpio_lcd_lead_himax_write_one_data(0x27);

gpio_lcd_lead_himax_write_one_index(0xC0); //STBA
gpio_lcd_lead_himax_write_one_data(0x60); //OPON
gpio_lcd_lead_himax_write_one_data(0x50); //OPON
gpio_lcd_lead_himax_write_one_data(0x01); //
gpio_lcd_lead_himax_write_one_data(0x3C); //
gpio_lcd_lead_himax_write_one_data(0xC8); //
gpio_lcd_lead_himax_write_one_data(0x08); //GEN

gpio_lcd_lead_himax_write_one_index(0xE0); //
gpio_lcd_lead_himax_write_one_data(0x00); //02 1//
gpio_lcd_lead_himax_write_one_data(0x02); //06 2//
gpio_lcd_lead_himax_write_one_data(0x05); //3
gpio_lcd_lead_himax_write_one_data(0x18); //4
gpio_lcd_lead_himax_write_one_data(0x21); //5
gpio_lcd_lead_himax_write_one_data(0x35); //6
gpio_lcd_lead_himax_write_one_data(0x41); //7
gpio_lcd_lead_himax_write_one_data(0x4A); //8
gpio_lcd_lead_himax_write_one_data(0x4E); //9
gpio_lcd_lead_himax_write_one_data(0x47); //10
gpio_lcd_lead_himax_write_one_data(0x40); //11
gpio_lcd_lead_himax_write_one_data(0x3A); //12
gpio_lcd_lead_himax_write_one_data(0x30); //13
gpio_lcd_lead_himax_write_one_data(0x2E); //14
gpio_lcd_lead_himax_write_one_data(0x2C); //15
gpio_lcd_lead_himax_write_one_data(0x07); //16

gpio_lcd_lead_himax_write_one_data(0x00); //02 17 v1
gpio_lcd_lead_himax_write_one_data(0x02); //06 18
gpio_lcd_lead_himax_write_one_data(0x05); //19
gpio_lcd_lead_himax_write_one_data(0x18); //20
gpio_lcd_lead_himax_write_one_data(0x21); //21
gpio_lcd_lead_himax_write_one_data(0x35); //22
gpio_lcd_lead_himax_write_one_data(0x41); //23
gpio_lcd_lead_himax_write_one_data(0x4A); //24
gpio_lcd_lead_himax_write_one_data(0x4E); //25
gpio_lcd_lead_himax_write_one_data(0x47); //26
gpio_lcd_lead_himax_write_one_data(0x40); //27
gpio_lcd_lead_himax_write_one_data(0x3A); //28
gpio_lcd_lead_himax_write_one_data(0x30); //29
gpio_lcd_lead_himax_write_one_data(0x2E); //30
gpio_lcd_lead_himax_write_one_data(0x2C); //28 31
gpio_lcd_lead_himax_write_one_data(0x07); //03 32
gpio_lcd_lead_himax_write_one_data(0x44); //33
gpio_lcd_lead_himax_write_one_data(0x00); //34

gpio_lcd_lead_himax_write_one_index(0x29); // Display On
msleep(25);
/*
gpio_lcd_lead_himax_write_one_index(0xB9); // Set EXTC 
gpio_lcd_lead_himax_write_one_data(0xFF); 
gpio_lcd_lead_himax_write_one_data(0x83); 
gpio_lcd_lead_himax_write_one_data(0x57); 
msleep(50);  

gpio_lcd_lead_himax_write_one_index(0xb6); 
gpio_lcd_lead_himax_write_one_data(0x2C);   
gpio_lcd_lead_himax_write_one_index(0x11); 
msleep(120);  


gpio_lcd_lead_himax_write_one_index(0xB0); 
gpio_lcd_lead_himax_write_one_data(0x66);//68 //70Hz 
gpio_lcd_lead_himax_write_one_data(0x01);  
gpio_lcd_lead_himax_write_one_index(0x3A); //SETPANEL
gpio_lcd_lead_himax_write_one_data(0x66);//02

 

gpio_lcd_lead_himax_write_one_index(0xE9); //SETPANEL 
gpio_lcd_lead_himax_write_one_data(0x01);//02
  
 
gpio_lcd_lead_himax_write_one_index(0x35); //TE	
gpio_lcd_lead_himax_write_one_data(0x00);

gpio_lcd_lead_himax_write_one_index(0xCC); //Set Panel
gpio_lcd_lead_himax_write_one_data(0x05); //BGR_Panel ////


 

gpio_lcd_lead_himax_write_one_index(0xc6); //SETPANEL
gpio_lcd_lead_himax_write_one_data(0x00);//02  
gpio_lcd_lead_himax_write_one_data(0xfb);//02


 

gpio_lcd_lead_himax_write_one_index(0xB3); //COLOR FORMAT
gpio_lcd_lead_himax_write_one_data(0xc3); //SDO_EN,BYPASS,EPF[1:0],0,0,RM,DM ///
  
gpio_lcd_lead_himax_write_one_data(0x0e); //DPL,HSPL,VSPL,EPL///
  
gpio_lcd_lead_himax_write_one_data(0x06); //RCM, HPL[5:0] ///
gpio_lcd_lead_himax_write_one_data(0x06); //VPL[5:0]
	
 
gpio_lcd_lead_himax_write_one_index(0xB1); //
 
gpio_lcd_lead_himax_write_one_data(0x00); //DP
gpio_lcd_lead_himax_write_one_data(0x15); //BT
 
gpio_lcd_lead_himax_write_one_data(0x1c); //VSPR
 
gpio_lcd_lead_himax_write_one_data(0x1c); //VSNR
gpio_lcd_lead_himax_write_one_data(0x83); //AP
gpio_lcd_lead_himax_write_one_data(0x48); //FS

 
gpio_lcd_lead_himax_write_one_index(0xC0); //		    
gpio_lcd_lead_himax_write_one_data(0x24); //OPON
gpio_lcd_lead_himax_write_one_data(0x24); //OPON  
gpio_lcd_lead_himax_write_one_data(0x01); //STBA  
gpio_lcd_lead_himax_write_one_data(0x3C); //STBA  
gpio_lcd_lead_himax_write_one_data(0x1e); //STBA   
gpio_lcd_lead_himax_write_one_data(0x08); //GEN   

gpio_lcd_lead_himax_write_one_index(0xB4); //  
gpio_lcd_lead_himax_write_one_data(0x01); //NW   
gpio_lcd_lead_himax_write_one_data(0x40); //RTN  
gpio_lcd_lead_himax_write_one_data(0x00); //DIV   
gpio_lcd_lead_himax_write_one_data(0x2A); //DUM    
gpio_lcd_lead_himax_write_one_data(0x2A); //DUM   
gpio_lcd_lead_himax_write_one_data(0x0d); //GDON
gpio_lcd_lead_himax_write_one_data(0x3f); //GDOFF	 
  
gpio_lcd_lead_himax_write_one_index(0x36); //Set_address_mode
gpio_lcd_lead_himax_write_one_data(0x00);

gpio_lcd_lead_himax_write_one_index(0xE0); 
gpio_lcd_lead_himax_write_one_data(0x03); 
gpio_lcd_lead_himax_write_one_data(0x0a); 
gpio_lcd_lead_himax_write_one_data(0x13); 
gpio_lcd_lead_himax_write_one_data(0x1b);
gpio_lcd_lead_himax_write_one_data(0x28);  
gpio_lcd_lead_himax_write_one_data(0x32); 
gpio_lcd_lead_himax_write_one_data(0x41); 
gpio_lcd_lead_himax_write_one_data(0x4d); 
gpio_lcd_lead_himax_write_one_data(0x4a); 
gpio_lcd_lead_himax_write_one_data(0x44); 
gpio_lcd_lead_himax_write_one_data(0x3f); 
gpio_lcd_lead_himax_write_one_data(0x31); 
gpio_lcd_lead_himax_write_one_data(0x2e); 
gpio_lcd_lead_himax_write_one_data(0x27); 
gpio_lcd_lead_himax_write_one_data(0x26); 
gpio_lcd_lead_himax_write_one_data(0x1a); 
gpio_lcd_lead_himax_write_one_data(0x03); 
gpio_lcd_lead_himax_write_one_data(0x0a); 
gpio_lcd_lead_himax_write_one_data(0x13); 
gpio_lcd_lead_himax_write_one_data(0x1b);
gpio_lcd_lead_himax_write_one_data(0x28);  
gpio_lcd_lead_himax_write_one_data(0x32); 
gpio_lcd_lead_himax_write_one_data(0x41); 
gpio_lcd_lead_himax_write_one_data(0x4d); 
gpio_lcd_lead_himax_write_one_data(0x4a); 
gpio_lcd_lead_himax_write_one_data(0x44); 
gpio_lcd_lead_himax_write_one_data(0x3f); 
gpio_lcd_lead_himax_write_one_data(0x31); 
gpio_lcd_lead_himax_write_one_data(0x2e); 
gpio_lcd_lead_himax_write_one_data(0x27); 
gpio_lcd_lead_himax_write_one_data(0x26); 
gpio_lcd_lead_himax_write_one_data(0x1a); 
gpio_lcd_lead_himax_write_one_data(0x00); 
gpio_lcd_lead_himax_write_one_data(0x01);   

//gpio_lcd_lead_himax_write_one_index(0x11); //Sleep Out 
//msleep(120); 
gpio_lcd_lead_himax_write_one_index(0x29); //Display On 
*/
	printk("lcd module Baolongda init exit\n!");
}
static void lcdc_lead_himax_init(void)
{ 

gpio_lcd_lead_himax_write_one_index(0xB9); //EXTC
gpio_lcd_lead_himax_write_one_data(0xFF); //EXTC
gpio_lcd_lead_himax_write_one_data(0x83); //EXTC
gpio_lcd_lead_himax_write_one_data(0x57); //EXTC
msleep(5);

gpio_lcd_lead_himax_write_one_index(0xB6); //
gpio_lcd_lead_himax_write_one_data(0x55); //6f VCOMDC//56

gpio_lcd_lead_himax_write_one_index(0x11); // SLPOUT
msleep(150);
gpio_lcd_lead_himax_write_one_index(0x35); // TE ON

gpio_lcd_lead_himax_write_one_index(0xB0);
//gpio_lcd_lead_himax_write_one_data(0x68); //80Hz
gpio_lcd_lead_himax_write_one_data(0x60); //80Hz
gpio_lcd_lead_himax_write_one_data(0x01); 

gpio_lcd_lead_himax_write_one_index(0x36);//set address mode
gpio_lcd_lead_himax_write_one_data(0X00);//0x40
gpio_lcd_lead_himax_write_one_index(0xCC); // Set Panel
gpio_lcd_lead_himax_write_one_data(0x05); //

gpio_lcd_lead_himax_write_one_index(0xB1); //
gpio_lcd_lead_himax_write_one_data(0x00); //
gpio_lcd_lead_himax_write_one_data(0x15); //BT
gpio_lcd_lead_himax_write_one_data(0x1E); //1c VSPR
gpio_lcd_lead_himax_write_one_data(0x1E); //1c VSNR
gpio_lcd_lead_himax_write_one_data(0x85); //83 AP  83
gpio_lcd_lead_himax_write_one_data(0x48); //FS

gpio_lcd_lead_himax_write_one_index(0xB4); //
gpio_lcd_lead_himax_write_one_data(0x01); //NW
gpio_lcd_lead_himax_write_one_data(0x40); //RTN
gpio_lcd_lead_himax_write_one_data(0x00); //DIV
gpio_lcd_lead_himax_write_one_data(0x2A); //DUM
gpio_lcd_lead_himax_write_one_data(0x2A); //DUM
gpio_lcd_lead_himax_write_one_data(0x03); //0D GDON
gpio_lcd_lead_himax_write_one_data(0x4c); //GDOFF

gpio_lcd_lead_himax_write_one_index(0xB5); 
gpio_lcd_lead_himax_write_one_data(0x01); 
gpio_lcd_lead_himax_write_one_data(0x01); 
gpio_lcd_lead_himax_write_one_data(0x67); 

gpio_lcd_lead_himax_write_one_index(0xB3); //COLOR FORMAT
gpio_lcd_lead_himax_write_one_data(0xc3); //SDO_EN,BYPASS,EPF[1:0],0,0,RM,DM 43
gpio_lcd_lead_himax_write_one_data(0x0E); //DPL,HSPL,VSPL,EPL
gpio_lcd_lead_himax_write_one_data(0x06); //RCM, HPL[5:0]
gpio_lcd_lead_himax_write_one_data(0x06); //VPL[5:0]

gpio_lcd_lead_himax_write_one_index(0xE3); 
gpio_lcd_lead_himax_write_one_data(0x37); 
gpio_lcd_lead_himax_write_one_data(0x27);

gpio_lcd_lead_himax_write_one_index(0xC0); //STBA
gpio_lcd_lead_himax_write_one_data(0x60); //OPON
gpio_lcd_lead_himax_write_one_data(0x50); //OPON
gpio_lcd_lead_himax_write_one_data(0x01); //
gpio_lcd_lead_himax_write_one_data(0x3C); //
gpio_lcd_lead_himax_write_one_data(0xC8); //
gpio_lcd_lead_himax_write_one_data(0x08); //GEN

gpio_lcd_lead_himax_write_one_index(0xE0); //
gpio_lcd_lead_himax_write_one_data(0x00); //02 1//
gpio_lcd_lead_himax_write_one_data(0x02); //06 2//
gpio_lcd_lead_himax_write_one_data(0x05); //3
gpio_lcd_lead_himax_write_one_data(0x18); //4
gpio_lcd_lead_himax_write_one_data(0x21); //5
gpio_lcd_lead_himax_write_one_data(0x35); //6
gpio_lcd_lead_himax_write_one_data(0x41); //7
gpio_lcd_lead_himax_write_one_data(0x4A); //8
gpio_lcd_lead_himax_write_one_data(0x4E); //9
gpio_lcd_lead_himax_write_one_data(0x47); //10
gpio_lcd_lead_himax_write_one_data(0x40); //11
gpio_lcd_lead_himax_write_one_data(0x3A); //12
gpio_lcd_lead_himax_write_one_data(0x30); //13
gpio_lcd_lead_himax_write_one_data(0x2E); //14
gpio_lcd_lead_himax_write_one_data(0x2C); //15
gpio_lcd_lead_himax_write_one_data(0x07); //16

gpio_lcd_lead_himax_write_one_data(0x00); //02 17 v1
gpio_lcd_lead_himax_write_one_data(0x02); //06 18
gpio_lcd_lead_himax_write_one_data(0x05); //19
gpio_lcd_lead_himax_write_one_data(0x18); //20
gpio_lcd_lead_himax_write_one_data(0x21); //21
gpio_lcd_lead_himax_write_one_data(0x35); //22
gpio_lcd_lead_himax_write_one_data(0x41); //23
gpio_lcd_lead_himax_write_one_data(0x4A); //24
gpio_lcd_lead_himax_write_one_data(0x4E); //25
gpio_lcd_lead_himax_write_one_data(0x47); //26
gpio_lcd_lead_himax_write_one_data(0x40); //27
gpio_lcd_lead_himax_write_one_data(0x3A); //28
gpio_lcd_lead_himax_write_one_data(0x30); //29
gpio_lcd_lead_himax_write_one_data(0x2E); //30
gpio_lcd_lead_himax_write_one_data(0x2C); //28 31
gpio_lcd_lead_himax_write_one_data(0x07); //03 32
gpio_lcd_lead_himax_write_one_data(0x44); //33
gpio_lcd_lead_himax_write_one_data(0x00); //34

gpio_lcd_lead_himax_write_one_index(0x29); // Display On
msleep(25);
 printk("lcd module lead himax 8357c init exit\n!");
//gpio_lcd_lead_himax_write_one_index(0x2C);


}
static void lcdc_truly_himax8357_init(void)
{
   #if 1 //HX8357C +3.47HSD   HIMAX�ṩ

  gpio_lcd_lead_himax_write_one_index(0xB9);  // Set EXTC 
  gpio_lcd_lead_himax_write_one_data(0xFF); 
  gpio_lcd_lead_himax_write_one_data(0x83); 
  gpio_lcd_lead_himax_write_one_data(0x57); 
  msleep(5); 
  gpio_lcd_lead_himax_write_one_index(0xB6);  // 
  gpio_lcd_lead_himax_write_one_data(0x24);  //VCOMDC 0x2C
 
  gpio_lcd_lead_himax_write_one_index(0x11); 
  msleep(150); 
   
  gpio_lcd_lead_himax_write_one_index(0x3A);  
  gpio_lcd_lead_himax_write_one_data(0x60); //262k 
 
  gpio_lcd_lead_himax_write_one_index(0xCC);  //Set Panel 
  gpio_lcd_lead_himax_write_one_data(0x09);  // 
  
  gpio_lcd_lead_himax_write_one_index(0xB3);  //COLOR FORMAT 
  gpio_lcd_lead_himax_write_one_data(0x43);  //SDO_EN,BYPASS,EPF[1:0],0,0,RM,DM 
  gpio_lcd_lead_himax_write_one_data(0x00);  //DPL,HSPL,VSPL,EPL 
  gpio_lcd_lead_himax_write_one_data(0x06);  //RCM, HPL[5:0] 
  gpio_lcd_lead_himax_write_one_data(0x06);  //VPL[5:0] 
 
  gpio_lcd_lead_himax_write_one_index(0xB1);  // 
  gpio_lcd_lead_himax_write_one_data(0x00);  //DP 
  gpio_lcd_lead_himax_write_one_data(0x15);  //BT 0x15
  gpio_lcd_lead_himax_write_one_data(0x12);  //VSPR 
  gpio_lcd_lead_himax_write_one_data(0x12);  //VSNR 
  gpio_lcd_lead_himax_write_one_data(0x83);  //AP 
  gpio_lcd_lead_himax_write_one_data(0x48);  //FS 0x44
 
  gpio_lcd_lead_himax_write_one_index(0xC0);  // 
  gpio_lcd_lead_himax_write_one_data(0x24);  //OPON 
  gpio_lcd_lead_himax_write_one_data(0x24);  //OPON 
  gpio_lcd_lead_himax_write_one_data(0x01);  //STBA 
  gpio_lcd_lead_himax_write_one_data(0x3C);  //STBA 
  gpio_lcd_lead_himax_write_one_data(0x1E);  //STBA 
  gpio_lcd_lead_himax_write_one_data(0x08);  //GEN 
 
  gpio_lcd_lead_himax_write_one_index(0xB4);  // 
  gpio_lcd_lead_himax_write_one_data(0x01);  //NW 
  gpio_lcd_lead_himax_write_one_data(0x40);  //RTN 
  gpio_lcd_lead_himax_write_one_data(0x00);  //DIV 
  gpio_lcd_lead_himax_write_one_data(0x2A);  //DUM 
  gpio_lcd_lead_himax_write_one_data(0x2A);  //DUM 
  gpio_lcd_lead_himax_write_one_data(0x0D);  //GDON  
  gpio_lcd_lead_himax_write_one_data(0x4F);  //GDOFF 
 
  gpio_lcd_lead_himax_write_one_index(0xE0);  //   
  gpio_lcd_lead_himax_write_one_data(0x02);  //1  
  gpio_lcd_lead_himax_write_one_data(0x08);  //2  
  gpio_lcd_lead_himax_write_one_data(0x11);  //3  
  gpio_lcd_lead_himax_write_one_data(0x23);  //4  
  gpio_lcd_lead_himax_write_one_data(0x2C);  //5  
  gpio_lcd_lead_himax_write_one_data(0x40);  //6  
  gpio_lcd_lead_himax_write_one_data(0x4A);  //7  
  gpio_lcd_lead_himax_write_one_data(0x52);  //8  
  gpio_lcd_lead_himax_write_one_data(0x48);  //9  
  gpio_lcd_lead_himax_write_one_data(0x41);  //10 
  gpio_lcd_lead_himax_write_one_data(0x3C);  //11 
  gpio_lcd_lead_himax_write_one_data(0x33);  //12 
  gpio_lcd_lead_himax_write_one_data(0x2E);  //13 
  gpio_lcd_lead_himax_write_one_data(0x28);  //14 
  gpio_lcd_lead_himax_write_one_data(0x27);  //15 
  gpio_lcd_lead_himax_write_one_data(0x1B);  //16 
  gpio_lcd_lead_himax_write_one_data(0x02);  //17 
  gpio_lcd_lead_himax_write_one_data(0x08);  //18 
  gpio_lcd_lead_himax_write_one_data(0x11);  //19 
  gpio_lcd_lead_himax_write_one_data(0x23);  //20 
  gpio_lcd_lead_himax_write_one_data(0x2C);  //21 
  gpio_lcd_lead_himax_write_one_data(0x40);  //22 
  gpio_lcd_lead_himax_write_one_data(0x4A);  //23 
  gpio_lcd_lead_himax_write_one_data(0x52);  //24 
  gpio_lcd_lead_himax_write_one_data(0x48);  //25 
  gpio_lcd_lead_himax_write_one_data(0x41);  //26 
  gpio_lcd_lead_himax_write_one_data(0x3C);  //27 
  gpio_lcd_lead_himax_write_one_data(0x33);  //28 
  gpio_lcd_lead_himax_write_one_data(0x2E);  //29 
  gpio_lcd_lead_himax_write_one_data(0x28);  //30 
  gpio_lcd_lead_himax_write_one_data(0x27);  //31 
  gpio_lcd_lead_himax_write_one_data(0x1B);  //32 
  gpio_lcd_lead_himax_write_one_data(0x00);  //33 
  gpio_lcd_lead_himax_write_one_data(0x01);  //34 
 
  gpio_lcd_lead_himax_write_one_index(0x29);  //Display on 
  msleep(25); 
  printk("lcd module truly himax 8357 init exit\n!");
  #endif

}
static void lcdc_ys_himax8357c_init(void)
{

   gpio_lcd_lead_himax_write_one_index(0xB9);
   gpio_lcd_lead_himax_write_one_data(0xFF);
   gpio_lcd_lead_himax_write_one_data(0x83);
   gpio_lcd_lead_himax_write_one_data(0x57);
   mdelay(5);
   
   gpio_lcd_lead_himax_write_one_index(0xB6);
   gpio_lcd_lead_himax_write_one_data(0x3F);  //VCDMDC 3c
 

   gpio_lcd_lead_himax_write_one_index(0x11);
   mdelay(150);

  // gpio_lcd_lead_himax_write_one_index(0x3A);
  // gpio_lcd_lead_himax_write_one_data(0x60);   //262K
  
   gpio_lcd_lead_himax_write_one_index(0xCC);
   gpio_lcd_lead_himax_write_one_data(0x09);
   
  gpio_lcd_lead_himax_write_one_index(0xE3);
gpio_lcd_lead_himax_write_one_data(0x10);

   //gpio_lcd_lead_himax_write_one_index(0xB3);
   //gpio_lcd_lead_himax_write_one_data(0x43);
  // gpio_lcd_lead_himax_write_one_data(0x00);
   gpio_lcd_lead_himax_write_one_index(0xB3); //COLOR FORMAT
   gpio_lcd_lead_himax_write_one_data(0xc3); //SDO_EN,BYPASS,EPF[1:0],0,0,RM,DM 43  c3
   gpio_lcd_lead_himax_write_one_data(0x0e); //DPL,HSPL,VSPL,EPL 0e
   gpio_lcd_lead_himax_write_one_data(0x06);
   gpio_lcd_lead_himax_write_one_data(0x06);

gpio_lcd_lead_himax_write_one_index(0x36);//set address mode
gpio_lcd_lead_himax_write_one_data(0X00);//0x40
gpio_lcd_lead_himax_write_one_index(0xCC); // Set Panel
gpio_lcd_lead_himax_write_one_data(0x05); //

   gpio_lcd_lead_himax_write_one_index(0xB1);
   gpio_lcd_lead_himax_write_one_data(0x00);
   gpio_lcd_lead_himax_write_one_data(0x16);//11
   gpio_lcd_lead_himax_write_one_data(0x1c);//18
   gpio_lcd_lead_himax_write_one_data(0x1c);//18
   gpio_lcd_lead_himax_write_one_data(0x83);
   gpio_lcd_lead_himax_write_one_data(0x48);//44

   gpio_lcd_lead_himax_write_one_index(0xC0);
   gpio_lcd_lead_himax_write_one_data(0x76);//24
   gpio_lcd_lead_himax_write_one_data(0x50);//0x24
   gpio_lcd_lead_himax_write_one_data(0x01);
   gpio_lcd_lead_himax_write_one_data(0x3C);
   gpio_lcd_lead_himax_write_one_data(0xc8);//0x1e
   gpio_lcd_lead_himax_write_one_data(0x08);

   gpio_lcd_lead_himax_write_one_index(0xB4);
   gpio_lcd_lead_himax_write_one_data(0x02);//01
   gpio_lcd_lead_himax_write_one_data(0x40);
   gpio_lcd_lead_himax_write_one_data(0x00);
   gpio_lcd_lead_himax_write_one_data(0x2A);
   gpio_lcd_lead_himax_write_one_data(0x2A);
   gpio_lcd_lead_himax_write_one_data(0x0D);
   gpio_lcd_lead_himax_write_one_data(0x4F);

   gpio_lcd_lead_himax_write_one_index(0xE0);
   gpio_lcd_lead_himax_write_one_data(0x02); //0x04
   gpio_lcd_lead_himax_write_one_data(0x23);//0x0a
   gpio_lcd_lead_himax_write_one_data(0x27);//0x13
   gpio_lcd_lead_himax_write_one_data(0x36);//0x24
   gpio_lcd_lead_himax_write_one_data(0x3C);//0x2d
   gpio_lcd_lead_himax_write_one_data(0x4A);//0x41
   gpio_lcd_lead_himax_write_one_data(0x52);//0x4B
   gpio_lcd_lead_himax_write_one_data(0x59);//0x54
   gpio_lcd_lead_himax_write_one_data(0x41);//0x44
   gpio_lcd_lead_himax_write_one_data(0x3A);//0x3D
   gpio_lcd_lead_himax_write_one_data(0x36);//0x38
   gpio_lcd_lead_himax_write_one_data(0x2D);//0x2f
   gpio_lcd_lead_himax_write_one_data(0x29);//0x2c
   gpio_lcd_lead_himax_write_one_data(0x23);//0x25
   gpio_lcd_lead_himax_write_one_data(0x21);//0x24
   gpio_lcd_lead_himax_write_one_data(0x03);//0x1b
   gpio_lcd_lead_himax_write_one_data(0x02);//0x04
   gpio_lcd_lead_himax_write_one_data(0x23);//0x0a
   gpio_lcd_lead_himax_write_one_data(0x27);//0x13
   gpio_lcd_lead_himax_write_one_data(0x36);//0x24
   gpio_lcd_lead_himax_write_one_data(0x3C);//0x2d
   gpio_lcd_lead_himax_write_one_data(0x4A);//0x41
   gpio_lcd_lead_himax_write_one_data(0x52);//0x4b
   gpio_lcd_lead_himax_write_one_data(0x59);//0x54
   gpio_lcd_lead_himax_write_one_data(0x41);//0x44
   gpio_lcd_lead_himax_write_one_data(0x3A);//0x3d
   gpio_lcd_lead_himax_write_one_data(0x36);//0x38
   gpio_lcd_lead_himax_write_one_data(0x2D);//0x2f
   gpio_lcd_lead_himax_write_one_data(0x29);//0x2c
   gpio_lcd_lead_himax_write_one_data(0x23);//0x25
   gpio_lcd_lead_himax_write_one_data(0x21);//0x24
   gpio_lcd_lead_himax_write_one_data(0x03);//0x1b
   gpio_lcd_lead_himax_write_one_data(0x00);
   gpio_lcd_lead_himax_write_one_data(0x01);


   gpio_lcd_lead_himax_write_one_index(0x29);
   mdelay(25);

printk("lcd module ys HX8357c init finish\n!");


   
}
static void lcd_panel_init(void)
{

	gpio_set_value(spi_sclk, 1);
	gpio_set_value(spi_sdo, 1);
	gpio_set_value(spi_cs, 1);
	mdelay(10);
	gpio_direction_output(lcd_panel_reset, 1);
	mdelay(10);
	gpio_direction_output(lcd_panel_reset, 0);
	mdelay(20);
	gpio_direction_output(lcd_panel_reset, 1);
	mdelay(100);

	switch(g_lcd_panel_type)
	{
		case LCD_PANEL_BOE_HVGA:
			lcdc_boe_R61581_init();
			break; 
		case LCD_PANEL_TRULY_HVGA_VER2:
			lcdc_new_truly_R61581B_init();
			break;
		case LCD_PANEL_TRULY_HVGA:
			lcdc_truly_R61581B_init();
			break;
		case LCD_PANEL_TFT_LEAD_HX8357:
			lcdc_lead_himax_init();
			break;
		case LCD_PANEL_TFT_BAOLONGDA_HX8357:
			lcdc_baolongda_himax_init();
			break;
		case LCD_PANEL_TFT_TRULY_HX8357:
			lcdc_truly_himax8357_init();
			break;	
		case LCD_PANEL_TFT_YS_HX8357C:
			lcdc_ys_himax8357c_init();
			break;
		default:
			break;
	}
}

static int lcdc_panel_on(struct platform_device *pdev)
{
	if(!is_firsttime)
	{
		if(bSleepWhenSuspend)
		{
			lcd_panel_wakeup();
		}
		else
		{
			lcd_panel_init();
		}
	}
	else
	{
		is_firsttime = false;
	}

	return 0;
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

	
	spi_sclk = *(lcd_panel_pdata->gpio_num);
	spi_cs   = *(lcd_panel_pdata->gpio_num + 1);
	spi_sdi  = *(lcd_panel_pdata->gpio_num + 2);
	spi_sdo  = *(lcd_panel_pdata->gpio_num + 3);
	lcd_panel_reset = *(lcd_panel_pdata->gpio_num + 4);
	lcd_bkl_ctl =  *(lcd_panel_pdata->gpio_num + 5);
	lcd_ic_id =  *(lcd_panel_pdata->gpio_num + 6);

	//gpio_set_value(spi_sclk, 1);
	//gpio_set_value(spi_sdo, 1);
	//gpio_set_value(spi_cs, 1);
		

}
static int lcdc_panel_off(struct platform_device *pdev)
{
	lcd_panel_sleep();

	if(!bSleepWhenSuspend)
	{
		gpio_direction_output(lcd_panel_reset, 0);
		gpio_direction_output(spi_sclk, 0);
		gpio_direction_output(spi_sdi, 0);
		gpio_direction_output(spi_sdo, 0);
		gpio_direction_output(spi_cs, 0);
	}

	return 0;
}

static struct msm_fb_panel_data lcd_lcdcpanel_panel_data = {
       .panel_info = {.bl_max = 32},
	.on = lcdc_panel_on,
	.off = lcdc_panel_off,
       .set_backlight = lcdc_set_bl,
};

static struct platform_device this_device = {
	.name   = "lcdc_toshiba_fwvga_pt",
	.id	= 1,
	.dev	= {
		.platform_data = &lcd_lcdcpanel_panel_data,
	}
};

static LCD_PANEL_TYPE lcd_panel_detect(void)
{
	
      unsigned int id_h=0;
	  

	  
      R61581B_WriteReg(0xB0);
      R61581B_WriteData(0x00);
      gpio_lcd_truly_R61581B_emuspi_read_one_index(0xBF,&id_h);

	if(id_h != 0x22)
	{
//		gpio_lcd_lead_himax_read_one_index(0xda,&id_h);
//		printk("***ycm***yushun er id is 0x%x\n",id_h);
// 		if (( (id_h >>16) & 0xff) == 0x0f)
//	 	{
//	  
//		    printk("***ycm***yushun er id is 0x%x\n",id_h);
//		    return LCD_PANEL_TFT_YS_HX8357C;

//	 	}

		gpio_lcd_lead_himax_write_one_index(0xB9);
        	gpio_lcd_lead_himax_write_one_data(0xFF); 
		gpio_lcd_lead_himax_write_one_data(0x83); 
		gpio_lcd_lead_himax_write_one_data(0x57); 
		
	 	gpio_lcd_lead_himax_write_one_index(0xB3);
              gpio_lcd_lead_himax_write_one_data(0x83); 
        				
		gpio_lcd_lead_himax_write_one_index(0xFE);
        	gpio_lcd_lead_himax_write_one_data(0xD0); 
			
	 	gpio_lcd_lead_himax_read_one_index(0xFF,&id_h);
		
		id_h = (id_h>>24)&0xFFFF;
	 	printk("himax id is 0x%x\n",id_h);
		
	 	if (id_h == 0x90)
	 	{
	 		if(!gpio_get_value(lcd_ic_id))
	    			return LCD_PANEL_TFT_LEAD_HX8357;
			else
				return LCD_PANEL_TFT_YS_HX8357C;
	 	}
		return LCD_PANEL_TFT_TRULY_HX8357;
		

	}
	else
	{
		gpio_direction_input(lcd_ic_id);
		if(gpio_get_value(lcd_ic_id))
		{
			printk("LCD_PANEL_BOE_HVGA\n");
			return LCD_PANEL_BOE_HVGA;
		}
		else
		{ 
			gpio_lcd_truly_R61581B_emuspi_read_one_index(0xA1,&id_h);
			if(id_h == 0x88){
				printk("LCD_PANEL_TRULY_HVGA_VER2n");
				return LCD_PANEL_TRULY_HVGA_VER2;
			}
			else
			{
			printk("LCD_PANEL_TRULY_HVGA\n");
			return LCD_PANEL_TRULY_HVGA;
			}
		}
	}


}

#ifdef CONFIG_ZTE_PLATFORM
static u32 __init get_lcdpanleid_from_bootloader(void)
{
	smem_global*	msm_lcd_global = (smem_global*) ioremap(SMEM_LOG_GLOBAL_BASE, sizeof(smem_global));
	
	printk("debug chip id 0x%x\n",msm_lcd_global->lcd_id);
	
	if (((msm_lcd_global->lcd_id) & 0xffff0000) == 0x09830000) 
	{
		
		switch(msm_lcd_global->lcd_id & 0x0000ffff)
		{	
			case 0x0001:
				return LCD_PANEL_BOE_HVGA;
			case 0x0002:
				return LCD_PANEL_TRULY_HVGA;
			case 0x0003:
				return LCD_PANEL_TRULY_HVGA_VER2;
			case 0x0004:
				return LCD_PANEL_TFT_LEAD_HX8357;
			case 0x0005:
				return LCD_PANEL_TFT_TRULY_HX8357;
			case 0x0006:
				return LCD_PANEL_TFT_YS_HX8357C;
			case 0x0007:
				return LCD_PANEL_TFT_BAOLONGDA_HX8357;
			default:
				break;
		}		
	}
	return LCD_PANEL_NOPANEL;
}
#endif

static int __devinit lcdc_panel_probe(struct platform_device *pdev)
{
	struct msm_panel_info *pinfo;
	int ret;

	if(pdev->id == 0) {
		lcd_panel_pdata = pdev->dev.platform_data;
		lcd_panel_pdata->panel_config_gpio(1);

	g_lcd_panel_type = LCD_PANEL_NONE;	
	spi_init();	
#ifdef CONFIG_ZTE_PLATFORM	
		if((g_lcd_panel_type = get_lcdpanleid_from_bootloader() )==LCD_PANEL_NONE)
			printk("cann't get get_lcdpanelid from bootloader\n");
#endif
		if(g_lcd_panel_type ==LCD_PANEL_NONE){
			g_lcd_panel_type = lcd_panel_detect();
			printk("debug lcdpanelid is 0x%x\n",g_lcd_panel_type);

			lcd_panel_init();

			}
  
		switch(g_lcd_panel_type)
		{
			case LCD_PANEL_TRULY_HVGA:
				LcdPanleID=LCD_PANEL_3P5_N766_R61581_TRULY;   //ZTE_LCD_LHT_20100611_001
				break;
			case LCD_PANEL_TRULY_HVGA_VER2:
				LcdPanleID=LCD_PANEL_3P5_N766_R61581_TRULY_VER2;   //ZTE_LCD_LHT_20100611_001
				break;
			case LCD_PANEL_BOE_HVGA:
				LcdPanleID=LCD_PANEL_3P5_N766_R61581_BOE;   //ZTE_LCD_LHT_20100611_001
				break;
			case LCD_PANEL_TFT_LEAD_HX8357:
				LcdPanleID=LCD_PANEL_3P5_N766_HX8357C_LEAD;
				break;
			case LCD_PANEL_TFT_YS_HX8357C:
				LcdPanleID=LCD_PANEL_3P5_N766_HX8357C_YUSHUN;
			case LCD_PANEL_TFT_BAOLONGDA_HX8357:
				LcdPanleID=LCD_PANEL_3P5_N766_HX8357C_BAOLONGDA;
			default:
				break;
		}		


		pinfo = &lcd_lcdcpanel_panel_data.panel_info;
		pinfo->xres = 320;
		pinfo->yres = 480;
		pinfo->type = LCDC_PANEL;
		pinfo->pdest = DISPLAY_1;
		pinfo->wait_cycle = 0;
		pinfo->bpp = 18;
		pinfo->fb_num = 2;

		pinfo->clk_rate = 10240000;
		switch(g_lcd_panel_type)
		{
		  	case LCD_PANEL_TRULY_HVGA:
		  	case LCD_PANEL_TRULY_HVGA_VER2:
			   pinfo->lcdc.h_back_porch = 10;
			   pinfo->lcdc.h_front_porch = 20;
			   pinfo->lcdc.h_pulse_width = 10;
			   pinfo->lcdc.v_back_porch = 6;
			   pinfo->lcdc.v_front_porch = 8;
			   pinfo->lcdc.v_pulse_width = 2;
		  	break;
		  
            		case LCD_PANEL_BOE_HVGA:
		  	
			   pinfo->lcdc.h_back_porch = 3;
			   pinfo->lcdc.h_front_porch = 3;
			   pinfo->lcdc.h_pulse_width = 3;
			   pinfo->lcdc.v_back_porch = 2;
			   pinfo->lcdc.v_front_porch = 10;
			   pinfo->lcdc.v_pulse_width = 2;
		  	break;
			case LCD_PANEL_TFT_LEAD_HX8357:
			case LCD_PANEL_TFT_BAOLONGDA_HX8357:
		  	
			   pinfo->lcdc.h_back_porch = 10;
			   pinfo->lcdc.h_front_porch = 10;
			   pinfo->lcdc.h_pulse_width = 10;
			   pinfo->lcdc.v_back_porch = 3;
			   pinfo->lcdc.v_front_porch = 3;
			   pinfo->lcdc.v_pulse_width = 4;
			break;
			
			case LCD_PANEL_TFT_TRULY_HX8357:
		  	
			   pinfo->lcdc.h_back_porch = 10;
			   pinfo->lcdc.h_front_porch = 10;
			   pinfo->lcdc.h_pulse_width = 10;
			   pinfo->lcdc.v_back_porch = 3;
			   pinfo->lcdc.v_front_porch = 3;
			   pinfo->lcdc.v_pulse_width = 4;
			  break;
			 case LCD_PANEL_TFT_YS_HX8357C:
		  	
			   pinfo->lcdc.h_back_porch = 10;
			   pinfo->lcdc.h_front_porch = 10;
			   pinfo->lcdc.h_pulse_width = 10;
			   pinfo->lcdc.v_back_porch = 3;
			   pinfo->lcdc.v_front_porch = 3;
			   pinfo->lcdc.v_pulse_width = 4;
			   
		  	default:
		 	break;
		}
		pinfo->lcdc.border_clr = 0;	/* blk */
		pinfo->lcdc.underflow_clr = 0xff;	/* blue */
		pinfo->lcdc.hsync_skew = 3;

    		ret = platform_device_register(&this_device);
		
		return 0;
	}
	msm_fb_add_device(pdev);
	
	return 0;
}

static struct platform_driver this_driver = {
	.probe  = lcdc_panel_probe,
	.driver = {
		.name   = "lcdc_toshiba_fwvga_pt",
	},
};



static int __init lcd_panel_panel_init(void)
{
	int ret;

	ret = platform_driver_register(&this_driver);

	if(lcdc_samsung_regist == 0)
	{
		printk("[lxw@lcd&fb]:unregist samsung driver!\n");
		platform_driver_unregister(&this_driver);
		return ret;
	}

	return ret;
}

device_initcall(lcd_panel_panel_init);

