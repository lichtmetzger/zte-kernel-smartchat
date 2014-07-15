/* Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_lead.h"
#include <mach/gpio.h>
#ifdef CONFIG_ZTE_PLATFORM
#include <mach/zte_memlog.h>
#endif
static struct dsi_buf lead_tx_buf;
static struct dsi_buf lead_rx_buf;
extern u32 LcdPanleID;
static int lcd_bkl_ctl=97;
#define GPIO_LCD_RESET 89

#define  GPIO_LCD_ID0  48
#define  GPIO_LCD_ID1  90
static bool onewiremode = false;



/*about icchip sleep and display on */
static char display_off[2] = {0x28, 0x00};
static char enter_sleep[2] = {0x10, 0x00};
//static char exit_sleep[2] = {0x11, 0x00};
//static char display_on[2] = {0x29, 0x00};
static struct dsi_cmd_desc display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 50, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(enter_sleep), enter_sleep}
};


/*about himax8363 chip id */
static char hx8357c_lead_para_0xb9[4]= {0xB9, 0xFF, 0x83, 0x57};
static char hx8357c_lead_para_0xcc[2]= {0xcc, 0x05};
static char hx8357c_lead_para_0xb6[2]= {0xb6, 0x50};
static char hx8357c_lead_para_0xb1[7]={0xb1, 0x00, 0x15, 0x18, 0x18, 0x85, 0xaa};  
static char hx8357c_lead_para_0xb3[5]={0xb3, 0x43, 0x00, 0x06, 0x06};
static char hx8357c_lead_para_0xc0[7]={0xc0, 0x24, 0x24, 0x01, 0x3c, 0xc8, 0x08}; 
static char hx8357c_lead_para_0xb4[8]={0xb4, 0x02, 0x40, 0x00, 0x2A, 0x2A, 0x03, 0x4F}; 
static char hx8357c_lead_para_0xb5[4]= {0xb5, 0x01, 0x01, 0x67}; 
static char hx8357c_lead_para_0xe3[3]= {0xe3, 0x37, 0x27}; 
static char hx8357c_lead_para_0xba[17]=
{ 
0xBA, 0x00, 0x56, 0xD4, 
0x00, 0x0A, 0x00, 0x10, 
0x32, 0x6E, 0x04, 0x05, 
0x9A, 0x14, 0x19, 0x10, 
0x40
}; 
static char hx8357c_lead_para_0xe0[68]=
{
0xE0, 0x00, 0x00, 0x00,
0x00, 0x02, 0x00, 0x05,
0x00, 0x18, 0x00, 0x21,
0x00, 0x35, 0x00, 0x41, 
0x00, 0x4A, 0x00, 0x50,
0x00, 0x49, 0x00, 0x44,
0x00, 0x3A, 0x00, 0x37,
0x00, 0x30, 0x00, 0x2E,
0x00, 0x21, 0x01, 0x00, 
0x00, 0x02, 0x00, 0x05,
0x00, 0x18, 0x00, 0x21,
0x00, 0x35, 0x00, 0x41,
0x00, 0x4A, 0x00, 0x50,
0x00, 0x49, 0x00, 0x44,
0x00, 0x3A, 0x00, 0x37, 
0x00, 0x30, 0x00, 0x2E, 
0x00, 0x21, 0x00, 0x00
};
static char hx8357c_lead_para_0x3a[2]={0x3a, 0x70}; 
static char hx8357c_lead_para_0xe9[2]={0xe9, 0x20}; 
static char hx8357c_lead_para_0x11[2]={0x11, 0x00}; 
static char hx8357c_lead_para_0x29[2]={0x29, 0x00}; 
static struct dsi_cmd_desc hx8357c_lead_display_on_cmds[] = 
{
	{DTYPE_DCS_LWRITE, 1, 0, 0, 10, sizeof(hx8357c_lead_para_0xb9), hx8357c_lead_para_0xb9},	
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(hx8357c_lead_para_0xcc), hx8357c_lead_para_0xcc},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(hx8357c_lead_para_0xb6), hx8357c_lead_para_0xb6},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_lead_para_0xb1), hx8357c_lead_para_0xb1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_lead_para_0xb3), hx8357c_lead_para_0xb3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_lead_para_0xc0), hx8357c_lead_para_0xc0},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_lead_para_0xb4), hx8357c_lead_para_0xb4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_lead_para_0xb5), hx8357c_lead_para_0xb5},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_lead_para_0xe3), hx8357c_lead_para_0xe3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_lead_para_0xba), hx8357c_lead_para_0xba},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_lead_para_0xe0), hx8357c_lead_para_0xe0},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(hx8357c_lead_para_0x3a), hx8357c_lead_para_0x3a},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_lead_para_0xe9), hx8357c_lead_para_0xe9},
	{DTYPE_DCS_WRITE, 1, 0, 0, 150, sizeof(hx8357c_lead_para_0x11), hx8357c_lead_para_0x11},
	{DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(hx8357c_lead_para_0x29), hx8357c_lead_para_0x29},
};

/**************************************
truly HX8357C   initial
***************************************/
static char hx8357c_truly_para_0xb9[4]= {0xB9, 0xFF, 0x83, 0x57};
static char hx8357c_truly_para_0xcc[2]= {0xcc, 0x05};
static char hx8357c_truly_para_0xb6[2]= {0xb6, 0x34};	
static char hx8357c_truly_para_0xb1[7]={0xb1, 0x00, 0x11, 0x1c, 0x1c, 0x83, 0xaa};  
static char hx8357c_truly_para_0xb3[5]={0xb3, 0x43, 0x00, 0x06, 0x06}; 
static char hx8357c_truly_para_0xb4[8]={0xb4, 0x02, 0x40, 0x00, 0x2A, 0x2A, 0x0D, 0x4F}; 
static char hx8357c_truly_para_0xc0[7]={0xc0, 0x24, 0x24, 0x01, 0x3c, 0x1e, 0x08}; 
static char hx8357c_truly_para_0xba[17]=
{
0xBA, 0x00, 0x56, 0xD4, 
0x00, 0x0A, 0x00, 0x10, 
0x32, 0x6E, 0x04, 0x05, 
0x9A, 0x14, 0x19, 0x10, 
0x40
}; 

static char hx8357c_truly_para_0xe0[68]=
{
0xE0,0x00,0x00,0x02,
0x00,0x1a,0x00,0x23,
0x00,0x2b,0x00,0x2f,
0x00,0x41,0x00,0x4b,
0x00,0x53,0x00,0x46,
0x00,0x40,0x00,0x3b,
0x00,0x31,0x00,0x2f,
0x00,0x26,0x00,0x26,
0x00,0x03,0x01,0x02,
0x00,0x1a,0x00,0x23,
0x00,0x2b,0x00,0x2f,
0x00,0x41,0x00,0x4b,
0x00,0x53,0x00,0x46,
0x00,0x40,0x00,0x3b,
0x00,0x31,0x00,0x2f,
0x00,0x26,0x00,0x26,
0x00,0x03,0x00,0x44

};
static char hx8357c_truly_para_0x3a[2]={0x3a, 0x70}; 
static char hx8357c_truly_para_0x11[2]={0x11, 0x00}; 
static char hx8357c_truly_para_0x29[2]={0x29, 0x00}; 

static struct dsi_cmd_desc hx8357c_truly_display_on_cmds[] = 
{
	{DTYPE_DCS_LWRITE, 1, 0, 0, 10, sizeof(hx8357c_truly_para_0xb9), hx8357c_truly_para_0xb9},	
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(hx8357c_truly_para_0xcc), hx8357c_truly_para_0xcc},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(hx8357c_truly_para_0xb6), hx8357c_truly_para_0xb6},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_truly_para_0xb1), hx8357c_truly_para_0xb1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_truly_para_0xb3), hx8357c_truly_para_0xb3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_truly_para_0xb4), hx8357c_truly_para_0xb4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_truly_para_0xc0), hx8357c_truly_para_0xc0},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_truly_para_0xba), hx8357c_truly_para_0xba},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_truly_para_0xe0), hx8357c_truly_para_0xe0},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(hx8357c_truly_para_0x3a), hx8357c_truly_para_0x3a},
	{DTYPE_DCS_WRITE, 1, 0, 0, 150, sizeof(hx8357c_truly_para_0x11), hx8357c_truly_para_0x11},
	{DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(hx8357c_truly_para_0x29), hx8357c_truly_para_0x29},
};

/**************************************
yushun HX8357C   initial
***************************************/
static char hx8357c_yushun_para_0xf1[7]= 
{ 
0XF1, 0x36, 0x04, 0x00, 
0x3C, 0x0F, 0x8F
};
static char hx8357c_yushun_para_0xf2[10]= 
{
0XF2, 0x18, 0xA3, 0x12,
0x02, 0xB2, 0x12, 0xFF,
0x10, 0x00
};
static char hx8357c_yushun_para_0xf8[3]= {0XF8, 0x21, 0x04};
static char hx8357c_yushun_para_0xf9_1[2]= {0XF9, 0x00};
static char hx8357c_yushun_para_0xf9_2[3]= {0XF9, 0x08, 0x08};
static char hx8357c_yushun_para_0x36[2]={0x36, 0x48}; 
static char hx8357c_yushun_para_0xf7_1[5]= {0XF7, 0xa9, 0x89, 0x2d,0x8a};
static char hx8357c_yushun_para_0xf7_2[3]= {0XF7, 0xB1, 0x91};
static char hx8357c_yushun_para_0xf7_3[2]= {0XF7, 0xa9};
static char hx8357c_yushun_para_0x3a[2]={0x3A, 0x66}; 
static char hx8357c_yushun_para_0XC2[2]={0XC2, 0x22}; 
static char hx8357c_yushun_para_0XB4[2]={0XB4, 0x02}; 
static char hx8357c_yushun_para_0XC0[3]= {0XC0, 0x01, 0x01};
static char hx8357c_yushun_para_0XC1[2]={0XC1, 0x41}; 
static char hx8357c_yushun_para_0XC5_1[2]= {0XC5, 0x00};
static char hx8357c_yushun_para_0XC5_2[3]= {0XC5, 0x1C, 0x1c};
static char hx8357c_yushun_para_0XB1[3]= {0XB1, 0xB0, 0x11};
static char hx8357c_yushun_para_0xB7[2]={0xB7, 0xC6}; 
static char hx8357c_yushun_para_0xE0[16]= 
{
0xE0, 0x0F, 0x24, 0x1A,
0x08, 0x0F, 0x08, 0x45,
0x24, 0x2A, 0x08, 0x11,
0x03, 0x07, 0x04, 0x00
};
static char hx8357c_yushun_para_0xE1[16]= 
{
0XE1, 0x0F, 0x3B, 0x38,
0x0C, 0x0E, 0x07, 0x47,
0x60, 0x3A, 0x07, 0x10,
0x02, 0x20, 0x1B, 0x00
};
static char hx8357c_yushun_para_0XB6[4]= {0XB6, 0x02, 0x62, 0x3B};
static char hx8357c_yushun_para_0x11[2]={0x11, 0x00}; 
static char hx8357c_yushun_para_0x29[2]={0x29, 0x00}; 

static struct dsi_cmd_desc hx8357c_yushun_display_on_cmds[] = 
{
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0XB1), hx8357c_yushun_para_0XB1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0xf1), hx8357c_yushun_para_0xf1},	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0xf7_1), hx8357c_yushun_para_0xf7_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0xf7_2), hx8357c_yushun_para_0xf7_2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0xf2), hx8357c_yushun_para_0xf2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0xf8), hx8357c_yushun_para_0xf8},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0XC0), hx8357c_yushun_para_0XC0},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0XC1), hx8357c_yushun_para_0XC1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0x3a), hx8357c_yushun_para_0x3a},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0XB4), hx8357c_yushun_para_0XB4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0xf9_2), hx8357c_yushun_para_0xf9_2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0xf7_3), hx8357c_yushun_para_0xf7_3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0x36), hx8357c_yushun_para_0x36},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0xE0), hx8357c_yushun_para_0xE0},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0xE1), hx8357c_yushun_para_0xE1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0xf9_1), hx8357c_yushun_para_0xf9_1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0XC2), hx8357c_yushun_para_0XC2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0XB6), hx8357c_yushun_para_0XB6},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0XC5_2), hx8357c_yushun_para_0XC5_2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0xB7), hx8357c_yushun_para_0xB7},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(hx8357c_yushun_para_0XC5_1), hx8357c_yushun_para_0XC5_1},
	
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(hx8357c_yushun_para_0x11), hx8357c_yushun_para_0x11},
	{DTYPE_DCS_WRITE, 1, 0, 0, 50, sizeof(hx8357c_yushun_para_0x29), hx8357c_yushun_para_0x29},
};
//>2012/5/15-lcd_more_driver_1-lizhiye

/**************************************
 HX8357C+cpt3.5   initial
***************************************/
static char HX8357C_CPT_para_0xB9[4]= 
{
	0xB9, 0xFF, 0x83, 0x57
};
static char HX8357C_CPT_para_0xB6[2]= 
{
	0xB6, 0x50
};

static char HX8357C_CPT_para_0x3A[2]= 
{
	0x3A, 0x77
};
static char HX8357C_CPT_para_0xe9[2]= 
{
	0xe9, 0x20
};
static char HX8357C_CPT_para_0xCC[2]= 
{
	0xCC, 0x05
};

static char HX8357C_CPT_para_0xB3[5]= 
{
	0xb3, 0x43, 0x00, 0x06, 0x06
};

static char HX8357C_CPT_para_0xB1[7]= 
{
	0xB1,0x00,0x14,0x18,0x18,0x85,0xAA
};

static char HX8357C_CPT_para_0xC0[7]= 
{
	0xC0,0x36,0x50,0x01,0x3C,0xC8,0x08
};

static char HX8357C_CPT_para_0xB4[8]= 
{
	0xB4,0x00,0x40,0x00,0x2A,0x2A,0x0D,0x4F
};

static char HX8357C_CPT_para_0xBA[17]= 
{
	0xBA,0x00,0x56,0xD4,0x00,0x0A,0x00,0x10,0x32,0x6E,0x04,0x05,0x9A,0x14,0x19,0x10,0x40
};

static char HX8357C_CPT_para_0xE0[68]= 
{
	0xE0,0x00,0x00,0x00,0x00,0x04,0x00,0x09,
	0x00,0x12,0x00,0x25,0x00,0x3b,0x00,0x49,
	0x00,0x50,0x00,0x48,0x00,0x44,0x00,0x40,
	0x00,0x38,0x00,0x36,0x00,0x31,0x00,0x31,
	0x00,0x2c,0x01,0x00,0x00,0x04,0x00,0x09,
	0x00,0x12,0x00,0x25,0x00,0x3b,0x00,0x49,
	0x00,0x50,0x00,0x48,0x00,0x44,0x00,0x40,
	0x00,0x38,0x00,0x36,0x00,0x31,0x00,0x31,
	0x00,0x2c,0x00,0x00
};


static char HX8357C_CPT_para_0x11[2]= 
{
	0x11, 0x00
};

static char HX8357C_CPT_para_0x29[2]= 
{
	0x29, 0x00
};


static struct dsi_cmd_desc HX8357C_CPT_display_on_cmds[] = 
{
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(HX8357C_CPT_para_0xB9), HX8357C_CPT_para_0xB9},
	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(HX8357C_CPT_para_0xB6), HX8357C_CPT_para_0xB6},
	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(HX8357C_CPT_para_0x3A), HX8357C_CPT_para_0x3A},
	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(HX8357C_CPT_para_0xe9), HX8357C_CPT_para_0xe9},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(HX8357C_CPT_para_0xCC), HX8357C_CPT_para_0xCC},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(HX8357C_CPT_para_0xB3), HX8357C_CPT_para_0xB3},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(HX8357C_CPT_para_0xB1), HX8357C_CPT_para_0xB1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(HX8357C_CPT_para_0xC0), HX8357C_CPT_para_0xC0},
	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(HX8357C_CPT_para_0xB4), HX8357C_CPT_para_0xB4},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(HX8357C_CPT_para_0xBA), HX8357C_CPT_para_0xBA},
	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(HX8357C_CPT_para_0xE0), HX8357C_CPT_para_0xE0},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 150, sizeof(HX8357C_CPT_para_0x11), HX8357C_CPT_para_0x11},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 5, sizeof(HX8357C_CPT_para_0x29), HX8357C_CPT_para_0x29},
	
};

/**************************************
tianma TM035PDZ75_NT35310   initial
***************************************/
	static char NT35310_TM_para_0xED[3]= 
	{
		0xED, 0x01, 0xFE
	};
	static char NT35310_TM_para_0xEE[3]= 
	{
		0xEE, 0xDE, 0x21
	};

	static char NT35310_TM_para_0xDF[2]= 
	{
		0xDF, 0x10
	};
	
	static char NT35310_TM_para_0xC0[5]= 
	{
		0xC0, 0x4c, 0x4c, 0x18, 0x18
	};
	
	
	static char NT35310_TM_para_0xC2[4]= 
	{
		0xC2, 0x44, 0x44, 0x44
	};
	
	static char NT35310_TM_para_0xC4[2]= 
	{
		0xC4, 0x36
	};

	static char NT35310_TM_para_0xC6[5]= 
	{
		0xC6, 0x00, 0xf8, 0xf8, 0xf8
	};
		
	static char NT35310_TM_para_0xB0[2]= 
	{
		0xB0, 0x8c
	};
	
	static char NT35310_TM_para_0xB3[2]= 
	{
		0xB3, 0x21
	};
	
	static char NT35310_TM_para_0xB4[2]= 
	{
		0xB4, 0x15
	};
	
	static char NT35310_TM_para_0xB7[2]= 
	{
		0xB7, 0x00
	};

	static char NT35310_TM_para_0xB8[2]= 
	{
		0xB8, 0x00
	};
	
	static char NT35310_TM_para_0xBF[2]= 
	{
		0xBF, 0xAA
	};
	
	
	static char NT35310_TM_para_0xE0[37]= 
	{
		0xE0, 0x00, 0x00, 0x06,
		0x00, 0x15, 0x00, 0x2f,
		0x00, 0x43, 0x00, 0x52,
		0x00, 0x67, 0x00, 0x7c,
		0x00, 0x8c, 0x00, 0x99,
		0x00, 0xa5, 0x00, 0xb2,
		0x00, 0xba, 0x00, 0xbf,
		0x00, 0xc6, 0x00, 0xcb,
		0x00, 0xd1, 0x00, 0xfe,
		0x00
	};
	
	static char NT35310_TM_para_0xE1[37]= 
	{
		0xE1, 0x00, 0x00, 0x06,
		0x00, 0x15, 0x00, 0x2f,
		0x00, 0x43, 0x00, 0x52,
		0x00, 0x67, 0x00, 0x7c,
		0x00, 0x8c, 0x00, 0x99,
		0x00, 0xa5, 0x00, 0xb2,
		0x00, 0xba, 0x00, 0xbf,
		0x00, 0xc6, 0x00, 0xcb,
		0x00, 0xd1, 0x00, 0xfe,
		0x00

	};
	
	static char NT35310_TM_para_0xE2[37]= 
	{
		0xE2, 0x00, 0x00, 0x06,
		0x00, 0x15, 0x00, 0x2f,
		0x00, 0x43, 0x00, 0x52,
		0x00, 0x67, 0x00, 0x7c,
		0x00, 0x8c, 0x00, 0x99,
		0x00, 0xa5, 0x00, 0xb2,
		0x00, 0xba, 0x00, 0xbf,
		0x00, 0xc6, 0x00, 0xcb,
		0x00, 0xd1, 0x00, 0xfe,
		0x00

	};
	
	static char NT35310_TM_para_0xE3[37]= 
	{
		0xE3, 0x00, 0x00, 0x06,
		0x00, 0x15, 0x00, 0x2f,
		0x00, 0x43, 0x00, 0x52,
		0x00, 0x67, 0x00, 0x7c,
		0x00, 0x8c, 0x00, 0x99,
		0x00, 0xa5, 0x00, 0xb2,
		0x00, 0xba, 0x00, 0xbf,
		0x00, 0xc6, 0x00, 0xcb,
		0x00, 0xd1, 0x00, 0xfe,
		0x00

	};
	
	static char NT35310_TM_para_0xE4[37]= 
	{
		0xE4, 0x00, 0x00, 0x06,
		0x00, 0x15, 0x00, 0x2f,
		0x00, 0x43, 0x00, 0x52,
		0x00, 0x67, 0x00, 0x7c,
		0x00, 0x8c, 0x00, 0x99,
		0x00, 0xa5, 0x00, 0xb2,
		0x00, 0xba, 0x00, 0xbf,
		0x00, 0xc6, 0x00, 0xcb,
		0x00, 0xd1, 0x00, 0xfe,
		0x00

	};
	
	static char NT35310_TM_para_0xE5[37]= 
	{
		0xE5, 0x00, 0x00, 0x06,
		0x00, 0x15, 0x00, 0x2f,
		0x00, 0x43, 0x00, 0x52,
		0x00, 0x67, 0x00, 0x7c,
		0x00, 0x8c, 0x00, 0x99,
		0x00, 0xa5, 0x00, 0xb2,
		0x00, 0xba, 0x00, 0xbf,
		0x00, 0xc6, 0x00, 0xcb,
		0x00, 0xd1, 0x00, 0xfe,
		0x00

	};
	
	static char NT35310_TM_para_0xE6[33]= 
	{
		0xE6, 0x22, 0x00, 0x33,
		0x00, 0x44, 0x00, 0x55,
		0x00, 0x66, 0x00, 0x66,
		0x00, 0x88, 0x00, 0x99,
		0x00, 0xAA, 0x00, 0x88,
		0x00, 0x66, 0x00, 0x55,
		0x00, 0x55, 0x00, 0x33,
		0x00, 0x11, 0x00, 0x11,
		0x00

	};
	
	static char NT35310_TM_para_0xE7[33]= 
	{
		0xE7, 0x22, 0x00, 0x33,
		0x00, 0x44, 0x00, 0x55,
		0x00, 0x66, 0x00, 0x66,
		0x00, 0x88, 0x00, 0x99,
		0x00, 0xAA, 0x00, 0x88,
		0x00, 0x66, 0x00, 0x55,
		0x00, 0x55, 0x00, 0x33,
		0x00, 0x11, 0x00, 0x11,
		0x00

	};
	
	static char NT35310_TM_para_0xE8[33]= 
	{
		0xE8, 0x22, 0x00, 0x33,
		0x00, 0x44, 0x00, 0x55,
		0x00, 0x66, 0x00, 0x66,
		0x00, 0x88, 0x00, 0x99,
		0x00, 0xAA, 0x00, 0x88,
		0x00, 0x66, 0x00, 0x55,
		0x00, 0x55, 0x00, 0x33,
		0x00, 0x11, 0x00, 0x11,
		0x00

	};
	
	
	static char NT35310_TM_para_0x00[2]= 
	{
		0x00, 0xAA
	};
	
	/*static char NT35310_TM_para_0x35[2]= 
	{
		0x35, 0X00
	};
	
	static char NT35310_TM_para_0x51[2]= 
	{
		0x51, 0xFF
	};
	
	
	static char NT35310_TM_para_0x53[2]= 
	{
		0x53, 0x2C
	};
	
	static char NT35310_TM_para_0x55[2]= 
	{
		0x55, 0xb0
	};*/
	

	static char NT35310_TM_para_0x3A[2]= 
	{
		0x3A, 0X77
	};

	static char NT35310_TM_para_0x36[2]= 
	{
		0x36, 0x00 /*0Xd4*/
	};


	
	static char NT35310_TM_para_0x11[2]={0x11, 0x00}; 
	static char NT35310_TM_para_0x29[2]={0x29, 0x00}; 
	static char NT35310_TM_para_0x2c[2]={0x2c, 0x00}; 
	static struct dsi_cmd_desc NT35310_TM_display_on_cmds[] = 
	{
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xED), NT35310_TM_para_0xED},
		
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xEE), NT35310_TM_para_0xEE},
		
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xDF), NT35310_TM_para_0xDF},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xC0), NT35310_TM_para_0xC0},
		
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xC2), NT35310_TM_para_0xC2},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xC4), NT35310_TM_para_0xC4},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xB4), NT35310_TM_para_0xB4},
		
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xC6), NT35310_TM_para_0xC6},
		
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xB0), NT35310_TM_para_0xB0},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xB3), NT35310_TM_para_0xB3},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xB4), NT35310_TM_para_0xB4},
		
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xB7), NT35310_TM_para_0xB7},
		
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xB8), NT35310_TM_para_0xB8},
		
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xBF), NT35310_TM_para_0xBF},
		
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xE0), NT35310_TM_para_0xE0},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xE1), NT35310_TM_para_0xE1},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xE2), NT35310_TM_para_0xE2},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xE3), NT35310_TM_para_0xE3},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xE4), NT35310_TM_para_0xE4},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xE5), NT35310_TM_para_0xE5},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xE6), NT35310_TM_para_0xE6},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xE7), NT35310_TM_para_0xE7},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0xE8), NT35310_TM_para_0xE8},
		
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0x00), NT35310_TM_para_0x00},
		//{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0x35), NT35310_TM_para_0x35},
		
		//{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0x51), NT35310_TM_para_0x51},
		//{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0x53), NT35310_TM_para_0x53},
		//{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0x55), NT35310_TM_para_0x55},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0x3A), NT35310_TM_para_0x3A},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0x36), NT35310_TM_para_0x36},
		
		{DTYPE_DCS_LWRITE, 1, 0, 0, 120, sizeof(NT35310_TM_para_0x11), NT35310_TM_para_0x11},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0x29), NT35310_TM_para_0x29},
		{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(NT35310_TM_para_0x2c), NT35310_TM_para_0x2c},
	};




void myudelay(unsigned int usec)
{
udelay(usec);
}
static void lcd_panle_reset(void)
{
	gpio_direction_output(GPIO_LCD_RESET,1);
	msleep(10);
	gpio_direction_output(GPIO_LCD_RESET,0);
	msleep(10);
	gpio_direction_output(GPIO_LCD_RESET,1);
	msleep(50);
}


#ifdef CONFIG_ZTE_PLATFORM
static u32 __init get_lcdpanleid_from_bootloader(void)
{
	smem_global * msm_lcd_global = (smem_global*) ioremap(SMEM_LOG_GLOBAL_BASE, sizeof(smem_global));
	
	printk("lizhiye lcd chip id 0x%x\n",msm_lcd_global->lcd_id);
	
	if (((msm_lcd_global->lcd_id) & 0xffff0000) == 0x09830000) 
	{
		switch(msm_lcd_global->lcd_id & 0x0000ffff)
		{				
			case 0x0001:
				return (u32)LCD_PANEL_3P5_ILI9486_YUSHUN;
				
			case 0x0002:
				return (u32)LCD_PANEL_3P5_HX8357_TRULY;
				
			case 0x0003:
				return (u32)LCD_PANEL_3P5_HX8357_LEAD;	
			
			case 0x0004:
				return (u32)LCD_PANEL_3P5_HX8357_BOE;

			case 0x0005:
				return (u32)LCD_PANEL_3P5_HX8357_CPT_LEAD;

			case 0x0006:
				return (u32)LCD_PANEL_3P5_NT35310_TM;
						
			default:
				break;
		}		
	}
	return (u32)LCD_PANEL_NOPANEL;
}
//>2012/5/15-lcd_more_driver_1-lizhiye
#endif

static int mipi_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;
	mipi_set_tx_power_mode(1);

	mipi_dsi_cmds_tx(mfd, &lead_tx_buf, display_off_cmds,
			ARRAY_SIZE(display_off_cmds));
	gpio_direction_output(GPIO_LCD_RESET,0);
	msleep(5);
	gpio_direction_output(GPIO_LCD_RESET,1);
	msleep(10);
	gpio_direction_output(121,0);
	return 0;
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
	printk("[LY] send_bkl_address \n");
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
	printk("[LY] send_bkl_data \n");
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
static void mipi_zte_set_backlight(struct msm_fb_data_type *mfd)
{
       /*value range is 1--32*/
	 int current_lel = mfd->bl_level;
  	 unsigned long flags;


    	printk("[ZYF] lcdc_set_bl level=%d, %d\n", 
		   current_lel , mfd->panel_power_on);

    	if(!mfd->panel_power_on)
	{
    		gpio_direction_output(lcd_bkl_ctl, 0);
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
    
    	local_irq_save(flags);
		
   	if(current_lel==0)
    	{
    		gpio_direction_output(lcd_bkl_ctl, 0);
		mdelay(3);
		onewiremode = FALSE;
			
    	}
    	else 
	{
		if(!onewiremode)	
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



static int first_time_panel_on = 1;
static int mipi_lcd_on(struct platform_device *pdev)
{

	struct msm_fb_data_type *mfd = platform_get_drvdata(pdev);

	     printk("gequn mipi_lcd_on\n");

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if(first_time_panel_on){
		first_time_panel_on = 0;
		return 0;
		if(LcdPanleID != (u32)LCD_PANEL_NOPANEL)
			return 0;
		//else
			//LcdPanleID = mipi_get_icpanleid(mfd);
	}

//< 2012/5/15-lcd_more_driver_1-lizhiye
//	LcdPanleID = (u32)LCD_PANEL_3P95_HX8357_BOE_BOE;
//>2012/5/15-lcd_more_driver_1-lizhiye
	
	lcd_panle_reset();
	printk("mipi init start\n");
	mipi_set_tx_power_mode(1);
	printk("LcdPanleID = %d\n",LcdPanleID);
	switch(LcdPanleID)
	{	
	//< 2012/5/15-lcd_more_driver_1-lizhiye- < short commond here >
		case (u32)LCD_PANEL_3P5_ILI9486_YUSHUN:
			mipi_dsi_cmds_tx(mfd, &lead_tx_buf, hx8357c_yushun_display_on_cmds, ARRAY_SIZE(hx8357c_yushun_display_on_cmds));
			printk("yushun init ok !!\n");
			break;
			
		case (u32)LCD_PANEL_3P5_HX8357_TRULY:
			mipi_dsi_cmds_tx(mfd, &lead_tx_buf, hx8357c_truly_display_on_cmds, ARRAY_SIZE(hx8357c_truly_display_on_cmds));
			printk("truly init ok !!\n");
			break;
			
		case (u32)LCD_PANEL_3P5_HX8357_LEAD:
			mipi_dsi_cmds_tx(mfd, &lead_tx_buf, hx8357c_lead_display_on_cmds, ARRAY_SIZE(hx8357c_lead_display_on_cmds));
			printk("lead init ok !!\n");
			break;

		case (u32)LCD_PANEL_3P5_HX8357_BOE:
			printk("jingdongfang init ok !!\n");
			break;

		case (u32)LCD_PANEL_3P5_HX8357_CPT_LEAD:
			mipi_dsi_cmds_tx(mfd, &lead_tx_buf, HX8357C_CPT_display_on_cmds, ARRAY_SIZE(HX8357C_CPT_display_on_cmds));
			printk("lead init ok !!\n");
			break;

		case (u32)LCD_PANEL_3P5_NT35310_TM:
			mipi_dsi_cmds_tx(mfd, &lead_tx_buf, NT35310_TM_display_on_cmds, ARRAY_SIZE(NT35310_TM_display_on_cmds));
			printk("lead init ok !!\n");
			break;


	
		default:
		    mipi_dsi_cmds_tx(mfd, &lead_tx_buf, hx8357c_truly_display_on_cmds,ARRAY_SIZE(hx8357c_truly_display_on_cmds));
			printk("BOE_hx8357C  init ok !!\n");
			break;
	//>2012/5/15-lcd_more_driver_1-lizhiye		
	}	
	mipi_set_tx_power_mode(0);
	return 0;
}



static struct msm_fb_panel_data lead_panel_data = {
	.on		= mipi_lcd_on,
	.off		= mipi_lcd_off,
	.set_backlight = mipi_zte_set_backlight,
};



static int ch_used[3];

int mipi_lead_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

printk(KERN_ERR
		  "%s: gequn mipi_device_register n855!\n", __func__);
	ch_used[channel] = TRUE;

	pdev = platform_device_alloc("mipi_lead", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	lead_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &lead_panel_data,
		sizeof(lead_panel_data));
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}


static int __devinit mipi_lead_lcd_probe(struct platform_device *pdev)
{	
	if (pdev->id == 0) return 0;
	
	mipi_dsi_buf_alloc(&lead_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&lead_rx_buf, DSI_BUF_SIZE);
	
#ifdef CONFIG_ZTE_PLATFORM	
	if((LcdPanleID = get_lcdpanleid_from_bootloader() )==(u32)LCD_PANEL_NOPANEL)
		printk("cann't get get_lcdpanelid from bootloader\n");
#endif	


	msm_fb_add_device(pdev);

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_lead_lcd_probe,
	.driver = {
		.name   = "mipi_lead",
	},
};

static int __init mipi_lcd_init(void)
{
   		printk("gequn mipi_lcd_init\n");

	return platform_driver_register(&this_driver);
}

module_init(mipi_lcd_init);
