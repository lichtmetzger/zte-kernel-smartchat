/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/* ========================================================================================
2010-08-04   yym        add f3 log support                         
==========================================================================================*/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio_event.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/msm_hsusb.h>
#include <mach/rpc_hsusb.h>
#include <mach/rpc_pmapp.h>
#include <mach/usbdiag.h>
#include <mach/usb_gadget_fserial.h>
#include <mach/msm_memtypes.h>
#include <mach/msm_serial_hs.h>

#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <mach/vreg.h>
#include <mach/pmic.h>
#include <mach/socinfo.h>

#include <linux/slab.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <asm/mach/mmc.h>
#include <linux/i2c.h>
#include <linux/i2c/sx150x.h>
#include <linux/gpio.h>

#ifdef CONFIG_USB_G_ANDROID
#include <linux/usb/android.h>
#include <mach/usbdiag.h>
#endif

#include <linux/android_pmem.h>
#include <linux/bootmem.h>
#include <linux/mfd/marimba.h>
#include <mach/vreg.h>
#include <linux/power_supply.h>
#include <mach/rpc_pmapp.h>
#include "proc_comm.h"

#include <mach/msm_battery.h>
#include <linux/smsc911x.h>
#include <linux/atmel_maxtouch.h>
#include "devices.h"
#include "timer.h"
#include "devices-msm7x2xa.h"
#include "pm.h"
#include <mach/rpc_server_handset.h>
#include <mach/socinfo.h>

#include <asm/setup.h>

#ifdef CONFIG_PN544_NFC
#include <linux/nfc/pn544.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_MXT224
#include <linux/atmel_qt602240.h>
extern struct atmel_i2c_platform_data atmel_data;
#endif
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_3K
#include <linux/input/synaptics_i2c_rmi.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_CY8CTST241
#include <linux/input/cyttsp.h>
#endif

#include <mach/camera.h>
#ifdef CONFIG_ZTE_PLATFORM
#include <mach/zte_memlog.h>
#include <linux/proc_fs.h>
#endif

#ifdef CONFIG_SENSOR_LIS302DL
#include <linux/lis302dl.h>
#endif

#define PMEM_KERNEL_EBI1_SIZE	0x3A000
#define MSM_PMEM_AUDIO_SIZE	0x5B000
#define BAHAMA_SLAVE_ID_FM_ADDR         0x2A
#define BAHAMA_SLAVE_ID_QMEMBIST_ADDR   0x7B
#define BAHAMA_SLAVE_ID_FM_REG 0x02
#define FM_GPIO	27


#ifdef CONFIG_ZTE_PLATFORM
#ifdef CONFIG_ANDROID_RAM_CONSOLE
#define MSM_RAM_CONSOLE_PHYS  0xFF00000
#define MSM_RAM_CONSOLE_SIZE  SZ_1M
#endif

static smem_global *global;
#endif

enum {
	GPIO_EXPANDER_IRQ_BASE	= NR_MSM_IRQS + NR_GPIO_IRQS,
	GPIO_EXPANDER_GPIO_BASE	= NR_MSM_GPIOS,
	/* SURF expander */
	GPIO_CORE_EXPANDER_BASE	= GPIO_EXPANDER_GPIO_BASE,
	GPIO_BT_SYS_REST_EN	= GPIO_CORE_EXPANDER_BASE,
	GPIO_WLAN_EXT_POR_N,
	GPIO_DISPLAY_PWR_EN,
	GPIO_BACKLIGHT_EN,
	GPIO_PRESSURE_XCLR,
	GPIO_VREG_S3_EXP,
	GPIO_UBM2M_PWRDWN,
	GPIO_ETM_MODE_CS_N,
	GPIO_HOST_VBUS_EN,
	GPIO_SPI_MOSI,
	GPIO_SPI_MISO,
	GPIO_SPI_CLK,
	GPIO_SPI_CS0_N,
	GPIO_CORE_EXPANDER_IO13,
	GPIO_CORE_EXPANDER_IO14,
	GPIO_CORE_EXPANDER_IO15,
	/* Camera expander */
	GPIO_CAM_EXPANDER_BASE	= GPIO_CORE_EXPANDER_BASE + 16,
	GPIO_CAM_GP_STROBE_READY	= GPIO_CAM_EXPANDER_BASE,
	GPIO_CAM_GP_AFBUSY,
	GPIO_CAM_GP_CAM_PWDN,
	GPIO_CAM_GP_CAM1MP_XCLR,
	GPIO_CAM_GP_CAMIF_RESET_N,
	GPIO_CAM_GP_STROBE_CE,
	GPIO_CAM_GP_LED_EN1,
	GPIO_CAM_GP_LED_EN2,
};

#if defined(CONFIG_GPIO_SX150X)
enum {
	SX150X_CORE,
	SX150X_CAM,
};

static struct sx150x_platform_data sx150x_data[] __initdata = {
	[SX150X_CORE]	= {
		.gpio_base		= GPIO_CORE_EXPANDER_BASE,
		.oscio_is_gpo		= false,
		.io_pullup_ena		= 0,
		.io_pulldn_ena		= 0,
		.io_open_drain_ena	= 0xfef8,
		.irq_summary		= -1,
	},
	[SX150X_CAM]	= {
		.gpio_base		= GPIO_CAM_EXPANDER_BASE,
		.oscio_is_gpo		= false,
		.io_pullup_ena		= 0,
		.io_pulldn_ena		= 0,
		.io_open_drain_ena	= 0x23,
		.irq_summary		= -1,
	},
};
#endif

#ifdef CONFIG_ZTE_PLATFORM
#ifdef CONFIG_ZTE_FTM_FLAG_SUPPORT
static int bcm_power(int on)
{
//	struct vreg *vreg_bt;
	//int pin, rc;
int rc;

	printk("%s\n", __func__);

	if (on) {
/*		for (pin = 0; pin < ARRAY_SIZE(bt_config_power_on); pin++) {
			rc = gpio_tlmm_config(bt_config_power_on[pin],
					      GPIO_ENABLE);
			if (rc) {
				printk(KERN_ERR
				       "%s: gpio_tlmm_config(%#x)=%d\n",
				       __func__, bt_config_power_on[pin], rc);
				return -EIO;
			}
		}
		printk("GPIO_XUM begin ok-xm!\n");
		rc =gpio_request(90, "bt_wake");	  
	  if(!rc)
		   gpio_direction_output(90, 1);
		else
			printk(KERN_ERR "gpio_request: %d failed!\n", 90);	
		gpio_free(90);	
	
*/		printk("%s close bt_config_power_on by xum\n", __func__);			
     gpio_tlmm_config(GPIO_CFG(91, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	   rc =gpio_request(91, "bt_reset");
	   if(!rc){
       gpio_direction_output(91, 0);
		   mdelay(1);
		   gpio_direction_output(91, 1);
		   }
		else
		   {
				printk(KERN_ERR "gpio_request: 91 failed!\n");	
		   }  
		gpio_free(91);
		
		//msleep(20);		
		

#if 0			
	   rc =gpio_request(91, "bt_reset");
	   if(!rc){
		   gpio_direction_output(91, 1);
		   printk(KERN_ERR "gpio_request: 91 to 1 ok-xm!\n");
		   }
		else
			printk(KERN_ERR "gpio_request: 91 failed!\n");		
		gpio_free(91);
				printk("GPIO_XUM end ok-xm!\n");
#endif	
	}else {
	
	/*	for (pin = 0; pin < ARRAY_SIZE(bt_config_power_off); pin++) {
			rc = gpio_tlmm_config(bt_config_power_off[pin],
					      GPIO_ENABLE);
			if (rc) {
				printk(KERN_ERR
				       "%s: gpio_tlmm_config(%#x)=%d\n",
				       __func__, bt_config_power_off[pin], rc);
				return -EIO;
			}
		}*/
  	gpio_tlmm_config(GPIO_CFG(91, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		rc = gpio_request(91, "bt_reset");
		if(!rc)
		   gpio_direction_output(91, 0);
		else
			printk(KERN_ERR "gpio_request: 91 failed!\n");
		gpio_free(91);		
		
	
	}
	return 0;
}
//compatible of qualcomm and broadcomm bluetooth chip    
#define ATAG_ZTEFTM  0x5d53cd73
//compatible of qualcomm and broadcomm bluetooth chip    
static struct platform_device msm_bcm_power_device = {
	.name = "bcm_power",
};
//compatible of qualcomm and broadcomm bluetooth chip   

static int g_zte_ftm_flag_fixup;

static int  parse_tag_zte_ftm(const struct tag *tags)
{
        int flag = 0, find = 0;
        struct tag *t = (struct tag *)tags;

        for (; t->hdr.size; t = tag_next(t)) {
                if (t->hdr.tag == ATAG_ZTEFTM) {
                        printk(KERN_DEBUG "find the zte ftm tag\n");
                        find = 1;
                        break;
                }
        }

        if (find)
                flag = t->u.revision.rev;
        printk(KERN_INFO "[ZYF@FTM]parse_tag_zteftm: zte FTM %s !\n",
	       flag?"enable":"disable");
        return flag;
}


int get_ftm_from_tag(void)
{
	return g_zte_ftm_flag_fixup;
}
EXPORT_SYMBOL(get_ftm_from_tag);

#endif 

static void __init zte_fixup(struct machine_desc *desc, struct tag *tags,
			     char **cmdline, struct meminfo *mi)
{
#ifdef CONFIG_ZTE_FTM_FLAG_SUPPORT
        g_zte_ftm_flag_fixup = parse_tag_zte_ftm((const struct tag *)tags);
#endif
}

#endif  


	/* FM Platform power and shutdown routines */
#define FPGA_MSM_CNTRL_REG2 0x90008010
static void config_pcm_i2s_mode(int mode)
{
	void __iomem *cfg_ptr;
	u8 reg2;

	cfg_ptr = ioremap_nocache(FPGA_MSM_CNTRL_REG2, sizeof(char));

	if (!cfg_ptr)
		return;
	if (mode) {
		/*enable the pcm mode in FPGA*/
		reg2 = readb_relaxed(cfg_ptr);
		if (reg2 == 0) {
			reg2 = 1;
			writeb_relaxed(reg2, cfg_ptr);
		}
	} else {
		/*enable i2s mode in FPGA*/
		reg2 = readb_relaxed(cfg_ptr);
		if (reg2 == 1) {
			reg2 = 0;
			writeb_relaxed(reg2, cfg_ptr);
		}
	}
	iounmap(cfg_ptr);
}

static unsigned fm_i2s_config_power_on[] = {
	/*FM_I2S_SD*/
	GPIO_CFG(68, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	/*FM_I2S_WS*/
	GPIO_CFG(70, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	/*FM_I2S_SCK*/
	GPIO_CFG(71, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
};

static unsigned fm_i2s_config_power_off[] = {
	/*FM_I2S_SD*/
	GPIO_CFG(68, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/*FM_I2S_WS*/
	GPIO_CFG(70, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/*FM_I2S_SCK*/
	GPIO_CFG(71, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
};

static unsigned bt_config_power_on[] = {
	/*RFR*/
	GPIO_CFG(43, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	/*CTS*/
	GPIO_CFG(44, 2, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	/*RX*/
	GPIO_CFG(45, 2, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	/*TX*/
	GPIO_CFG(46, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
#if defined(CONFIG_BCM_BT_SLEEP_GPIO)
	GPIO_CFG(90, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,   GPIO_CFG_2MA),
	GPIO_CFG(83, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN,  GPIO_CFG_2MA),
#endif			
};
static unsigned bt_config_power_off[] = {
	/*RFR*/
	GPIO_CFG(43, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/*CTS*/
	GPIO_CFG(44, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/*RX*/
	GPIO_CFG(45, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/*TX*/
	GPIO_CFG(46, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
#if defined(CONFIG_BCM_BT_SLEEP_GPIO)
	GPIO_CFG(90, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN,   GPIO_CFG_2MA),	
  GPIO_CFG(83, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN,   GPIO_CFG_2MA),
#endif
};
static unsigned bt_config_pcm_on[] = {
	/*PCM_DOUT*/
	GPIO_CFG(68, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	/*PCM_DIN*/
	GPIO_CFG(69, 1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	/*PCM_SYNC*/
	GPIO_CFG(70, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	/*PCM_CLK*/
	GPIO_CFG(71, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
};

static unsigned bt_config_pcm_off[] = {
	/*PCM_DOUT*/
	GPIO_CFG(68, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/*PCM_DIN*/
	GPIO_CFG(69, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/*PCM_SYNC*/
	GPIO_CFG(70, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/*PCM_CLK*/
	GPIO_CFG(71, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
};


static int config_i2s(int mode)
{
	int pin, rc = 0;

	if (mode == FM_I2S_ON) {
		if (1/*machine_is_msm7x27a_surf()*/)
			config_pcm_i2s_mode(0);
		pr_err("%s mode = FM_I2S_ON", __func__);
		for (pin = 0; pin < ARRAY_SIZE(fm_i2s_config_power_on);
			pin++) {
				rc = gpio_tlmm_config(
					fm_i2s_config_power_on[pin],
					GPIO_CFG_ENABLE
					);
				if (rc < 0)
					return rc;
			}
	} else if (mode == FM_I2S_OFF) {
		pr_err("%s mode = FM_I2S_OFF", __func__);
		for (pin = 0; pin < ARRAY_SIZE(fm_i2s_config_power_off);
			pin++) {
				rc = gpio_tlmm_config(
					fm_i2s_config_power_off[pin],
					GPIO_CFG_ENABLE
					);
				if (rc < 0)
					return rc;
			}
	}
	return rc;
}
static int config_pcm(int mode)
{
	int pin, rc = 0;

	if (mode == BT_PCM_ON) {
		if (1/*machine_is_msm7x27a_surf()*/)
			config_pcm_i2s_mode(1);
		pr_err("%s mode =BT_PCM_ON", __func__);
		for (pin = 0; pin < ARRAY_SIZE(bt_config_pcm_on);
			pin++) {
				rc = gpio_tlmm_config(bt_config_pcm_on[pin],
					GPIO_CFG_ENABLE);
				if (rc < 0)
					return rc;
			}
	} else if (mode == BT_PCM_OFF) {
		pr_err("%s mode =BT_PCM_OFF", __func__);
		for (pin = 0; pin < ARRAY_SIZE(bt_config_pcm_off);
			pin++) {
				rc = gpio_tlmm_config(bt_config_pcm_off[pin],
					GPIO_CFG_ENABLE);
				if (rc < 0)
					return rc;
			}

	}

	return rc;
}

static int msm_bahama_setup_pcm_i2s(int mode)
{
	int fm_state = 0, bt_state = 0;
	int rc = 0;
	struct marimba config = { .mod_id =  SLAVE_ID_BAHAMA};

	fm_state = marimba_get_fm_status(&config);
	bt_state = marimba_get_bt_status(&config);

	switch (mode) {
	case BT_PCM_ON:
	case BT_PCM_OFF:
		if (!fm_state)
			rc = config_pcm(mode);
		break;
	case FM_I2S_ON:
		rc = config_i2s(mode);
		break;
	case FM_I2S_OFF:
		if (bt_state)
			rc = config_pcm(BT_PCM_ON);
		else
			rc = config_i2s(mode);
		break;
	default:
		rc = -EIO;
		pr_err("%s:Unsupported mode", __func__);
	}
	return rc;
}

static int bt_set_gpio(int on)
{
	int rc = 0;
	struct marimba config = { .mod_id =  SLAVE_ID_BAHAMA};

	if (on) {
		rc = gpio_direction_output(91, 1);
		msleep(100);
	} else {
		if (!marimba_get_fm_status(&config) &&
				!marimba_get_bt_status(&config)) {
			gpio_set_value_cansleep(91, 0);
			rc = gpio_direction_input(91);
			msleep(100);
		}
	}
	if (rc)
		pr_err("%s: BT sys_reset_en GPIO : Error", __func__);

	return rc;
}

static struct vreg *fm_regulator;
static int fm_radio_setup(struct marimba_fm_platform_data *pdata)
{
	int rc = 0;
	const char *id = "FMPW";
	uint32_t irqcfg;
	struct marimba config = { .mod_id =  SLAVE_ID_BAHAMA};
	u8 value;
	/* Voting for 1.8V Regulator */
	fm_regulator = vreg_get(NULL , "msme1");
	if (IS_ERR(fm_regulator)) {
		pr_err("%s: vreg get failed with : (%ld)\n",
			__func__, PTR_ERR(fm_regulator));
		return -EINVAL;
	}

	/* Set the voltage level to 1.8V */
	rc = vreg_set_level(fm_regulator, 1800);
	if (rc < 0) {
		pr_err("%s: set regulator level failed with :(%d)\n",
			__func__, rc);
		goto fm_vreg_fail;
	}

	/* Enabling the 1.8V regulator */
	rc = vreg_enable(fm_regulator);
	if (rc) {
		pr_err("%s: enable regulator failed with :(%d)\n",
			__func__, rc);
		goto fm_vreg_fail;
	}

	/* Voting for 19.2MHz clock */
	rc = pmapp_clock_vote(id, PMAPP_CLOCK_ID_D1,
			PMAPP_CLOCK_VOTE_ON);
	if (rc < 0) {
		pr_err("%s: clock vote failed with :(%d)\n",
			 __func__, rc);
		goto fm_clock_vote_fail;
	}

	rc = bt_set_gpio(1);
	if (rc) {
		pr_err("%s: bt_set_gpio = %d", __func__, rc);
		goto fm_gpio_config_fail;
	}
	/*re-write FM Slave Id, after reset*/
	value = BAHAMA_SLAVE_ID_FM_ADDR;
	rc = marimba_write_bit_mask(&config,
			BAHAMA_SLAVE_ID_FM_REG, &value, 1, 0xFF);
	if (rc < 0) {
		pr_err("%s: FM Slave ID rewrite Failed = %d", __func__, rc);
		goto fm_gpio_config_fail;
	}

	/* Configuring the FM GPIO */
	irqcfg = GPIO_CFG(FM_GPIO, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL,
			GPIO_CFG_2MA);
		pr_err("songyy fm gpio=27");
	rc = gpio_tlmm_config(irqcfg, GPIO_CFG_ENABLE);
	if (rc) {
		pr_err("%s: gpio_tlmm_config(%#x)=%d\n",
			 __func__, irqcfg, rc);
		goto fm_gpio_config_fail;
	}

	return 0;

fm_gpio_config_fail:
	pmapp_clock_vote(id, PMAPP_CLOCK_ID_D1,
		PMAPP_CLOCK_VOTE_OFF);

	bt_set_gpio(0);

fm_clock_vote_fail:
	vreg_disable(fm_regulator);

fm_vreg_fail:
	vreg_put(fm_regulator);

	return rc;
};

static void fm_radio_shutdown(struct marimba_fm_platform_data *pdata)
{
	int rc;
	const char *id = "FMPW";

	/* Releasing the GPIO line used by FM */
	uint32_t irqcfg = GPIO_CFG(FM_GPIO, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP,
		GPIO_CFG_2MA);
	pr_err("songyy fm gpio=27");
	rc = gpio_tlmm_config(irqcfg, GPIO_CFG_ENABLE);
	if (rc)
		pr_err("%s: gpio_tlmm_config(%#x)=%d\n",
			 __func__, irqcfg, rc);

	/* Releasing the 1.8V Regulator */
	if (fm_regulator != NULL) {
		rc = vreg_disable(fm_regulator);

		if (rc)
			pr_err("%s: disable regulator failed:(%d)\n",
				__func__, rc);
		fm_regulator = NULL;
	}

	/* Voting off the clock */
	rc = pmapp_clock_vote(id, PMAPP_CLOCK_ID_D1,
		PMAPP_CLOCK_VOTE_OFF);

	if (rc < 0)
		pr_err("%s: voting off failed with :(%d)\n",
			__func__, rc);

	rc = bt_set_gpio(0);
	if (rc)
		pr_err("%s: bt_set_gpio = %d", __func__, rc);

}

static struct marimba_fm_platform_data marimba_fm_pdata = {
	.fm_setup = fm_radio_setup,
	.fm_shutdown = fm_radio_shutdown,
	.irq = MSM_GPIO_TO_INT(FM_GPIO),
	.vreg_s2 = NULL,
	.vreg_xo_out = NULL,
	/* Configuring the FM SoC as I2S Master */
	.is_fm_soc_i2s_master = true,
	.config_i2s_gpio = msm_bahama_setup_pcm_i2s,
};

#ifdef CONFIG_LEDS_MSM_PMIC//added by zhang.yu_1 for enabling CONFIG_LEDS_MSM_PMIC @110823
static struct platform_device msm_device_pmic_leds = {
	.name   = "pmic-leds",
	.id = -1,
};
#endif

#if defined(CONFIG_BT) && defined(CONFIG_MARIMBA_CORE)

static struct platform_device msm_bt_power_device = {
	.name = "bt_power",
};
	struct bahama_config_register {
		u8 reg;
		u8 value;
		u8 mask;
};

struct bt_vreg_info {
	const char *name;
	unsigned int pmapp_id;
	unsigned int level;
	unsigned int is_pin_controlled;
	struct vreg *vregs;
};
static struct bt_vreg_info bt_vregs[] = {
	{"msme1", 2, 1800, 0, NULL},
	{"bt", 21, 2900, 1, NULL}
};

static int bahama_bt(int on)
{

	int rc = 0;
	int i;

	struct marimba config = { .mod_id =  SLAVE_ID_BAHAMA};

	struct bahama_variant_register {
		const size_t size;
		const struct bahama_config_register *set;
	};

	const struct bahama_config_register *p;

	u8 version;

	const struct bahama_config_register v10_bt_on[] = {
		{ 0xE9, 0x00, 0xFF },
		{ 0xF4, 0x80, 0xFF },
		{ 0xE4, 0x00, 0xFF },
		{ 0xE5, 0x00, 0x0F },
#ifdef CONFIG_WLAN
		{ 0xE6, 0x38, 0x7F },
		{ 0xE7, 0x06, 0xFF },
#endif
		{ 0xE9, 0x21, 0xFF },
		{ 0x01, 0x0C, 0x1F },
		{ 0x01, 0x08, 0x1F },
	};

	const struct bahama_config_register v20_bt_on_fm_off[] = {
		{ 0x11, 0x0C, 0xFF },
		{ 0x13, 0x01, 0xFF },
		{ 0xF4, 0x80, 0xFF },
		{ 0xF0, 0x00, 0xFF },
		{ 0xE9, 0x00, 0xFF },
#ifdef CONFIG_WLAN
		{ 0x81, 0x00, 0x7F },
		{ 0x82, 0x00, 0xFF },
		{ 0xE6, 0x38, 0x7F },
		{ 0xE7, 0x06, 0xFF },
#endif
		{ 0x8E, 0x15, 0xFF },
		{ 0x8F, 0x15, 0xFF },
		{ 0x90, 0x15, 0xFF },

		{ 0xE9, 0x21, 0xFF },
	};

	const struct bahama_config_register v20_bt_on_fm_on[] = {
		{ 0x11, 0x0C, 0xFF },
		{ 0x13, 0x01, 0xFF },
		{ 0xF4, 0x86, 0xFF },
		{ 0xF0, 0x06, 0xFF },
		{ 0xE9, 0x00, 0xFF },
#ifdef CONFIG_WLAN
		{ 0x81, 0x00, 0x7F },
		{ 0x82, 0x00, 0xFF },
		{ 0xE6, 0x38, 0x7F },
		{ 0xE7, 0x06, 0xFF },
#endif
		{ 0xE9, 0x21, 0xFF },
	};

	const struct bahama_config_register v10_bt_off[] = {
		{ 0xE9, 0x00, 0xFF },
	};

	const struct bahama_config_register v20_bt_off_fm_off[] = {
		{ 0xF4, 0x84, 0xFF },
		{ 0xF0, 0x04, 0xFF },
		{ 0xE9, 0x00, 0xFF }
	};

	const struct bahama_config_register v20_bt_off_fm_on[] = {
		{ 0xF4, 0x86, 0xFF },
		{ 0xF0, 0x06, 0xFF },
		{ 0xE9, 0x00, 0xFF }
	};
	const struct bahama_variant_register bt_bahama[2][3] = {
	{
		{ ARRAY_SIZE(v10_bt_off), v10_bt_off },
		{ ARRAY_SIZE(v20_bt_off_fm_off), v20_bt_off_fm_off },
		{ ARRAY_SIZE(v20_bt_off_fm_on), v20_bt_off_fm_on }
	},
	{
		{ ARRAY_SIZE(v10_bt_on), v10_bt_on },
		{ ARRAY_SIZE(v20_bt_on_fm_off), v20_bt_on_fm_off },
		{ ARRAY_SIZE(v20_bt_on_fm_on), v20_bt_on_fm_on }
	}
	};

	u8 offset = 0; /* index into bahama configs */
	on = on ? 1 : 0;
	version = marimba_read_bahama_ver(&config);


	if ((int)version < 0 || version == BAHAMA_VER_UNSUPPORTED) {
		dev_err(&msm_bt_power_device.dev, "%s: Bahama \
				version read Error, version = %d \n",
				__func__, version);
		return -EIO;
	}
	

	if (version == BAHAMA_VER_2_0) {
		if (marimba_get_fm_status(&config))
			offset = 0x01;
	}

	p = bt_bahama[on][version + offset].set;

	dev_info(&msm_bt_power_device.dev,
		"%s: found version %d\n", __func__, version);

	for (i = 0; i < bt_bahama[on][version + offset].size; i++) {
		u8 value = (p+i)->value;
		rc = marimba_write_bit_mask(&config,
			(p+i)->reg,
			&value,
			sizeof((p+i)->value),
			(p+i)->mask);
		if (rc < 0) {
			dev_err(&msm_bt_power_device.dev,
				"%s: reg %x write failed: %d\n",
				__func__, (p+i)->reg, rc);
			return rc;
		}
		dev_info(&msm_bt_power_device.dev,
			"%s: reg 0x%02x write value 0x%02x mask 0x%02x\n",
				__func__, (p+i)->reg,
				value, (p+i)->mask);
		value = 0;
		rc = marimba_read_bit_mask(&config,
				(p+i)->reg, &value,
				sizeof((p+i)->value), (p+i)->mask);
		if (rc < 0)
			dev_err(&msm_bt_power_device.dev, "%s marimba_read_bit_mask- error",
					__func__);
		dev_info(&msm_bt_power_device.dev,
			"%s: reg 0x%02x read value 0x%02x mask 0x%02x\n",
				__func__, (p+i)->reg,
				value, (p+i)->mask);
	}
	/* Update BT Status */
	if (on)
		marimba_set_bt_status(&config, true);
	else
		marimba_set_bt_status(&config, false);
	return rc;
}
static int bluetooth_switch_regulators(int on)
{
	int i, rc = 0;

	const char *id = "BTPW";

	for (i = 0; i < ARRAY_SIZE(bt_vregs); i++) {
		if (!bt_vregs[i].vregs) {
			pr_err("%s: vreg_get %s failed(%d)\n",
			__func__, bt_vregs[i].name, rc);
			goto vreg_fail;
		}
		rc = on ? vreg_set_level(bt_vregs[i].vregs,
				bt_vregs[i].level) : 0;

		if (rc < 0) {
			pr_err("%s: vreg set level failed (%d)\n",
					__func__, rc);
			goto vreg_set_level_fail;
		}

		if (bt_vregs[i].is_pin_controlled == 1) {
			rc = pmapp_vreg_pincntrl_vote(id,
					bt_vregs[i].pmapp_id,
					PMAPP_CLOCK_ID_D1,
					on ? PMAPP_CLOCK_VOTE_ON :
					PMAPP_CLOCK_VOTE_OFF);
		} else {
		rc = on ? vreg_enable(bt_vregs[i].vregs) :
			  vreg_disable(bt_vregs[i].vregs);
		}



		if (rc < 0) {
			pr_err("%s: vreg %s %s failed(%d)\n",
					__func__, bt_vregs[i].name,
					on ? "enable" : "disable", rc);
			goto vreg_fail;
		}

	}
	return rc;

vreg_fail:

	while (i) {
		if (on)
			vreg_disable(bt_vregs[--i].vregs);
		}

vreg_set_level_fail:

	vreg_put(bt_vregs[0].vregs);
	vreg_put(bt_vregs[1].vregs);

	return rc;
}

static unsigned int msm_bahama_setup_power(void)
{
	int rc = 0;
	struct vreg *vreg_s3 = NULL;

	vreg_s3 = vreg_get(NULL, "msme1");
	if (IS_ERR(vreg_s3)) {
		pr_err("%s: vreg get failed (%ld)\n",
			__func__, PTR_ERR(vreg_s3));
		return PTR_ERR(vreg_s3);
	}
	rc = vreg_set_level(vreg_s3, 1800);
	if (rc < 0) {
		pr_err("%s: vreg set level failed (%d)\n",
				__func__, rc);
		goto vreg_fail;
	}
	rc = vreg_enable(vreg_s3);
	if (rc < 0) {
		pr_err("%s: vreg enable failed (%d)\n",
		       __func__, rc);
		goto vreg_fail;
	}

	/*setup Bahama_sys_reset_n*/
#ifndef CONFIG_BCM_BT_SLEEP_GPIO
	rc = gpio_request(91, "bahama sys_rst_n");
#else
    rc = gpio_request(GPIO_BT_SYS_REST_EN, "bahama sys_rst_n");
#endif
	if (rc < 0) {
		pr_err("%s: gpio_request %d = %d\n", __func__,
			GPIO_BT_SYS_REST_EN, rc);
		goto vreg_fail;
	}
#ifndef CONFIG_BCM_BT_SLEEP_GPIO
	rc = bt_set_gpio(1);
	if (rc < 0) {
		pr_err("%s: bt_set_gpio %d = %d\n", __func__,
			GPIO_BT_SYS_REST_EN, rc);
		goto gpio_fail;
	}
#else
   	rc = gpio_direction_output(GPIO_BT_SYS_REST_EN, 1);
		if (rc < 0) {
		pr_err("%s: gpio_direction_output %d = %d\n", __func__,
			GPIO_BT_SYS_REST_EN, rc);
		goto gpio_fail;
	}
#endif
	return rc;

gpio_fail:
#ifndef CONFIG_BCM_BT_SLEEP_GPIO
	gpio_free(91);
#else
	gpio_free(GPIO_BT_SYS_REST_EN);
#endif
vreg_fail:
	vreg_put(vreg_s3);
	return rc;
}

static unsigned int msm_bahama_shutdown_power(int value)
{
	int rc = 0;
	struct vreg *vreg_s3 = NULL;

	vreg_s3 = vreg_get(NULL, "msme1");
	if (IS_ERR(vreg_s3)) {
		pr_err("%s: vreg get failed (%ld)\n",
			__func__, PTR_ERR(vreg_s3));
		return PTR_ERR(vreg_s3);
	}
	rc = vreg_disable(vreg_s3);
	if (rc) {
		pr_err("%s: vreg disable failed (%d)\n",
		       __func__, rc);
		vreg_put(vreg_s3);
		return rc;
	}

	if (value == BAHAMA_ID) {
		rc = bt_set_gpio(0);
		if (rc) {
			pr_err("%s: bt_set_gpio = %d\n",
					__func__, rc);
		}
	}

	return rc;
}


static unsigned int msm_bahama_core_config(int type)
{
	int rc = 0;

	if (type == BAHAMA_ID) {
		int i;
		struct marimba config = { .mod_id = SLAVE_ID_BAHAMA };
		const struct bahama_config_register v20_init[] = {
			/* reg, value, mask */
			{ 0xF4, 0x84, 0xFF }, /* AREG */
			{ 0xF0, 0x04, 0xFF } /* DREG */
		};
		if (marimba_read_bahama_ver(&config) == BAHAMA_VER_2_0) {
			for (i = 0; i < ARRAY_SIZE(v20_init); i++) {
				u8 value = v20_init[i].value;
				rc = marimba_write_bit_mask(&config,
					v20_init[i].reg,
					&value,
					sizeof(v20_init[i].value),
					v20_init[i].mask);
				if (rc < 0) {
					pr_err("%s: reg %d write failed: %d\n",
						__func__, v20_init[i].reg, rc);
					return rc;
				}
				pr_debug("%s: reg 0x%02x value 0x%02x"
					" mask 0x%02x\n",
					__func__, v20_init[i].reg,
					v20_init[i].value, v20_init[i].mask);
			}
		}
	}

	rc = bt_set_gpio(0);
	if (rc) {
		pr_err("%s: bt_set_gpio = %d\n",
		       __func__, rc);
	}

	pr_debug("core type: %d\n", type);
	return rc;
}

static int bluetooth_power(int on)
{
	int pin, rc = 0;
	const char *id = "BTPW";


	if (on) {
	

		/*setup power for BT SOC*/

		rc = bt_set_gpio(on);
		if (rc) {
			pr_err("%s: bt_set_gpio = %d\n",
					__func__, rc);
			goto exit;
		}

		rc = bluetooth_switch_regulators(on);
		if (rc < 0) {
			pr_err("%s: bluetooth_switch_regulators rc = %d",
					__func__, rc);
			goto exit;
		}
		/*setup BT GPIO lines*/
		for (pin = 0; pin < ARRAY_SIZE(bt_config_power_on);
			pin++) {
			rc = gpio_tlmm_config(bt_config_power_on[pin],
					GPIO_CFG_ENABLE);
			if (rc < 0) {
				pr_err("%s: gpio_tlmm_config(%#x)=%d\n",
						__func__,
						bt_config_power_on[pin],
						rc);
				goto fail_power;
			}
		}
		/*Setup BT clocks*/
		rc = pmapp_clock_vote(id, PMAPP_CLOCK_ID_D1,
			PMAPP_CLOCK_VOTE_ON);
		if (rc < 0) {
			pr_err("Failed to vote for TCXO_D1 ON\n");
			goto fail_clock;
		}
		msleep(20);

		/*I2C config for Bahama*/
		rc = bahama_bt(1);
		if (rc < 0) {
			pr_err("%s: bahama_bt rc = %d", __func__, rc);
			goto fail_i2c;
		}
		msleep(20);

		/*setup BT PCM lines*/
		rc = msm_bahama_setup_pcm_i2s(BT_PCM_ON);
		pr_err("bluetooth_power setup_pcm_i2s(BT_PCM_ON)");
		if (rc < 0) {
			pr_err("%s: msm_bahama_setup_pcm_i2s , rc =%d\n",
				__func__, rc);
				goto fail_power;
			}
		rc = pmapp_clock_vote(id, PMAPP_CLOCK_ID_D1,
				  PMAPP_CLOCK_VOTE_PIN_CTRL);
		if (rc < 0)
			pr_err("%s:Pin Control Failed, rc = %d",
					__func__, rc);

	} else {
		rc = bahama_bt(0);
		if (rc < 0)
			pr_err("%s: bahama_bt rc = %d", __func__, rc);
fail_i2c:
		rc = pmapp_clock_vote(id, PMAPP_CLOCK_ID_D1,
				  PMAPP_CLOCK_VOTE_OFF);
		if (rc < 0)
			pr_err("%s: Failed to vote Off D1\n", __func__);
fail_clock:
		for (pin = 0; pin < ARRAY_SIZE(bt_config_power_off);
			pin++) {
				rc = gpio_tlmm_config(bt_config_power_off[pin],
					GPIO_CFG_ENABLE);
				if (rc < 0) {
					pr_err("%s: gpio_tlmm_config(%#x)=%d\n",
					__func__, bt_config_power_off[pin], rc);
				}
			}
		rc = msm_bahama_setup_pcm_i2s(BT_PCM_OFF);
		if (rc < 0) {
			pr_err("%s: msm_bahama_setup_pcm_i2s, rc =%d\n",
					__func__, rc);
				}
fail_power:
		rc = bluetooth_switch_regulators(0);
		if (rc < 0) {
			pr_err("%s: switch_regulators : rc = %d",\
					__func__, rc);
			goto exit;
		}
	}
	return rc;
exit:
	pr_err("%s: failed with rc = %d", __func__, rc);
	return rc;
}

static int __init bt_power_init(void)
{
	int i, rc = 0;

	for (i = 0; i < ARRAY_SIZE(bt_vregs); i++) {
			bt_vregs[i].vregs = vreg_get(NULL,
					bt_vregs[i].name);
			if (IS_ERR(bt_vregs[i].vregs)) {
				pr_err("%s: vreg get %s failed (%ld)\n",
				       __func__, bt_vregs[i].name,
				       PTR_ERR(bt_vregs[i].vregs));
				rc = PTR_ERR(bt_vregs[i].vregs);
				goto vreg_get_fail;
			}
		}

	msm_bt_power_device.dev.platform_data = &bluetooth_power;
    
	msm_bcm_power_device.dev.platform_data = &bcm_power;  
	return rc;

vreg_get_fail:

	while (i)
		vreg_put(bt_vregs[--i].vregs);

	return rc;
}

static struct marimba_platform_data marimba_pdata = {
	.slave_id[SLAVE_ID_BAHAMA_FM]        = BAHAMA_SLAVE_ID_FM_ADDR,
	.slave_id[SLAVE_ID_BAHAMA_QMEMBIST]  = BAHAMA_SLAVE_ID_QMEMBIST_ADDR,
	.bahama_setup                        = msm_bahama_setup_power,
	.bahama_shutdown                     = msm_bahama_shutdown_power,
	.bahama_core_config                  = msm_bahama_core_config,
	.fm				     = &marimba_fm_pdata,
};

#endif

#ifdef CONFIG_PN544_NFC
struct pn544_i2c_platform_data pn544_data={
       .irq_gpio=41,
       .ven_gpio=26,
       .firm_gpio=40,
       .dcdc_gpio=0,
       .clock_gpio=0,
       .int_active_low=0,
};
#endif
#if defined(CONFIG_I2C) && defined(CONFIG_GPIO_SX150X)
static struct i2c_board_info core_exp_i2c_info[] __initdata = {
/* 0x3e address conflict with focaltech touchsreen driver
	{
		I2C_BOARD_INFO("sx1509q", 0x3e),
	},
*/	
	{
		.type         = "taos",
		.addr         = 0x39,
	},
};

#if 0
static struct lis302dl_platform_data gsensor = {
	.gpio_intr1 =  28,
	.gpio_intr2 =  29,
	.scale      =  2 ,
	.int_active_low = 1,
};
#endif

static struct i2c_board_info cam_exp_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO("sx1508q", 0x22),
		.platform_data	= &sx150x_data[SX150X_CAM],
	},
	
#ifdef CONFIG_SENSORS_AKM8962
	{
		I2C_BOARD_INFO("akm8962", 0xc),
	},
#endif
#ifdef CONFIG_SENSOR_LIS302DL
	{
		//.type = "lis302dl",
		//.addr = 0x1c,
		//.platform_data = &gsensor,
		I2C_BOARD_INFO("lis302dl", 0x1c),
	},
#endif
#ifdef CONFIG_SENSOR_ADXL34X
	{
		I2C_BOARD_INFO("adxl34x", 0x53),
	},
#endif
#ifdef CONFIG_PN544_NFC 
        { 
                I2C_BOARD_INFO("pn544", 0x50>>1 ), 
		   .platform_data = &pn544_data,			
                .irq = MSM_GPIO_TO_INT(41),   
        }, 
#endif 
};
#endif

#if defined(CONFIG_BT) && defined(CONFIG_MARIMBA_CORE)
static struct i2c_board_info bahama_devices[] = {
{
	I2C_BOARD_INFO("marimba", 0xc),
	.platform_data = &marimba_pdata,
},
};
#endif

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_3K
#define SYNAPTICS_I2C_RMI4_NAME "synaptics-rmi4-ts"
#define TS_I2C_ADDR		0x22
#define GPIO_TS_IRQ		82
#define GPIO_TOUCH_EN	107

static int touchscreen_power(int on)
{
	int ret=0;

	ret = gpio_request(GPIO_TOUCH_EN, "touch voltage");
	if (!ret){
		if (on==1){
			gpio_direction_output(GPIO_TOUCH_EN, 1);
			msleep(10);
			/*{
			    int ret2=0;
			    ret2=__gpio_get_value(GPIO_TOUCH_EN);
			    printk("%s: gpio 107=%d\n", __func__, ret2);
			}*/
		}else if (on==0){
			gpio_direction_output(GPIO_TOUCH_EN,0);
	}
	}else{
		pr_err("touchscreen_power, gpio request error!\n");
		ret=-EIO;
	}

	gpio_free(GPIO_TOUCH_EN);

	return ret;
}

static struct synaptics_rmi4_data synaptics_ts_data = {
	.orientation	= 0,	// ROTATION_0 = 0,ROTATION_90 = 1, ROTATION_180 = 2, ROTATION_270 = 3
	.power	= touchscreen_power,
	.gpio_irq = GPIO_TS_IRQ,
};
#endif

#define GPIO_TS_IRQ		82
#define GPIO_TS_EN		107

#ifdef CONFIG_TOUCHSCREEN_CY8CTST241

#define CYPRESS_I2C_ADDR		0x24
#define CY_USE_MT		/* define if using Single-Touch */

/* optional init function; set up IRQ GPIO;
 * call reference in platform data structure
 */
static int cyttsp_i2c_init(int on)
{
	int ret;
	static int initonce=0;

	/* add any special code to initialize any required system hw
	 * such as regulators or gpio pins
	 */

	if (on) {
		if (initonce==0)
		{
			ret = gpio_request(GPIO_TS_EN, "touch voltage");
			if (ret){
				pr_err("%s, gpio request error!\n",__func__);
				return ret;
			}
			gpio_direction_output(GPIO_TS_EN, 1);
			msleep(10);
			gpio_set_value(GPIO_TS_EN,1);

			/* for MSM systems the call to gpio_direction_input can be
			 * replaced with the more explicit call:
			 gpio_tlmm_config(GPIO_CFG(GPIO_TS_IRQ, 0, GPIO_INPUT,
				GPIO_PULL_UP, GPIO_6MA), GPIO_ENABLE);
			 */
			ret = gpio_request(GPIO_TS_IRQ, "CYTTSP IRQ GPIO");
			if (ret) {
				printk("%s: Failed to request GPIO %d\n",
				       __func__, GPIO_TS_IRQ);
				return ret;
			}
			gpio_direction_input(GPIO_TS_IRQ);

			initonce=1;
		}
		else
		{
			// hardware reset
			printk("cyttsp_i2c_init~~hw reset~~\n");

			//pull down: as io
			//gpio_tlmm_config(GPIO_CFG(131, 0, GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
			//gpio_tlmm_config(GPIO_CFG(132, 0, GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
			//gpio_direction_output(131, 0);
			//gpio_direction_output(132, 0);
			gpio_direction_output(GPIO_TS_EN, 0);
			//gpio_direction_output(GPIO_TS_IRQ, 0);
			//msleep(500);
			msleep(1);

			//pull up
			//gpio_tlmm_config(GPIO_CFG(131, 2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
			//gpio_tlmm_config(GPIO_CFG(132, 2 , GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
			gpio_direction_output(GPIO_TS_EN, 1);
			//gpio_direction_input(GPIO_TS_IRQ);
 			msleep(100);
		}

	} else {
	
		gpio_direction_output(GPIO_TS_EN,0);
		gpio_free(GPIO_TS_EN);
		gpio_free(GPIO_TS_IRQ);
	}
	return 0;
}

static int cyttsp_i2c_wakeup(void)
{
    gpio_direction_output(GPIO_TS_IRQ, 1);
	usleep(10);	
	gpio_set_value(GPIO_TS_IRQ,0);
	usleep(10);
	gpio_set_value(GPIO_TS_IRQ,1);
	usleep(10);
	gpio_set_value(GPIO_TS_IRQ,0);
	usleep(10);
	gpio_set_value(GPIO_TS_IRQ,1);
	
	usleep(1000);
	gpio_direction_input(GPIO_TS_IRQ);

	return 0;
}

static struct cyttsp_platform_data cypress_i2c_ttsp_platform_data = {
	.wakeup = cyttsp_i2c_wakeup,
	//.wakeup = NULL,
	.init = cyttsp_i2c_init,
#ifdef CY_USE_MT
	.mt_sync = input_mt_sync,
#endif
	.maxx = 320,
	.maxy = 240,
	.flags = 0,
	.gen = CY_GEN2,
	.use_st = 0,
	.use_mt = 1,
	.use_trk_id = 0,
	.use_hndshk = 1,
	.use_timer = 0,
	.use_sleep = 1,
	.use_gestures = 0,
	.use_load_file = 0,
	.use_force_fw_update = 0,
	.use_virtual_keys = 1,
	/* activate up to 4 groups
	 * and set active distance
	 */
	.gest_set = CY_GEST_GRP_NONE | CY_ACT_DIST,
	/* change act_intrvl to customize the Active power state
	 * scanning/processing refresh interval for Operating mode
	 */
	.act_intrvl = CY_ACT_INTRVL_DFLT,
	/* change tch_tmout to customize the touch timeout for the
	 * Active power state for Operating mode
	 */
	.tch_tmout = CY_TCH_TMOUT_DFLT,
	/* change lp_intrvl to customize the Low Power power state
	 * scanning/processing refresh interval for Operating mode
	 */
	.lp_intrvl = CY_LP_INTRVL_DFLT,
	.name = CY_I2C_NAME,
	.irq_gpio = GPIO_TS_IRQ,
};

#endif


static struct i2c_board_info i2c_touch_devices[] = {
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI
	{
		.type         = "synaptics-rmi-ts",
		/*.flags        = ,*/
		.addr         = 0x22,
		.irq          = MSM_GPIO_TO_INT(82),
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_CY8CTST241
	{
		I2C_BOARD_INFO(CY_I2C_NAME, CYPRESS_I2C_ADDR ),
		.irq = MSM_GPIO_TO_INT(GPIO_TS_IRQ),
		//.platform_data = touchscreen_power,
		.platform_data = &cypress_i2c_ttsp_platform_data,
	},
#endif

#ifdef CONFIG_TOUCHSCREEN_FOCALTECH_NEW		
	{				
		.type         = "ft5x0x_ts",
		/*.flags        = ,*/
		.addr         = 0x3E, 
		.irq          = MSM_GPIO_TO_INT(82),		
	},	
#endif
#ifdef CONFIG_TOUCHSCREEN_MXT224
        {    
	        I2C_BOARD_INFO(ATMEL_QT602240_NAME, 0x4a ),
	        .platform_data = &atmel_data,
	        .irq = MSM_GPIO_TO_INT(82),
	},   
#endif
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_3K
{
		I2C_BOARD_INFO(SYNAPTICS_I2C_RMI4_NAME, TS_I2C_ADDR ),
		.irq = MSM_GPIO_TO_INT(GPIO_TS_IRQ),
		//.platform_data = touchscreen_power,
		.platform_data = &synaptics_ts_data,
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS
{				
	.type		  = "melfas-ts",
	/*.flags		= ,*/
	.addr		  = 0x6A, 
	.irq		  = MSM_GPIO_TO_INT(82),		
},	


#endif
};
#if defined(CONFIG_I2C) && defined(CONFIG_GPIO_SX150X)
static void __init register_i2c_devices(void)
{

	i2c_register_board_info(MSM_GSBI0_QUP_I2C_BUS_ID,
				cam_exp_i2c_info,
				ARRAY_SIZE(cam_exp_i2c_info));

	if (1/*machine_is_msm7x27a_surf()*/)
		sx150x_data[SX150X_CORE].io_open_drain_ena = 0xe0f0;

	core_exp_i2c_info[0].platform_data =
			&sx150x_data[SX150X_CORE];

	i2c_register_board_info(MSM_GSBI1_QUP_I2C_BUS_ID,
				core_exp_i2c_info,
				ARRAY_SIZE(core_exp_i2c_info));
#if defined(CONFIG_BT) && defined(CONFIG_MARIMBA_CORE)
	i2c_register_board_info(MSM_GSBI1_QUP_I2C_BUS_ID,
				bahama_devices,
				ARRAY_SIZE(bahama_devices));
#endif
}
#endif

static struct msm_gpio qup_i2c_gpios_io[] = {
	{ GPIO_CFG(60, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
		"qup_scl" },
	{ GPIO_CFG(61, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
		"qup_sda" },
	{ GPIO_CFG(131, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"qup_scl" },
	{ GPIO_CFG(132, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"qup_sda" },
};

static struct msm_gpio qup_i2c_gpios_hw[] = {
	{ GPIO_CFG(60, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
		"qup_scl" },
	{ GPIO_CFG(61, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
		"qup_sda" },
	{ GPIO_CFG(131, 2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"qup_scl" },
	{ GPIO_CFG(132, 2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"qup_sda" },
};

static void gsbi_qup_i2c_gpio_config(int adap_id, int config_type)
{
	int rc;

	if (adap_id < 0 || adap_id > 1)
		return;

	/* Each adapter gets 2 lines from the table */
	if (config_type)
		rc = msm_gpios_request_enable(&qup_i2c_gpios_hw[adap_id*2], 2);
	else
		rc = msm_gpios_request_enable(&qup_i2c_gpios_io[adap_id*2], 2);
	if (rc < 0)
		pr_err("QUP GPIO request/enable failed: %d\n", rc);
}

static struct msm_i2c_platform_data msm_gsbi0_qup_i2c_pdata = {
	.clk_freq		= 100000,
	.msm_i2c_config_gpio	= gsbi_qup_i2c_gpio_config,
};

static struct msm_i2c_platform_data msm_gsbi1_qup_i2c_pdata = {
	.clk_freq		= 300000,
	.msm_i2c_config_gpio	= gsbi_qup_i2c_gpio_config,
};

#ifdef CONFIG_ARCH_MSM7X27A
/* old values
//ruanmeisi for liuhaitao 20111220, HVGA(480x320)
#define MSM_PMEM_MDP_SIZE       0x1DD1000 //stock p752d
#define MSM_PMEM_MDP_SIZE       (13*1024*1024) //codeaurora spec, 13M
#define MSM7x25A_MSM_PMEM_MDP_SIZE	0x1000000
#define MSM_PMEM_ADSP_SIZE      0x1000000 //stock p752d
#define MSM_PMEM_ADSP_SIZE      0xB71000 //codeaurora spec, 11.5M
#define MSM7x25A_MSM_PMEM_ADSP_SIZE      0xB91000


#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
//ruanmeisi for liuhaitao 20111220, HVGA(480x320)
#define MSM_FB_SIZE		0x260000 //stock p752d
#define MSM_FB_SIZE		(1*1024*1024) //codeaurora spec, 1M
#else
#define MSM_FB_SIZE		0x195000 //stock*/

//values from Konstat (atlas40)
#define MSM_PMEM_MDP_SIZE       0x2300000
#define MSM7x25A_MSM_PMEM_MDP_SIZE       0x1500000

#define MSM_PMEM_ADSP_SIZE      0x1000000
#define MSM7x25A_MSM_PMEM_ADSP_SIZE      0xB91000


#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
#define MSM_FB_SIZE		0x46512C
#define MSM7x25A_MSM_FB_SIZE	0xE1000
#else
#define MSM_FB_SIZE		0x196000
#define MSM7x25A_MSM_FB_SIZE	0x96000


#endif

#endif

#ifdef CONFIG_USB_G_ANDROID
static struct android_usb_platform_data android_usb_pdata = {
	.update_pid_and_serial_num = usb_diag_update_pid_and_serial_num,
};

static struct platform_device android_usb_device = {
        .name       = "android_usb",
        .id         = -1,
        .dev        = {
                .platform_data = &android_usb_pdata,
        },
};
#endif

#ifdef CONFIG_USB_EHCI_MSM_72K
static void msm_hsusb_vbus_power(unsigned phy_info, int on)
{
	int rc = 0;
	unsigned gpio;

	gpio = GPIO_HOST_VBUS_EN;

	rc = gpio_request(gpio, "i2c_host_vbus_en");
	if (rc < 0) {
		pr_err("failed to request %d GPIO\n", gpio);
		return;
	}
	gpio_direction_output(gpio, !!on);
	gpio_set_value_cansleep(gpio, !!on);
	gpio_free(gpio);
}

static struct msm_usb_host_platform_data msm_usb_host_pdata = {
	.phy_info       = (USB_PHY_INTEGRATED | USB_PHY_MODEL_45NM),
};

static void __init msm7x2x_init_host(void)
{
	msm_add_host(0, &msm_usb_host_pdata);
}
#endif

#ifdef CONFIG_USB_MSM_OTG_72K
static int hsusb_rpc_connect(int connect)
{
	if (connect)
		return msm_hsusb_rpc_connect();
	else
		return msm_hsusb_rpc_close();
}

static struct vreg *vreg_3p3;
static int msm_hsusb_ldo_init(int init)
{
	if (init) {
		vreg_3p3 = vreg_get(NULL, "usb");
		if (IS_ERR(vreg_3p3))
			return PTR_ERR(vreg_3p3);
	} else
		vreg_put(vreg_3p3);

	return 0;
}

static int msm_hsusb_ldo_enable(int enable)
{
	static int ldo_status;

	if (!vreg_3p3 || IS_ERR(vreg_3p3))
		return -ENODEV;

	if (ldo_status == enable)
		return 0;

	ldo_status = enable;

	if (enable)
		return vreg_enable(vreg_3p3);

	return vreg_disable(vreg_3p3);
}

#ifndef CONFIG_USB_EHCI_MSM_72K
static int msm_hsusb_pmic_notif_init(void (*callback)(int online), int init)
{
	int ret = 0;

	if (init)
		ret = msm_pm_app_rpc_init(callback);
	else
		msm_pm_app_rpc_deinit(callback);

	return ret;
}
#endif

static struct msm_otg_platform_data msm_otg_pdata = {
#ifndef CONFIG_USB_EHCI_MSM_72K
	.pmic_vbus_notif_init	 = msm_hsusb_pmic_notif_init,
#else
	.vbus_power		 = msm_hsusb_vbus_power,
#endif
	.rpc_connect		 = hsusb_rpc_connect,
	.core_clk		 = 1,
	.pemp_level		 = PRE_EMPHASIS_WITH_20_PERCENT,
	.cdr_autoreset		 = CDR_AUTO_RESET_DISABLE,
	.drv_ampl		 = HS_DRV_AMPLITUDE_DEFAULT,
	.se1_gating		 = SE1_GATING_DISABLE,
	.ldo_init		 = msm_hsusb_ldo_init,
	.ldo_enable		 = msm_hsusb_ldo_enable,
	.chg_init		 = hsusb_chg_init,
	.chg_connected		 = hsusb_chg_connected,
	.chg_vbus_draw		 = hsusb_chg_vbus_draw,
};
#endif

static struct msm_hsusb_gadget_platform_data msm_gadget_pdata = {
	.is_phy_status_timer_on = 1,
};

static struct resource smc91x_resources[] = {
	[0] = {
		.start = 0x90000300,
		.end   = 0x900003ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = MSM_GPIO_TO_INT(4),
		.end   = MSM_GPIO_TO_INT(4),
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device smc91x_device = {
	.name           = "smc91x",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(smc91x_resources),
	.resource       = smc91x_resources,
};

#if (defined(CONFIG_MMC_MSM_SDC1_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC2_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC3_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC4_SUPPORT))

static unsigned long vreg_sts, gpio_sts;
static struct vreg *vreg_mmc;
static struct vreg *vreg_emmc;

struct sdcc_vreg {
	struct vreg *vreg_data;
	unsigned level;
};

static struct sdcc_vreg sdcc_vreg_data[4];

struct sdcc_gpio {
	struct msm_gpio *cfg_data;
	uint32_t size;
	struct msm_gpio *sleep_cfg_data;
};

static struct msm_gpio sdc1_cfg_data[] = {
	{GPIO_CFG(51, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc1_dat_3"},
	{GPIO_CFG(52, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc1_dat_2"},
	{GPIO_CFG(53, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc1_dat_1"},
	{GPIO_CFG(54, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc1_dat_0"},
	{GPIO_CFG(55, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc1_cmd"},
	{GPIO_CFG(56, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
								"sdc1_clk"},
};

static struct msm_gpio sdc2_cfg_data[] = {
	{GPIO_CFG(62, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
								"sdc2_clk"},
	{GPIO_CFG(63, 2, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc2_cmd"},
	{GPIO_CFG(64, 2, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc2_dat_3"},
	{GPIO_CFG(65, 2, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc2_dat_2"},
	{GPIO_CFG(66, 2, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc2_dat_1"},
	{GPIO_CFG(67, 2, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc2_dat_0"},
};

static struct msm_gpio sdc2_sleep_cfg_data[] = {
	{GPIO_CFG(62, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
								"sdc2_clk"},
	{GPIO_CFG(63, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
								"sdc2_cmd"},
	{GPIO_CFG(64, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
								"sdc2_dat_3"},
	{GPIO_CFG(65, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
								"sdc2_dat_2"},
	{GPIO_CFG(66, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
								"sdc2_dat_1"},
	{GPIO_CFG(67, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
								"sdc2_dat_0"},
};
static struct msm_gpio sdc3_cfg_data[] = {
	{GPIO_CFG(88, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
								"sdc3_clk"},
	{GPIO_CFG(89, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc3_cmd"},
	{GPIO_CFG(90, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc3_dat_3"},
	{GPIO_CFG(91, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc3_dat_2"},
	{GPIO_CFG(92, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc3_dat_1"},
	{GPIO_CFG(93, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc3_dat_0"},
#ifdef CONFIG_MMC_MSM_SDC3_8_BIT_SUPPORT
	{GPIO_CFG(19, 3, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc3_dat_7"},
	{GPIO_CFG(20, 3, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc3_dat_6"},
	{GPIO_CFG(21, 3, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc3_dat_5"},
	{GPIO_CFG(108, 3, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc3_dat_4"},
#endif
};

static struct msm_gpio sdc4_cfg_data[] = {
	#ifdef CONFIG_GPIO_19_20_CTRL_LED//added by zhang.yu_1 for marm led control @110727
	{GPIO_CFG(0xff, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
							"sdc4_dat_3"},
	{GPIO_CFG(0xff, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc4_dat_2"},
	#else
	{GPIO_CFG(19, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc4_dat_3"},
	{GPIO_CFG(20, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc4_dat_2"},
	#endif
	{GPIO_CFG(21, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc4_dat_1"},
	{GPIO_CFG(107, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc4_cmd"},
	{GPIO_CFG(108, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
								"sdc4_dat_0"},
	{GPIO_CFG(109, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
								"sdc4_clk"},
};

static struct sdcc_gpio sdcc_cfg_data[] = {
	{
		.cfg_data = sdc1_cfg_data,
		.size = ARRAY_SIZE(sdc1_cfg_data),
	},
	{
		.cfg_data = sdc2_cfg_data,
		.size = ARRAY_SIZE(sdc2_cfg_data),
		.sleep_cfg_data = sdc2_sleep_cfg_data,
	},
	{
		.cfg_data = sdc3_cfg_data,
		.size = ARRAY_SIZE(sdc3_cfg_data),
	},
	{
		.cfg_data = sdc4_cfg_data,
		.size = ARRAY_SIZE(sdc4_cfg_data),
	},
};

static int msm_sdcc_setup_gpio(int dev_id, unsigned int enable)
{
	int rc = 0;
	struct sdcc_gpio *curr;

	curr = &sdcc_cfg_data[dev_id - 1];
	if (!(test_bit(dev_id, &gpio_sts)^enable))
		return rc;

	if (enable) {
		set_bit(dev_id, &gpio_sts);
		rc = msm_gpios_request_enable(curr->cfg_data, curr->size);
		if (rc)
			pr_err("%s: Failed to turn on GPIOs for slot %d\n",
					__func__,  dev_id);
	} else {
		clear_bit(dev_id, &gpio_sts);
		if (curr->sleep_cfg_data) {
			rc = msm_gpios_enable(curr->sleep_cfg_data, curr->size);
			msm_gpios_free(curr->sleep_cfg_data, curr->size);
			return rc;
		}
		msm_gpios_disable_free(curr->cfg_data, curr->size);
	}
	return rc;
}

static int msm_sdcc_setup_vreg(int dev_id, unsigned int enable)
{
	int rc = 0;
	struct sdcc_vreg *curr;

	curr = &sdcc_vreg_data[dev_id - 1];

	if (!(test_bit(dev_id, &vreg_sts)^enable))
		return rc;

	if (enable) {
		set_bit(dev_id, &vreg_sts);
		rc = vreg_set_level(curr->vreg_data, curr->level);
		if (rc)
			pr_err("%s: vreg_set_level() = %d\n", __func__, rc);

		rc = vreg_enable(curr->vreg_data);
		if (rc)
			pr_err("%s: vreg_enable() = %d\n", __func__, rc);
	} else {
		clear_bit(dev_id, &vreg_sts);
		rc = vreg_disable(curr->vreg_data);
		if (rc)
			pr_err("%s: vreg_disable() = %d\n", __func__, rc);
	}
	return rc;
}

static uint32_t msm_sdcc_setup_power(struct device *dv, unsigned int vdd)
{
	int rc = 0;
	struct platform_device *pdev;

	pdev = container_of(dv, struct platform_device, dev);

	rc = msm_sdcc_setup_gpio(pdev->id, !!vdd);
	if (rc)
		goto out;
		
        if(pdev->id==2)
        {     
            return 0;
        }
	
	rc = msm_sdcc_setup_vreg(pdev->id, !!vdd);
out:
	return rc;
}

#define GPIO_SDC1_HW_DET 85

#if defined(CONFIG_MMC_MSM_SDC1_SUPPORT) \
	&& defined(CONFIG_MMC_MSM_CARD_HW_DETECTION)
static unsigned int msm7x2xa_sdcc_slot_status(struct device *dev)
{
	int status;

	status = gpio_tlmm_config(GPIO_CFG(GPIO_SDC1_HW_DET, 2, GPIO_CFG_INPUT,
			GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	if (status)
		pr_err("%s:Failed to configure tlmm for GPIO %d\n", __func__,
				GPIO_SDC1_HW_DET);

	status = gpio_request(GPIO_SDC1_HW_DET, "SD_HW_Detect");
	if (status) {
		pr_err("%s:Failed to request GPIO %d\n", __func__,
				GPIO_SDC1_HW_DET);
	} else {
		status = gpio_direction_input(GPIO_SDC1_HW_DET);
		if (!status)
			status = gpio_get_value(GPIO_SDC1_HW_DET);
		gpio_free(GPIO_SDC1_HW_DET);
	}
	return status;
}
#endif

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
static struct mmc_platform_data sdc1_plat_data = {
	.ocr_mask	= MMC_VDD_28_29,
	.translate_vdd  = msm_sdcc_setup_power,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.msmsdcc_fmin	= 144000,
	.msmsdcc_fmid	= 24576000,
	.msmsdcc_fmax	= 49152000,
#ifdef CONFIG_MMC_MSM_CARD_HW_DETECTION
	.status      = msm7x2xa_sdcc_slot_status,
	.status_irq  = MSM_GPIO_TO_INT(GPIO_SDC1_HW_DET),
	.irq_flags   = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
#else
       .enable_polling_timer = 1,
#endif
       .nonremovable	= 0,
};
#endif

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
static struct mmc_platform_data sdc2_plat_data = {
	/*
	 * SDC2 supports only 1.8V, claim for 2.85V range is just
	 * for allowing buggy cards who advertise 2.8V even though
	 * they can operate at 1.8V supply.
	 */
	.ocr_mask	= MMC_VDD_28_29 | MMC_VDD_165_195,
	.translate_vdd  = msm_sdcc_setup_power,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
#ifdef CONFIG_MMC_MSM_SDIO_SUPPORT
#ifdef CONFIG_ATH_WIFI
#ifdef CONFIG_WOW_BY_SDIO_DATA1 
	.sdiowakeup_irq = MSM_GPIO_TO_INT(66),
#else
	.sdiowakeup_irq = MSM_GPIO_TO_INT(92),
#endif
#endif	
#endif
	.msmsdcc_fmin	= 144000,
	.msmsdcc_fmid	= 24576000,
	.msmsdcc_fmax	= 49152000,
#ifdef CONFIG_MMC_MSM_SDC2_DUMMY52_REQUIRED
	.dummy52_required = 1,
#endif
};
#endif

#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
static struct mmc_platform_data sdc3_plat_data = {
	.ocr_mask	= MMC_VDD_28_29,
	.translate_vdd  = msm_sdcc_setup_power,
#ifdef CONFIG_MMC_MSM_SDC3_8_BIT_SUPPORT
	.mmc_bus_width  = MMC_CAP_8_BIT_DATA,
#else
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
#endif
	.msmsdcc_fmin	= 144000,
	.msmsdcc_fmid	= 24576000,
	.msmsdcc_fmax	= 49152000,
	.nonremovable	= 1,
};
#endif

#if (defined(CONFIG_MMC_MSM_SDC4_SUPPORT)\
		&& !defined(CONFIG_MMC_MSM_SDC3_8_BIT_SUPPORT))
static struct mmc_platform_data sdc4_plat_data = {
	.ocr_mask	= MMC_VDD_28_29,
	.translate_vdd  = msm_sdcc_setup_power,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.msmsdcc_fmin	= 144000,
	.msmsdcc_fmid	= 24576000,
	.msmsdcc_fmax	= 49152000,
};
#endif
#endif

#ifdef CONFIG_SERIAL_MSM_HS
static struct msm_serial_hs_platform_data msm_uart_dm1_pdata = {
	.inject_rx_on_wakeup	= 1,
	.rx_to_inject		= 0xFD,
};
#endif
static struct msm_pm_platform_data msm7x27a_pm_data[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] = {
					.idle_supported = 1,
					.suspend_supported = 1,
					.idle_enabled = 1,
					.suspend_enabled = 1,
					.latency = 16000,
					.residency = 20000,
	},
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN] = {
					.idle_supported = 1,
					.suspend_supported = 1,
					.idle_enabled = 1,
					.suspend_enabled = 1,
					.latency = 12000,
					.residency = 20000,
	},
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT] = {
					.idle_supported = 1,
					.suspend_supported = 1,
					.idle_enabled = 0,
					.suspend_enabled = 1,
					.latency = 2000,
					.residency = 0,
	},
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT] = {
					.idle_supported = 1,
					.suspend_supported = 1,
					.idle_enabled = 1,
					.suspend_enabled = 1,
					.latency = 2,
					.residency = 0,
	},
};

static struct android_pmem_platform_data android_pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
	.memory_type = MEMTYPE_EBI1,
};

static struct platform_device android_pmem_adsp_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &android_pmem_adsp_pdata },
};

static unsigned pmem_mdp_size = MSM_PMEM_MDP_SIZE;
static int __init pmem_mdp_size_setup(char *p)
{
	pmem_mdp_size = memparse(p, NULL);
	return 0;
}

early_param("pmem_mdp_size", pmem_mdp_size_setup);

static unsigned pmem_adsp_size = MSM_PMEM_ADSP_SIZE;
static int __init pmem_adsp_size_setup(char *p)
{
	pmem_adsp_size = memparse(p, NULL);
	return 0;
}

early_param("pmem_adsp_size", pmem_adsp_size_setup);

/*
 * Reserve enough v4l2 space for a double buffered full screen
 * res image (864x480x1.5x2)
 */
#define MSM_V4L2_VIDEO_OVERLAY_BUF_SIZE 1244160

static unsigned fb_size = MSM_FB_SIZE;
static int __init fb_size_setup(char *p)
{
	fb_size = memparse(p, NULL);
	return 0;
}

early_param("fb_size", fb_size_setup);

#if 0
#define LCDC_CONFIG_PROC          21
#define LCDC_UN_CONFIG_PROC       22
#define LCDC_API_PROG             0x30000066
#define LCDC_API_VERS             0x00010001

static struct msm_rpc_endpoint *lcdc_ep;

static int msm_fb_lcdc_config(int on)
{
	int rc = 0;
	struct rpc_request_hdr hdr;

	if (on)
		pr_info("lcdc config\n");
	else
		pr_info("lcdc un-config\n");

	lcdc_ep = msm_rpc_connect_compatible(LCDC_API_PROG, LCDC_API_VERS, 0);

	if (IS_ERR(lcdc_ep)) {
		printk(KERN_ERR "%s: msm_rpc_connect failed! rc = %ld\n",
			__func__, PTR_ERR(lcdc_ep));
		return -EINVAL;
	}

	rc = msm_rpc_call(lcdc_ep,
		(on) ? LCDC_CONFIG_PROC : LCDC_UN_CONFIG_PROC,
		&hdr, sizeof(hdr), 5 * HZ);

	if (rc)
		printk(KERN_ERR
			"%s: msm_rpc_call failed! rc = %d\n", __func__, rc);

	msm_rpc_close(lcdc_ep);
	return rc;
}
#endif
static const char * const msm_fb_lcdc_vreg[] = {
		"bt",
		//"msme1",
};

static const int msm_fb_lcdc_vreg_mV[] = {
	2800,
	//1800,
};

struct vreg *lcdc_vreg[ARRAY_SIZE(msm_fb_lcdc_vreg)];

static uint32_t lcdc_gpio_initialized;

static void lcdc_toshiba_gpio_init(void)
{
	int i, rc = 0;
	if (!lcdc_gpio_initialized) {
		#if 0
		if (gpio_request(124, "spi_clk")) {
			pr_err("failed to request gpio spi_clk\n");
			return;
		}
		if (gpio_request(122, "spi_cs")) {
			pr_err("failed to request gpio spi_cs0_N\n");
			goto fail_gpio6;
		}
		if (gpio_request(109, "spi_mosi")) {
			pr_err("failed to request gpio spi_mosi\n");
			goto fail_gpio5;
		}
		if (gpio_request(123, "spi_miso")) {
			pr_err("failed to request gpio spi_miso\n");
			goto fail_gpio4;
		}
		if (gpio_request(89, "gpio_disp_rst")) {
			pr_err("failed to request gpio_disp_pwr\n");
			goto fail_gpio3;
		}
		if (gpio_request(97, "gpio_bkl_en")) {
			pr_err("failed to request gpio_bkl_en\n");
			goto fail_gpio2;
		}
		#endif
		//pmapp_disp_backlight_init();

		for (i = 0; i < ARRAY_SIZE(msm_fb_lcdc_vreg); i++) {
			lcdc_vreg[i] = vreg_get(0, msm_fb_lcdc_vreg[i]);

			rc = vreg_set_level(lcdc_vreg[i],
						msm_fb_lcdc_vreg_mV[i]);

			if (rc < 0) {
				pr_err("%s: set regulator level failed "
					"with :(%d)\n", __func__, rc);
				goto fail_gpio1;
			}
		}
		lcdc_gpio_initialized = 1;
	}
	return;

fail_gpio1:
	for (; i > 0; i--)
			vreg_put(lcdc_vreg[i - 1]);
#if 0
	gpio_free(97);
fail_gpio2:
	gpio_free(89);
fail_gpio3:
	gpio_free(123);
fail_gpio4:
	gpio_free(109);
fail_gpio5:
	gpio_free(122);
fail_gpio6:
	gpio_free(124);
	#endif
	lcdc_gpio_initialized = 0;
}

static uint32_t lcdc_gpio_table[] = {
	124,
	122,
	109,
	89,
	//GPIO_BACKLIGHT_EN,
	123,
	97,
	48,
};

static void config_lcdc_gpio_table(uint32_t *table, int len, unsigned enable)
{
	int n;
	if(gpio_tlmm_config(GPIO_CFG(89, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE))
		pr_err("failed to gpio_tlmm_config lcd reset\n");
	gpio_tlmm_config(GPIO_CFG(48, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
if (gpio_request(124, "spi_clk")) {
			pr_err("failed to request gpio spi_clk\n");
			return;
		}
		if (gpio_request(122, "spi_cs")) {
			pr_err("failed to request gpio spi_cs0_N\n");
			//goto fail_gpio6;
		}
		if (gpio_request(109, "spi_mosi")) {
			pr_err("failed to request gpio spi_mosi\n");
			//goto fail_gpio5;
		}
		if (gpio_request(123, "spi_miso")) {
			pr_err("failed to request gpio spi_miso\n");
			//goto fail_gpio4;
		}
		if (gpio_request(89, "gpio_disp_rst")) {
			pr_err("failed to request gpio_disp_pwr\n");
			//goto fail_gpio3;
		}
		if (gpio_request(97, "gpio_bkl_en")) {
			pr_err("failed to request gpio_bkl_en\n");
			//goto fail_gpio2;
		}
		if (gpio_request(48, "gpio_lcd_ic_id")) {
			pr_err("failed to request gpio_lcd_ic_id\n");
			//goto fail_gpio2;
		}
	if (lcdc_gpio_initialized) {
		/* All are IO Expander GPIOs */
		for (n = 0; n < (len - 1); n++);
			//gpio_direction_output(table[n], 1);
	}
}

static void lcdc_toshiba_config_gpios(int enable)
{
	config_lcdc_gpio_table(lcdc_gpio_table,
		ARRAY_SIZE(lcdc_gpio_table), enable);
}

static int msm_fb_lcdc_power_save(int on)
{
	int i, rc = 0;
	/* Doing the init of the LCDC GPIOs very late as they are from
		an I2C-controlled IO Expander */
	lcdc_toshiba_gpio_init();

	if (lcdc_gpio_initialized) {
		//gpio_set_value_cansleep(GPIO_DISPLAY_PWR_EN, on);
		//gpio_set_value_cansleep(GPIO_BACKLIGHT_EN, on);

		for (i = 0; i < ARRAY_SIZE(msm_fb_lcdc_vreg); i++) {
			if (on) {
				rc = vreg_enable(lcdc_vreg[i]);

				if (rc) {
					printk(KERN_ERR "vreg_enable: %s vreg"
						"operation failed\n",
						msm_fb_lcdc_vreg[i]);
						goto lcdc_vreg_fail;
				}
			} else {
				/* modify by lukai to keep L17 alway on.L17 is vdd of sensors
				rc = vreg_disable(lcdc_vreg[i]);

				if (rc) {
					printk(KERN_ERR "vreg_disable: %s vreg "
						"operation failed\n",
						msm_fb_lcdc_vreg[i]);
					goto lcdc_vreg_fail;
				}
			
			*/
			}
		}
	}

	return rc;

lcdc_vreg_fail:
	if (on) {
		for (; i > 0; i--)
			vreg_disable(lcdc_vreg[i - 1]);
	} else {
		for (; i > 0; i--)
			vreg_enable(lcdc_vreg[i - 1]);
	}

return rc;

}


static int lcdc_toshiba_set_bl(int level)
{
	int ret;

	return 0;

	ret = pmapp_disp_backlight_set_brightness(level);
	if (ret)
		pr_err("%s: can't set lcd backlight!\n", __func__);

	return ret;
}


static struct lcdc_platform_data lcdc_pdata = {
	.lcdc_gpio_config = NULL,
	.lcdc_power_save   = msm_fb_lcdc_power_save,
};

static int lcd_panel_spi_gpio_num[] = {
		124,   /* spi_clk */
		122, /* spi_cs  */
		109,  /* spi_sdoi */
		123,  /* spi_sdi */
		89, /* rst */
		97,
		48,
};

static struct msm_panel_common_pdata lcdc_toshiba_panel_data = {
	.panel_config_gpio = lcdc_toshiba_config_gpios,
	.pmic_backlight = lcdc_toshiba_set_bl,
	.gpio_num	  = lcd_panel_spi_gpio_num,
};

static struct platform_device lcdc_toshiba_panel_device = {
	.name   = "lcdc_panel_qvga",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_toshiba_panel_data,
	}
};

static struct resource msm_fb_resources[] = {
	{
		.flags  = IORESOURCE_DMA,
	}
};

#ifdef CONFIG_MSM_V4L2_VIDEO_OVERLAY_DEVICE
static struct resource msm_v4l2_video_overlay_resources[] = {
	{
		.flags = IORESOURCE_DMA,
	}
};
#endif

static int msm_fb_detect_panel(const char *name)
{
	int ret = -EPERM;

	if (1/*machine_is_msm7x27a_surf()*/) {
		if (!strncmp(name, "lcdc_toshiba_fwvga_pt", 21))
			ret = 0;
	} else {
		ret = -ENODEV;
	}

	return ret;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_fb_resources),
	.resource       = msm_fb_resources,
	.dev    = {
		.platform_data = &msm_fb_pdata,
	}
};

#ifdef CONFIG_MSM_V4L2_VIDEO_OVERLAY_DEVICE
static struct platform_device msm_v4l2_video_overlay_device = {
		.name   = "msm_v4l2_overlay_pd",
		.id     = 0,
		.num_resources  = ARRAY_SIZE(msm_v4l2_video_overlay_resources),
		.resource       = msm_v4l2_video_overlay_resources,
	};
#endif


#ifdef CONFIG_FB_MSM_MIPI_DSI
static int mipi_renesas_set_bl(int level)
{
	int ret;

	ret = pmapp_disp_backlight_set_brightness(level);

	if (ret)
		pr_err("%s: can't set lcd backlight!\n", __func__);

	return ret;
}

static struct msm_panel_common_pdata mipi_renesas_pdata = {
	.pmic_backlight = mipi_renesas_set_bl,
};


static struct platform_device mipi_dsi_renesas_panel_device = {
	.name = "mipi_renesas",
	.id = 0,
	.dev    = {
		.platform_data = &mipi_renesas_pdata,
	}
};
#endif

static void __init msm7x27a_init_mmc(void)
{
	vreg_emmc = vreg_get(NULL, "emmc");
	if (IS_ERR(vreg_emmc)) {
		pr_err("%s: vreg get failed (%ld)\n",
				__func__, PTR_ERR(vreg_emmc));
		return;
	}

	vreg_mmc = vreg_get(NULL, "mmc");
	if (IS_ERR(vreg_mmc)) {
		pr_err("%s: vreg get failed (%ld)\n",
				__func__, PTR_ERR(vreg_mmc));
		return;
	}

	/* eMMC slot */
#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
	sdcc_vreg_data[2].vreg_data = vreg_emmc;
	sdcc_vreg_data[2].level = 3000;
	msm_add_sdcc(3, &sdc3_plat_data);
#endif
	/* Micro-SD slot */
#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	sdcc_vreg_data[0].vreg_data = vreg_mmc;
	sdcc_vreg_data[0].level = 2850;
	msm_add_sdcc(1, &sdc1_plat_data);
#endif
	/* SDIO WLAN slot */
#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
	sdcc_vreg_data[1].vreg_data = vreg_mmc;
	sdcc_vreg_data[1].level = 2850;
	msm_add_sdcc(2, &sdc2_plat_data);
#endif
	/* Not Used */
#if (defined(CONFIG_MMC_MSM_SDC4_SUPPORT)\
		&& !defined(CONFIG_MMC_MSM_SDC3_8_BIT_SUPPORT))
	sdcc_vreg_data[3].vreg_data = vreg_mmc;
	sdcc_vreg_data[3].level = 2850;
	msm_add_sdcc(4, &sdc4_plat_data);
#endif
}
#define SND(desc, num) { .name = #desc, .id = num }
static struct snd_endpoint snd_endpoints_list[] = {
	SND(HANDSET, 0),
	SND(MONO_HEADSET, 2),
	SND(HEADSET, 3),
	SND(SPEAKER, 6),
	SND(TTY_HEADSET, 8),
	SND(TTY_VCO, 9),
	SND(TTY_HCO, 10),
	SND(BT_EC_OFF,11),
	SND(BT, 12),
	SND(AUXMIC_SPEAKER, 14),
	SND(IN_S_SADC_OUT_HANDSET, 16),
	SND(IN_S_SADC_OUT_SPEAKER_PHONE, 25),
	SND(FM_DIGITAL_STEREO_HEADSET, 26),
	SND(FM_DIGITAL_SPEAKER_PHONE, 27),
	SND(FM_DIGITAL_BT_A2DP_HEADSET, 28),
	SND(HEADSET_AND_SPEAKER, 31),
	SND(CURRENT, 38),
};
#undef SND

static struct msm_snd_endpoints msm_device_snd_endpoints = {
	.endpoints = snd_endpoints_list,
	.num = sizeof(snd_endpoints_list) / sizeof(struct snd_endpoint)
};

static struct platform_device msm_device_snd = {
	.name = "msm_snd",
	.id = -1,
	.dev    = {
		.platform_data = &msm_device_snd_endpoints
	},
};

#define DEC0_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC1_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC2_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC3_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC4_FORMAT (1<<MSM_ADSP_CODEC_MIDI)

static unsigned int dec_concurrency_table[] = {
	/* Audio LP */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DMA)), 0,
	0, 0, 0,

	/* Concurrency 1 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	 /* Concurrency 2 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 3 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 4 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 5 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),

	/* Concurrency 6 */
#if 0
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	0, 0, 0, 0,
#else
// chenjun:support decoder Concurrency
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),
#endif

	/* Concurrency 7 */
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC4_FORMAT),
};

#define DEC_INFO(name, queueid, decid, nr_codec) { .module_name = name, \
	.module_queueid = queueid, .module_decid = decid, \
	.nr_codec_support = nr_codec}

static struct msm_adspdec_info dec_info_list[] = {
	DEC_INFO("AUDPLAY0TASK", 13, 0, 11), /* AudPlay0BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY1TASK", 14, 1, 11),  /* AudPlay1BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY2TASK", 15, 2, 11),  /* AudPlay2BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY3TASK", 16, 3, 11),  /* AudPlay3BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY4TASK", 17, 4, 1),  /* AudPlay4BitStreamCtrlQueue */
};

static struct msm_adspdec_database msm_device_adspdec_database = {
	.num_dec = ARRAY_SIZE(dec_info_list),
	.num_concurrency_support = (ARRAY_SIZE(dec_concurrency_table) / \
					ARRAY_SIZE(dec_info_list)),
	.dec_concurrency_table = dec_concurrency_table,
	.dec_info_list = dec_info_list,
};

static struct platform_device msm_device_adspdec = {
	.name = "msm_adspdec",
	.id = -1,
	.dev    = {
		.platform_data = &msm_device_adspdec_database
	},
};

static struct android_pmem_platform_data android_pmem_audio_pdata = {
	.name = "pmem_audio",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
	.memory_type = MEMTYPE_EBI1,
};

static struct platform_device android_pmem_audio_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &android_pmem_audio_pdata },
};

static struct android_pmem_platform_data android_pmem_pdata = {
	.name = "pmem",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 1,
	.memory_type = MEMTYPE_EBI1,
};
static struct platform_device android_pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = { .platform_data = &android_pmem_pdata },
};

static u32 msm_calculate_batt_capacity(u32 current_voltage);

static struct msm_psy_batt_pdata msm_psy_batt_data = {
	.voltage_min_design     = 2800,
	.voltage_max_design     = 4300,
	.avail_chg_sources      = AC_CHG | USB_CHG ,
	.batt_technology        = POWER_SUPPLY_TECHNOLOGY_LION,
	.calculate_capacity     = &msm_calculate_batt_capacity,
};

static u32 msm_calculate_batt_capacity(u32 current_voltage)
{
	u32 low_voltage	 = msm_psy_batt_data.voltage_min_design;
	u32 high_voltage = msm_psy_batt_data.voltage_max_design;

	return (current_voltage - low_voltage) * 100
			/ (high_voltage - low_voltage);
}

static struct platform_device msm_batt_device = {
	.name               = "msm-battery",
	.id                 = -1,
	.dev.platform_data  = &msm_psy_batt_data,
};


static struct smsc911x_platform_config smsc911x_config = {
	.irq_polarity	= SMSC911X_IRQ_POLARITY_ACTIVE_HIGH,
	.irq_type	= SMSC911X_IRQ_TYPE_PUSH_PULL,
	.flags		= SMSC911X_USE_16BIT,
};


static struct resource smsc911x_resources[] = {
	[0] = {
		.start	= 0x90000000,
		.end	= 0x90007fff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= MSM_GPIO_TO_INT(48),
		.end	= MSM_GPIO_TO_INT(48),
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	},
};

static struct platform_device smsc911x_device = {
	.name		= "smsc911x",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(smsc911x_resources),
	.resource	= smsc911x_resources,
	.dev		= {
		.platform_data	= &smsc911x_config,
	},
};

static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= 83,
		.end	= 83,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= 90,  
		.end	= 90,    
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake",
		.start	= MSM_GPIO_TO_INT(83),
		.end	= MSM_GPIO_TO_INT(83),
		.flags	= IORESOURCE_IRQ,
	},
};
#if 0
static struct platform_device msm_bluesleep_device = {
	.name = "bluesleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};
#endif
static struct msm_gpio smsc911x_gpios[] = {
	{ GPIO_CFG(48, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_6MA),
							 "smsc911x_irq"  },
	{ GPIO_CFG(49, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_6MA),
							 "eth_fifo_sel" },
};

static struct platform_device msm_bcmsleep_device = {
	.name = "bcmsleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};
#define ETH_FIFO_SEL_GPIO	49
static void msm7x27a_cfg_smsc911x(void)
{
	int res;

	res = msm_gpios_request_enable(smsc911x_gpios,
				 ARRAY_SIZE(smsc911x_gpios));
	if (res) {
		pr_err("%s: unable to enable gpios for SMSC911x\n", __func__);
		return;
	}

	/* ETH_FIFO_SEL */
	res = gpio_direction_output(ETH_FIFO_SEL_GPIO, 0);
	if (res) {
		pr_err("%s: unable to get direction for gpio %d\n", __func__,
							 ETH_FIFO_SEL_GPIO);
		msm_gpios_disable_free(smsc911x_gpios,
						 ARRAY_SIZE(smsc911x_gpios));
		return;
	}
	gpio_set_value(ETH_FIFO_SEL_GPIO, 0);
}

#ifdef CONFIG_MSM_CAMERA
static uint32_t camera_off_gpio_table[] = {
	GPIO_CFG(15, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
};

static uint32_t camera_on_gpio_table[] = {
	GPIO_CFG(15, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_8MA),
};

#ifdef CONFIG_MSM_CAMERA_FLASH
static struct msm_camera_sensor_flash_src msm_flash_src = {
	.flash_sr_type = MSM_CAMERA_FLASH_SRC_CURRENT_DRIVER,
	._fsrc.current_driver_src.led1 = GPIO_CAM_GP_LED_EN1,
	._fsrc.current_driver_src.led2 = GPIO_CAM_GP_LED_EN2,
};
#endif

static struct vreg *vreg_gp1;
static struct vreg *vreg_gp2;
static struct vreg *vreg_gp3;
static void msm_camera_vreg_config(int vreg_en)
{
	int rc;

	if (vreg_gp1 == NULL) {
		vreg_gp1 = vreg_get(NULL, "msme1");
		if (IS_ERR(vreg_gp1)) {
			pr_err("%s: vreg_get(%s) failed (%ld)\n",
				__func__, "msme1", PTR_ERR(vreg_gp1));
			return;
		}

		rc = vreg_set_level(vreg_gp1, 1800);
		if (rc) {
			pr_err("%s: GP1 set_level failed (%d)\n",
				__func__, rc);
			return;
		}
	}

	if (vreg_gp2 == NULL) {
		vreg_gp2 = vreg_get(NULL, "gp2");
		if (IS_ERR(vreg_gp2)) {
			pr_err("%s: vreg_get(%s) failed (%ld)\n",
				__func__, "gp2", PTR_ERR(vreg_gp2));
			return;
		}

		rc = vreg_set_level(vreg_gp2, 2850);
		if (rc) {
			pr_err("%s: GP2 set_level failed (%d)\n",
				__func__, rc);
		}
	}

	if (vreg_gp3 == NULL) {
		vreg_gp3 = vreg_get(NULL, "usb2");
		if (IS_ERR(vreg_gp3)) {
			pr_err("%s: vreg_get(%s) failed (%ld)\n",
				__func__, "gp3", PTR_ERR(vreg_gp3));
			return;
		}

		rc = vreg_set_level(vreg_gp3, 1800);
		if (rc) {
			pr_err("%s: GP3 set level failed (%d)\n",
				__func__, rc);
		}
	}

	if (vreg_en) {
		rc = vreg_enable(vreg_gp1);
		if (rc) {
			pr_err("%s: GP1 enable failed (%d)\n",
				__func__, rc);
			return;
		}

		rc = vreg_enable(vreg_gp2);
		if (rc) {
			pr_err("%s: GP2 enable failed (%d)\n",
				__func__, rc);
		}

		rc = vreg_enable(vreg_gp3);
		if (rc) {
			pr_err("%s: GP3 enable failed (%d)\n",
				__func__, rc);
		}
	} else {
		rc = vreg_disable(vreg_gp1);
		if (rc)
			pr_err("%s: GP1 disable failed (%d)\n",
				__func__, rc);

		rc = vreg_disable(vreg_gp2);
		if (rc) {
			pr_err("%s: GP2 disable failed (%d)\n",
				__func__, rc);
		}

		rc = vreg_disable(vreg_gp3);
		if (rc) {
			pr_err("%s: GP3 disable failed (%d)\n",
				__func__, rc);
		}
	}
}

static int config_gpio_table(uint32_t *table, int len)
{
	int rc = 0, i = 0;

	for (i = 0; i < len; i++) {
		rc = gpio_tlmm_config(table[i], GPIO_CFG_ENABLE);
		if (rc) {
			pr_err("%s not able to get gpio\n", __func__);
			for (i--; i >= 0; i--)
				gpio_tlmm_config(camera_off_gpio_table[i],
							GPIO_CFG_ENABLE);
			break;
		}
	}
	return rc;
}

//static struct msm_camera_sensor_info msm_camera_sensor_s5k4e1_data;
//static struct msm_camera_sensor_info msm_camera_sensor_ov9726_data;
#ifdef CONFIG_OV5640
static struct msm_camera_sensor_info msm_camera_sensor_ov5640_data;
#endif
#ifdef CONFIG_MT9D115
static struct msm_camera_sensor_info msm_camera_sensor_mt9d115_data;
#endif
#ifdef CONFIG_S5K5CAGX
static struct msm_camera_sensor_info msm_camera_sensor_s5k5cagx_data;
#endif
#ifdef CONFIG_S5K5CAGX_FF
static struct msm_camera_sensor_info msm_camera_sensor_s5k5cagx_ff_data;
#endif
static int config_camera_on_gpios_rear(void)
{
	int rc = 0;

	if (machine_is_msm7x27a_ffa())
		msm_camera_vreg_config(1);

	rc = config_gpio_table(camera_on_gpio_table,
			ARRAY_SIZE(camera_on_gpio_table));
	if (rc < 0) {
		pr_err("%s: CAMSENSOR gpio table request"
		"failed\n", __func__);
		return rc;
	}

	return rc;
}

static void config_camera_off_gpios_rear(void)
{
	if (machine_is_msm7x27a_ffa())
		msm_camera_vreg_config(0);

	config_gpio_table(camera_off_gpio_table,
			ARRAY_SIZE(camera_off_gpio_table));
}

static int config_camera_on_gpios_front(void)
{
	int rc = 0;

	if (machine_is_msm7x27a_ffa())
		msm_camera_vreg_config(1);

	rc = config_gpio_table(camera_on_gpio_table,
			ARRAY_SIZE(camera_on_gpio_table));
	if (rc < 0) {
		pr_err("%s: CAMSENSOR gpio table request"
			"failed\n", __func__);
		return rc;
	}

	return rc;
}

static void config_camera_off_gpios_front(void)
{
	if (machine_is_msm7x27a_ffa())
		msm_camera_vreg_config(0);

	config_gpio_table(camera_off_gpio_table,
			ARRAY_SIZE(camera_off_gpio_table));
}

#define MSM_CAMERA_POWER_BACKEND_DVDD_VAL       (1800)
#define MSM_CAMERA_POWER_BACKEND_IOVDD_VAL      (1800)
#define MSM_CAMERA_POWER_BACKEND_AVDD_VAL       (2800)
#define MSM_CAMERA_POWER_BACKEND_MOTO_VAL       (2800)
int32_t msm_camera_power_backend(enum msm_camera_pwr_mode_t pwr_mode)
{
    struct vreg *vreg_cam_dvdd  = NULL;
    struct vreg *vreg_cam_avdd  = NULL;
    struct vreg *vreg_cam_iovdd = NULL;
    struct vreg *vreg_cam_moto  = NULL;

    int32_t rc_cam_dvdd, rc_cam_avdd, rc_cam_iovdd, rc_cam_moto;

    CDBG("%s: entry\n", __func__);

    /*
      * Power-up Sequence according to datasheet of sensor:
      *
      * VREG_CAM_DVDD1V8  = VREG_GP2
      * VREG_CAM_IOVDD2V8 = VREG_MSMP
      * VREG_CAM_AVDD2V6  = VREG_GP3
      */
    vreg_cam_dvdd  = vreg_get(0, "msme1");
    vreg_cam_iovdd = vreg_get(0, "msme1");
    vreg_cam_avdd  = vreg_get(0, "wlan4");
    vreg_cam_moto  = vreg_get(0, "emmc");
	
    if ((!vreg_cam_dvdd) || (!vreg_cam_iovdd) || (!vreg_cam_avdd)|| (!vreg_cam_moto))
    {
        pr_err("%s: vreg_get failed!\n", __func__);
        return -EIO;
    }

    switch (pwr_mode)
    {
        case MSM_CAMERA_PWRUP_MODE:
        {
            /* DVDD for both 5.0Mp and 3.0Mp camera on board-blade */
            rc_cam_dvdd = vreg_set_level(vreg_cam_dvdd, MSM_CAMERA_POWER_BACKEND_DVDD_VAL);
            if (rc_cam_dvdd)
            {
                pr_err("%s: vreg_set_level failed!\n", __func__);
                return -EIO;
            }

            rc_cam_dvdd = vreg_enable(vreg_cam_dvdd);
            if (rc_cam_dvdd)
            {
                pr_err("%s: vreg_enable failed!\n", __func__);
                return -EIO;
            }

            mdelay(1);

            rc_cam_iovdd = vreg_set_level(vreg_cam_iovdd, MSM_CAMERA_POWER_BACKEND_IOVDD_VAL);
            if (rc_cam_iovdd)
            {
                pr_err("%s: vreg_set_level failed!\n", __func__);
                return -EIO;
            }

            rc_cam_iovdd = vreg_enable(vreg_cam_iovdd);
            if (rc_cam_iovdd)
            {
                pr_err("%s: vreg_enable failed!\n", __func__);
                return -EIO;
            }

            mdelay(2);

            /*
               * AVDD for both 5.0Mp and 3.0Mp camera on board-blade
               * AVDD and VCM are connected together on board-blade
               */
            rc_cam_avdd = vreg_set_level(vreg_cam_avdd, MSM_CAMERA_POWER_BACKEND_AVDD_VAL);
            if (rc_cam_avdd)
            {
                pr_err("%s: vreg_set_level failed!\n", __func__);
                return -EIO;
            }

            rc_cam_avdd = vreg_enable(vreg_cam_avdd);
            if (rc_cam_avdd)
            {
                pr_err("%s: vreg_enable failed!\n", __func__);
                return -EIO;
            }

            rc_cam_moto = vreg_set_level(vreg_cam_moto, MSM_CAMERA_POWER_BACKEND_MOTO_VAL);
            if (rc_cam_moto)
            {
                pr_err("%s: vreg_set_level failed!\n", __func__);
                return -EIO;
            }

            rc_cam_moto = vreg_enable(vreg_cam_moto);
            if (rc_cam_moto)
            {
                pr_err("%s: vreg_enable failed!\n", __func__);
                return -EIO;
            }
            
            mdelay(500);

            break;
        }
        case MSM_CAMERA_STANDBY_MODE:
        {
            rc_cam_avdd  = vreg_disable(vreg_cam_avdd);
            if (rc_cam_avdd)
            {
                pr_err("%s: vreg_disable failed!\n", __func__);
                return -EIO;
            }

            break;
        }
        case MSM_CAMERA_NORMAL_MODE:
        {
            /*
               * AVDD and VCM are connected together on board-blade
               */
            rc_cam_avdd = vreg_set_level(vreg_cam_avdd, MSM_CAMERA_POWER_BACKEND_AVDD_VAL);
            if (rc_cam_avdd)
            {
                pr_err("%s: vreg_set_level failed!\n", __func__);
                return -EIO;
            }

            rc_cam_avdd = vreg_enable(vreg_cam_avdd);
            if (rc_cam_avdd)
            {
                pr_err("%s: vreg_enable failed!\n", __func__);
                return -EIO;
            }

            mdelay(100);

            break;
        }
        case MSM_CAMERA_PWRDWN_MODE:
        {
            /*
               * Attention: DVDD, AVDD, or MOTORVDD may be used by other devices
               */
            rc_cam_dvdd  = vreg_disable(vreg_cam_dvdd);
            rc_cam_avdd  = vreg_disable(vreg_cam_avdd);
            rc_cam_moto  = vreg_disable(vreg_cam_moto);
	
            if ((rc_cam_dvdd) || (rc_cam_avdd) || (rc_cam_moto))
            {
                pr_err("%s: vreg_disable failed!\n", __func__);
                return -EIO;
            }

            break;
        }
        default:
        {
            pr_err("%s: parameter not supported!\n", __func__);
            return -EIO;
        }
    }

    return 0;
}
struct msm_camera_device_platform_data msm_camera_device_data_rear = {
	.camera_gpio_on  = config_camera_on_gpios_rear,
	.camera_gpio_off = config_camera_off_gpios_rear,
	.ioext.csiphy = 0xA1000000,
	.ioext.csisz  = 0x00100000,
	.ioext.csiirq = INT_CSI_IRQ_1,
	.ioclk.mclk_clk_rate = 24000000,
	.ioclk.vfe_clk_rate  = 192000000,
	.ioext.appphy = MSM_CLK_CTL_PHYS,
	.ioext.appsz  = MSM_CLK_CTL_SIZE,
};

struct msm_camera_device_platform_data msm_camera_device_data_front = {
	.camera_gpio_on  = config_camera_on_gpios_front,
	.camera_gpio_off = config_camera_off_gpios_front,
	.ioext.csiphy = 0xA0F00000,
	.ioext.csisz  = 0x00100000,
	.ioext.csiirq = INT_CSI_IRQ_0,
	.ioclk.mclk_clk_rate = 24000000,
	.ioclk.vfe_clk_rate  = 192000000,
	.ioext.appphy = MSM_CLK_CTL_PHYS,
	.ioext.appsz  = MSM_CLK_CTL_SIZE,
};

#ifdef CONFIG_S5K4E1
static struct msm_camera_sensor_platform_info s5k4e1_sensor_7627a_info = {
	.mount_angle = 90
};

static struct msm_camera_sensor_flash_data flash_s5k4e1 = {
	.flash_type             = MSM_CAMERA_FLASH_LED,
	.flash_src              = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k4e1_data = {
	.sensor_name    = "s5k4e1",
	.sensor_reset_enable = 1,
	.sensor_reset   = GPIO_CAM_GP_CAMIF_RESET_N,
	.sensor_pwd             = 85,
	.vcm_pwd                = GPIO_CAM_GP_CAM_PWDN,
	.vcm_enable             = 1,
	.pdata                  = &msm_camera_device_data_rear,
	.flash_data             = &flash_s5k4e1,
	.sensor_platform_info   = &s5k4e1_sensor_7627a_info,
	.csi_if                 = 1
};

static struct platform_device msm_camera_sensor_s5k4e1 = {
	.name   = "msm_camera_s5k4e1",
	.dev    = {
		.platform_data = &msm_camera_sensor_s5k4e1_data,
	},
};
#endif
#ifdef CONFIG_OV5640
static struct msm_camera_sensor_platform_info ov5640_sensor_7627a_info = {
	.mount_angle = 90
};

static struct msm_camera_sensor_flash_data flash_ov5640 = {
	.flash_type             = MSM_CAMERA_FLASH_LED,
	.flash_src              = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_ov5640_data = {
	.sensor_name    = "ov5640",
	.sensor_reset_enable = 0,
	.sensor_reset   = 27,//GPIO_CAM_GP_CAMIF_RESET_N,
	.sensor_pwd             = 42,
	.vcm_pwd                = 1,//GPIO_CAM_GP_CAM_PWDN,
	.vcm_enable             = 0,
	.pdata                  = &msm_camera_device_data_rear,
	.flash_data             = &flash_ov5640,
	.sensor_platform_info   = &ov5640_sensor_7627a_info,
	.csi_if                 = 1
};

static struct platform_device msm_camera_sensor_ov5640 = {
	.name   = "msm_camera_ov5640",
	.dev    = {
		.platform_data = &msm_camera_sensor_ov5640_data,
	},
};
#endif

#ifdef CONFIG_MT9D115
static struct msm_camera_sensor_platform_info mt9d115_sensor_7627a_info = {
	.mount_angle = 90
};

static struct msm_camera_sensor_flash_data flash_mt9d115 = {
	.flash_type             = MSM_CAMERA_FLASH_NONE,
	.flash_src              = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_mt9d115_data = {
	.sensor_name    = "mt9d115",
	.sensor_reset_enable = 0,
	.sensor_reset   = 27,//GPIO_CAM_GP_CAMIF_RESET_N,
	.sensor_pwd             = 42,
	.vcm_pwd                = 1,//GPIO_CAM_GP_CAM_PWDN,
	.vcm_enable             = 0,
	.pdata                  = &msm_camera_device_data_rear,
	.flash_data             = &flash_mt9d115,
	.sensor_platform_info   = &mt9d115_sensor_7627a_info,
	.csi_if                 = 1
};

static struct platform_device msm_camera_sensor_mt9d115 = {
	.name   = "msm_camera_mt9d115",
	.dev    = {
		.platform_data = &msm_camera_sensor_mt9d115_data,
	},
};
#endif
#ifdef CONFIG_S5K5CAGX
static struct msm_camera_sensor_platform_info s5k5cagx_sensor_7627a_info = {
	.mount_angle = 90
};

static struct msm_camera_sensor_flash_data flash_s5k5cagx = {
	.flash_type             = MSM_CAMERA_FLASH_NONE,
	.flash_src              = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k5cagx_data = {
	.sensor_name    = "s5k5cagx",
	.sensor_reset_enable = 0,
	.sensor_reset   = 49,//GPIO_CAM_GP_CAMIF_RESET_N,
	.sensor_pwd             = 88,
	.vcm_pwd                = 1,//GPIO_CAM_GP_CAM_PWDN,
	.vcm_enable             = 0,
	.pdata                  = &msm_camera_device_data_rear,
	.flash_data             = &flash_s5k5cagx,
	.sensor_platform_info   = &s5k5cagx_sensor_7627a_info,
	.csi_if                 = 1
};

static struct platform_device msm_camera_sensor_s5k5cagx = {
	.name   = "msm_camera_s5k5cagx",
	.dev    = {
		.platform_data = &msm_camera_sensor_s5k5cagx_data,
	},
};
#endif

#ifdef CONFIG_S5K5CAGX_FF
static struct msm_camera_sensor_platform_info s5k5cagx_sensor_7627a_info = {
	.mount_angle = 90
};

static struct msm_camera_sensor_flash_data flash_s5k5cagx = {
	.flash_type             = MSM_CAMERA_FLASH_NONE,
	.flash_src              = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k5cagx_ff_data = {
	.sensor_name    = "s5k5cagx_ff",
	.sensor_reset_enable = 0,
	.sensor_reset   = 49,//GPIO_CAM_GP_CAMIF_RESET_N,
	.sensor_pwd             = 88,
	.vcm_pwd                = 1,//GPIO_CAM_GP_CAM_PWDN,
	.vcm_enable             = 0,
	.pdata                  = &msm_camera_device_data_rear,
	.flash_data             = &flash_s5k5cagx,
	.sensor_platform_info   = &s5k5cagx_sensor_7627a_info,
	.csi_if                 = 1
};

static struct platform_device msm_camera_sensor_s5k5cagx_ff = {
	.name   = "msm_camera_s5k5cagx_ff",
	.dev    = {
		.platform_data = &msm_camera_sensor_s5k5cagx_ff_data,
	},
};
#endif


#ifdef CONFIG_IMX072
static struct msm_camera_sensor_platform_info imx072_sensor_7627a_info = {
	.mount_angle = 90
};

static struct msm_camera_sensor_flash_data flash_imx072 = {
	.flash_type             = MSM_CAMERA_FLASH_LED,
	.flash_src              = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_imx072_data = {
	.sensor_name    = "imx072",
	.sensor_reset_enable = 1,
	.sensor_reset   = GPIO_CAM_GP_CAMIF_RESET_N, /* TODO 106,*/
	.sensor_pwd             = 85,
	.vcm_pwd                = GPIO_CAM_GP_CAM_PWDN,
	.vcm_enable             = 1,
	.pdata                  = &msm_camera_device_data_rear,
	.flash_data             = &flash_imx072,
	.sensor_platform_info = &imx072_sensor_7627a_info,
	.csi_if                 = 1
};

static struct platform_device msm_camera_sensor_imx072 = {
	.name   = "msm_camera_imx072",
	.dev    = {
		.platform_data = &msm_camera_sensor_imx072_data,
	},
};
#endif

#ifdef CONFIG_WEBCAM_OV9726
static struct msm_camera_sensor_platform_info ov9726_sensor_7627a_info = {
	.mount_angle = 90
};

static struct msm_camera_sensor_flash_data flash_ov9726 = {
	.flash_type             = MSM_CAMERA_FLASH_NONE,
	.flash_src              = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_ov9726_data = {
	.sensor_name    = "ov9726",
	.sensor_reset_enable = 0,
	.sensor_reset   = GPIO_CAM_GP_CAM1MP_XCLR,
	.sensor_pwd             = 85,
	.vcm_pwd                = 1,
	.vcm_enable             = 0,
	.pdata                  = &msm_camera_device_data_front,
	.flash_data             = &flash_ov9726,
	.sensor_platform_info   = &ov9726_sensor_7627a_info,
	.csi_if                 = 1
};

static struct platform_device msm_camera_sensor_ov9726 = {
	.name   = "msm_camera_ov9726",
	.dev    = {
		.platform_data = &msm_camera_sensor_ov9726_data,
	},
};
#endif

#ifdef CONFIG_MT9E013
static struct msm_camera_sensor_platform_info mt9e013_sensor_7627a_info = {
	.mount_angle = 90
};

static struct msm_camera_sensor_flash_data flash_mt9e013 = {
	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_src  = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_mt9e013_data = {
	.sensor_name    = "mt9e013",
	.sensor_reset   = 0,
	.sensor_reset_enable = 1,
	.sensor_pwd     = 85,
	.vcm_pwd        = 1,
	.vcm_enable     = 0,
	.pdata          = &msm_camera_device_data_rear,
	.flash_data     = &flash_mt9e013,
	.sensor_platform_info   = &mt9e013_sensor_7627a_info,
	.csi_if         = 1
};

static struct platform_device msm_camera_sensor_mt9e013 = {
	.name      = "msm_camera_mt9e013",
	.dev       = {
		.platform_data = &msm_camera_sensor_mt9e013_data,
	},
};
#endif

static struct i2c_board_info i2c_camera_devices[] = {
	#ifdef CONFIG_S5K4E1
	{
		I2C_BOARD_INFO("s5k4e1", 0x36),
	},
	{
		I2C_BOARD_INFO("s5k4e1_af", 0x8c >> 1),
	},
	#endif
	#ifdef CONFIG_OV5640
	{
		I2C_BOARD_INFO("ov5640", 0x78 >> 1),
	},
	#endif
	#ifdef CONFIG_MT9D115
	#if defined(CONFIG_CAMERA_ADAPTER)
	{
		I2C_BOARD_INFO("mt9d115", 0x78 >> 2),
	},
	#else
	{
		I2C_BOARD_INFO("mt9d115", 0x78 >> 1),
	},
	#endif
	#endif
	#ifdef CONFIG_S5K5CAGX
	{
		I2C_BOARD_INFO("s5k5cagx", 0x5A >> 1),
	},
	#endif
    #ifdef CONFIG_S5K5CAGX_FF
	{
		I2C_BOARD_INFO("s5k5cagx_ff", 0x5A >> 1),
	},
	#endif
	#ifdef CONFIG_WEBCAM_OV9726
	{
		I2C_BOARD_INFO("ov9726", 0x10),
	},
	#endif
	#ifdef CONFIG_IMX072
	{
		I2C_BOARD_INFO("imx072", 0x34),
	},
	#endif
	#ifdef CONFIG_MT9E013
	{
		I2C_BOARD_INFO("mt9e013", 0x6C >> 2),
	},
	#endif
	{
		I2C_BOARD_INFO("sc628a", 0x37),
	},
};
#endif
#if defined(CONFIG_SERIAL_MSM_HSL_CONSOLE) \
		&& defined(CONFIG_MSM_SHARED_GPIO_FOR_UART2DM)
static struct msm_gpio uart2dm_gpios[] = {
	{GPIO_CFG(19, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
							"uart2dm_rfr_n" },
	{GPIO_CFG(20, 2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
							"uart2dm_cts_n" },
	{GPIO_CFG(21, 2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
							"uart2dm_rx"    },
	{GPIO_CFG(108, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
							"uart2dm_tx"    },
};

static void msm7x27a_cfg_uart2dm_serial(void)
{
	int ret;
	ret = msm_gpios_request_enable(uart2dm_gpios,
					ARRAY_SIZE(uart2dm_gpios));
	if (ret)
		pr_err("%s: unable to enable gpios for uart2dm\n", __func__);
}
#else
static void msm7x27a_cfg_uart2dm_serial(void) { }
#endif

static struct platform_device *rumi_sim_devices[] __initdata = {
	&msm_device_dmov,
	&msm_device_smd,
	&smc91x_device,
	&msm_device_uart1,
	&msm_device_nand,
	&msm_device_uart_dm1,
	&msm_gsbi0_qup_i2c_device,
	&msm_gsbi1_qup_i2c_device,
};

#ifdef CONFIG_ZTE_PLATFORM
#ifdef CONFIG_ANDROID_RAM_CONSOLE
static struct resource ram_console_resource[] = {
	{
		.start	= MSM_RAM_CONSOLE_PHYS,
		.end	= MSM_RAM_CONSOLE_PHYS + MSM_RAM_CONSOLE_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ram_console_device = {
	.name = "ram_console",
	.id = -1,
	.num_resources  = ARRAY_SIZE(ram_console_resource),
	.resource       = ram_console_resource,
};
#endif
#endif

#ifdef CONFIG_ZTE_PLATFORM
static struct gpio_led android_led_list[] = {
	{
		.name = "button-backlight",
		.gpio = 85,
	},
};

static struct gpio_led_platform_data android_leds_data = {
	.num_leds	= ARRAY_SIZE(android_led_list),
	.leds		= android_led_list,
};

static struct platform_device android_leds = {
	.name		= "leds-gpio",
	.id		= -1,
	.dev		= {
		.platform_data = &android_leds_data,
	},
};
#endif

#ifdef CONFIG_ATH_WIFI
static struct platform_device msm_wlan_ar6000_pm_device = {
        .name           = "wlan_ar6000_pm_dev",
        .id             = 1,
        .num_resources  = 0,
        .resource       = NULL,
};
#endif

static struct platform_device *surf_ffa_devices[] __initdata = {
	&msm_device_dmov,
	&msm_device_smd,
	&msm_device_uart1,
	&msm_device_uart_dm1,
	&msm_device_uart_dm2,
	&msm_device_nand,
	&msm_gsbi0_qup_i2c_device,
	&msm_gsbi1_qup_i2c_device,
	&msm_device_otg,
	&msm_device_gadget_peripheral,
#ifdef CONFIG_USB_G_ANDROID
	&android_usb_device,
#endif
	&android_pmem_device,
	&android_pmem_adsp_device,
	&usb_gadget_fserial_device,
	&android_pmem_audio_device,
  #ifdef CONFIG_BT 
	//&msm_bt_power_device,
	&msm_bcm_power_device, 
	&msm_bcmsleep_device,
	&msm_bt_power_device,
#endif
	&msm_device_snd,
	&msm_device_adspdec,
	&msm_fb_device,
	&lcdc_toshiba_panel_device,
	&msm_batt_device,
	&smsc911x_device,
#ifdef CONFIG_S5K4E1
	&msm_camera_sensor_s5k4e1,
#endif
#ifdef CONFIG_OV5640
	&msm_camera_sensor_ov5640,	
#endif
#ifdef CONFIG_MT9D115
	&msm_camera_sensor_mt9d115,	
#endif
#ifdef CONFIG_S5K5CAGX
	&msm_camera_sensor_s5k5cagx,
#endif
#ifdef CONFIG_S5K5CAGX_FF
	&msm_camera_sensor_s5k5cagx_ff,
#endif
#ifdef CONFIG_IMX072
	&msm_camera_sensor_imx072,
#endif
	//&msm_bluesleep_device,
	//&msm_bcmsleep_device,    
#ifdef CONFIG_WEBCAM_OV9726
	&msm_camera_sensor_ov9726,
#endif
#ifdef CONFIG_MT9E013
	&msm_camera_sensor_mt9e013,
#endif
#ifdef CONFIG_FB_MSM_MIPI_DSI
	&mipi_dsi_renesas_panel_device,
#endif
#ifdef CONFIG_MSM_V4L2_VIDEO_OVERLAY_DEVICE
	&msm_v4l2_video_overlay_device,
#endif
	&msm_kgsl_3d0,
//#ifdef CONFIG_BT
//	&msm_bt_power_device,
//#endif
#ifdef CONFIG_LEDS_MSM_PMIC
  &msm_device_pmic_leds,
#endif
#ifdef CONFIG_ZTE_PLATFORM
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	&ram_console_device,
#endif
	&android_leds,
#endif

};

static unsigned pmem_kernel_ebi1_size = PMEM_KERNEL_EBI1_SIZE;
static int __init pmem_kernel_ebi1_size_setup(char *p)
{
	pmem_kernel_ebi1_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_kernel_ebi1_size", pmem_kernel_ebi1_size_setup);

static unsigned pmem_audio_size = MSM_PMEM_AUDIO_SIZE;
static int __init pmem_audio_size_setup(char *p)
{
	pmem_audio_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_audio_size", pmem_audio_size_setup);

static void __init msm_msm7x2x_allocate_memory_regions(void)
{
	void *addr;
	unsigned long size;

#if defined(CONFIG_ZTE_PLATFORM) && defined(CONFIG_F3_LOG)
      unsigned int len;
      smem_global *global_tmp = (smem_global *)(MSM_RAM_LOG_BASE + PAGE_SIZE) ;
  
      len = global_tmp->f3log;
#endif



	if (machine_is_msm7625a_surf() || machine_is_msm7625a_ffa())
		fb_size = MSM7x25A_MSM_FB_SIZE;
	else
		fb_size = MSM_FB_SIZE;

	size = fb_size;
	addr = alloc_bootmem_align(size, 0x1000);
	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for fb\n",
		size, addr, __pa(addr));

#ifdef CONFIG_MSM_V4L2_VIDEO_OVERLAY_DEVICE
	fb_size = MSM_V4L2_VIDEO_OVERLAY_BUF_SIZE;
	addr = alloc_bootmem_align(fb_size, 0x1000);
	msm_v4l2_video_overlay_resources[0].start = __pa(addr);
	msm_v4l2_video_overlay_resources[0].end =
		msm_v4l2_video_overlay_resources[0].start + fb_size - 1;
	pr_debug("allocating %lu bytes at %p (%lx physical) for v4l2\n",
		fb_size, addr, __pa(addr));
#endif


#if defined(CONFIG_ZTE_PLATFORM) && defined(CONFIG_F3_LOG)
	pr_info("length = %d ++ \n", len);

	if (len > 12)
		len = 12;
	else
		len = len/2*2;
    
    pr_info("length = %d -- \n", len);
    size = len;
    
	if (size)
		reserve_bootmem(MSM_SDLOG_RAM_PHYS, size*MSM_SDLOG_RAM_SIZE, BOOTMEM_DEFAULT);

	addr = phys_to_virt(MSM_SDLOG_RAM_PHYS);
	pr_info("allocating %lu M at %p (%lx physical) for F3\n",size, addr, __pa(addr));
#endif 

  
}

static struct memtype_reserve msm7x27a_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static void __init size_pmem_devices(void)
{

	if (machine_is_msm7625a_surf() || machine_is_msm7625a_ffa()) {
		pmem_mdp_size = MSM7x25A_MSM_PMEM_MDP_SIZE;
		pmem_adsp_size = MSM7x25A_MSM_PMEM_ADSP_SIZE;
	} else {
		pmem_mdp_size = MSM_PMEM_MDP_SIZE;
		pmem_adsp_size = MSM_PMEM_ADSP_SIZE;
	}

#ifdef CONFIG_ANDROID_PMEM
	android_pmem_adsp_pdata.size = pmem_adsp_size;
	android_pmem_pdata.size = pmem_mdp_size;
	android_pmem_audio_pdata.size = pmem_audio_size;
#endif
}

static void __init reserve_memory_for(struct android_pmem_platform_data *p)
{
	msm7x27a_reserve_table[p->memory_type].size += p->size;
}

static void __init reserve_pmem_memory(void)
{
#ifdef CONFIG_ANDROID_PMEM
	reserve_memory_for(&android_pmem_adsp_pdata);
	reserve_memory_for(&android_pmem_pdata);
	reserve_memory_for(&android_pmem_audio_pdata);
	msm7x27a_reserve_table[MEMTYPE_EBI1].size += pmem_kernel_ebi1_size;
#endif
}

static void __init msm7x27a_calculate_reserve_sizes(void)
{
	size_pmem_devices();
	reserve_pmem_memory();
}

static int msm7x27a_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static struct reserve_info msm7x27a_reserve_info __initdata = {
	.memtype_reserve_table = msm7x27a_reserve_table,
	.calculate_reserve_sizes = msm7x27a_calculate_reserve_sizes,
	.paddr_to_memtype = msm7x27a_paddr_to_memtype,
};

static void __init msm7x27a_reserve(void)
{
	reserve_info = &msm7x27a_reserve_info;
	msm_reserve();
}

static void __init msm_device_i2c_init(void)
{
	msm_gsbi0_qup_i2c_device.dev.platform_data = &msm_gsbi0_qup_i2c_pdata;
	msm_gsbi1_qup_i2c_device.dev.platform_data = &msm_gsbi1_qup_i2c_pdata;
}

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = 97,
	.mdp_rev = MDP_REV_303,
};

#define GPIO_LCDC_BRDG_PD	128
#define GPIO_LCDC_BRDG_RESET_N	129

#define LCDC_RESET_PHYS		0x90008014
static	void __iomem *lcdc_reset_ptr;

static unsigned mipi_dsi_gpio[] = {
	GPIO_CFG(GPIO_LCDC_BRDG_RESET_N, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
		GPIO_CFG_2MA),       /* LCDC_BRDG_RESET_N */
	GPIO_CFG(GPIO_LCDC_BRDG_PD, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
		GPIO_CFG_2MA),       /* LCDC_BRDG_RESET_N */
};

enum {
	DSI_SINGLE_LANE = 1,
	DSI_TWO_LANES,
};

static int msm_fb_get_lane_config(void)
{
	int rc = DSI_TWO_LANES;

	if (cpu_is_msm7x25a() || cpu_is_msm7x25aa()) {
		rc = DSI_SINGLE_LANE;
		pr_info("DSI Single Lane\n");
	} else {
		pr_info("DSI Two Lanes\n");
	}
	return rc;
}

static int msm_fb_dsi_client_reset(void)
{
	int rc = 0;

	rc = gpio_request(GPIO_LCDC_BRDG_RESET_N, "lcdc_brdg_reset_n");
	if (rc < 0) {
		pr_err("failed to request lcd brdg reset_n\n");
		return rc;
	}

	rc = gpio_request(GPIO_LCDC_BRDG_PD, "lcdc_brdg_pd");
	if (rc < 0) {
		pr_err("failed to request lcd brdg pd\n");
		return rc;
	}

	rc = gpio_tlmm_config(mipi_dsi_gpio[0], GPIO_CFG_ENABLE);
	if (rc) {
		pr_err("Failed to enable LCDC Bridge reset enable\n");
		goto gpio_error;
	}

	rc = gpio_tlmm_config(mipi_dsi_gpio[1], GPIO_CFG_ENABLE);
	if (rc) {
		pr_err("Failed to enable LCDC Bridge pd enable\n");
		goto gpio_error2;
	}

	rc = gpio_direction_output(GPIO_LCDC_BRDG_RESET_N, 1);
	rc |= gpio_direction_output(GPIO_LCDC_BRDG_PD, 1);
	gpio_set_value_cansleep(GPIO_LCDC_BRDG_PD, 0);

	if (!rc) {
		if (1/*machine_is_msm7x27a_surf()*/) {
			lcdc_reset_ptr = ioremap_nocache(LCDC_RESET_PHYS,
				sizeof(uint32_t));

			if (!lcdc_reset_ptr)
				return 0;
		}
		return rc;
	} else {
		goto gpio_error;
	}

gpio_error2:
	pr_err("Failed GPIO bridge pd\n");
	gpio_free(GPIO_LCDC_BRDG_PD);

gpio_error:
	pr_err("Failed GPIO bridge reset\n");
	gpio_free(GPIO_LCDC_BRDG_RESET_N);
	return rc;
}

static const char * const msm_fb_dsi_vreg[] = {
	"gp2",
	"msme1",
};

static const int msm_fb_dsi_vreg_mV[] = {
	2850,
	1800,
};

static struct vreg *dsi_vreg[ARRAY_SIZE(msm_fb_dsi_vreg)];
static int dsi_gpio_initialized;

static int mipi_dsi_panel_power(int on)
{
	int i, rc = 0;
	uint32_t lcdc_reset_cfg;

	/* I2C-controlled GPIO Expander -init of the GPIOs very late */
	if (!dsi_gpio_initialized) {
		pmapp_disp_backlight_init();

		rc = gpio_request(GPIO_DISPLAY_PWR_EN, "gpio_disp_pwr");
		if (rc < 0) {
			pr_err("failed to request gpio_disp_pwr\n");
			return rc;
		}

		if (1/*machine_is_msm7x27a_surf()*/) {
			rc = gpio_direction_output(GPIO_DISPLAY_PWR_EN, 1);
			if (rc < 0) {
				pr_err("failed to enable display pwr\n");
				goto fail_gpio1;
			}

			rc = gpio_request(GPIO_BACKLIGHT_EN, "gpio_bkl_en");
			if (rc < 0) {
				pr_err("failed to request gpio_bkl_en\n");
				goto fail_gpio1;
			}

			rc = gpio_direction_output(GPIO_BACKLIGHT_EN, 1);
			if (rc < 0) {
				pr_err("failed to enable backlight\n");
				goto fail_gpio2;
			}
		}

		for (i = 0; i < ARRAY_SIZE(msm_fb_dsi_vreg); i++) {
			dsi_vreg[i] = vreg_get(0, msm_fb_dsi_vreg[i]);

			if (IS_ERR(dsi_vreg[i])) {
				pr_err("%s: vreg get failed with : (%ld)\n",
					__func__, PTR_ERR(dsi_vreg[i]));
				goto fail_gpio2;
			}

			rc = vreg_set_level(dsi_vreg[i],
				msm_fb_dsi_vreg_mV[i]);

			if (rc < 0) {
				pr_err("%s: set regulator level failed "
					"with :(%d)\n",	__func__, rc);
				goto vreg_fail1;
			}
		}
		dsi_gpio_initialized = 1;
	}

		if (1/*machine_is_msm7x27a_surf()*/) {
			gpio_set_value_cansleep(GPIO_DISPLAY_PWR_EN, on);
			gpio_set_value_cansleep(GPIO_BACKLIGHT_EN, on);
		} else if (machine_is_msm7x27a_ffa()) {
			if (on) {
				/* This line drives an active low pin on FFA */
				rc = gpio_direction_output(GPIO_DISPLAY_PWR_EN,
					!on);
				if (rc < 0)
					pr_err("failed to set direction for "
						"display pwr\n");
			} else {
				gpio_set_value_cansleep(GPIO_DISPLAY_PWR_EN,
					!on);
				rc = gpio_direction_input(GPIO_DISPLAY_PWR_EN);
				if (rc < 0)
					pr_err("failed to set direction for "
						"display pwr\n");
			}
		}

		if (on) {
			gpio_set_value_cansleep(GPIO_LCDC_BRDG_PD, 0);

			if (1/*machine_is_msm7x27a_surf()*/) {
				lcdc_reset_cfg = readl_relaxed(lcdc_reset_ptr);
				rmb();
				lcdc_reset_cfg &= ~1;

				writel_relaxed(lcdc_reset_cfg, lcdc_reset_ptr);
				msleep(20);
				wmb();
				lcdc_reset_cfg |= 1;
				writel_relaxed(lcdc_reset_cfg, lcdc_reset_ptr);
			} else {
				gpio_set_value_cansleep(GPIO_LCDC_BRDG_RESET_N,
					0);
				msleep(20);
				gpio_set_value_cansleep(GPIO_LCDC_BRDG_RESET_N,
					1);
			}

			if (pmapp_disp_backlight_set_brightness(100))
				pr_err("backlight set brightness failed\n");
		} else {
			gpio_set_value_cansleep(GPIO_LCDC_BRDG_PD, 1);

			if (pmapp_disp_backlight_set_brightness(0))
				pr_err("backlight set brightness failed\n");
		}

		/*Configure vreg lines */
		for (i = 0; i < ARRAY_SIZE(msm_fb_dsi_vreg); i++) {
			if (on) {
				rc = vreg_enable(dsi_vreg[i]);

				if (rc) {
					printk(KERN_ERR "vreg_enable: %s vreg"
						"operation failed\n",
						msm_fb_dsi_vreg[i]);

					goto vreg_fail2;
				}
			} else {
			/* sensor && lcd used together, do not to shut it down alone.we need to check it later!
				rc = vreg_disable(dsi_vreg[i]);

				if (rc) {
					printk(KERN_ERR "vreg_disable: %s vreg "
						"operation failed\n",
						msm_fb_dsi_vreg[i]);
					goto vreg_fail2;
				}
			*/
			}
		}

	return rc;

vreg_fail2:
	if (on) {
		for (; i > 0; i--)
			vreg_disable(dsi_vreg[i - 1]);
	} else {
		for (; i > 0; i--)
			vreg_enable(dsi_vreg[i - 1]);
	}

	return rc;

vreg_fail1:
	for (; i > 0; i--)
		vreg_put(dsi_vreg[i - 1]);

fail_gpio2:
	gpio_free(GPIO_BACKLIGHT_EN);
fail_gpio1:
	gpio_free(GPIO_DISPLAY_PWR_EN);
	dsi_gpio_initialized = 0;
	return rc;
}

#define MDP_303_VSYNC_GPIO 97

#ifdef CONFIG_FB_MSM_MDP303
static struct mipi_dsi_platform_data mipi_dsi_pdata = {
	.vsync_gpio = MDP_303_VSYNC_GPIO,
	.dsi_power_save   = mipi_dsi_panel_power,
	.dsi_client_reset = msm_fb_dsi_client_reset,
	.get_lane_config = msm_fb_get_lane_config,
};
#endif

static void __init msm_fb_add_devices(void)
{
	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("lcdc", &lcdc_pdata);
	msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);
}

#define MSM_EBI2_PHYS			0xa0d00000
#define MSM_EBI2_XMEM_CS2_CFG1		0xa0d10030

static void __init msm7x27a_init_ebi2(void)
{
	uint32_t ebi2_cfg;
	void __iomem *ebi2_cfg_ptr;

	ebi2_cfg_ptr = ioremap_nocache(MSM_EBI2_PHYS, sizeof(uint32_t));
	if (!ebi2_cfg_ptr)
		return;

	ebi2_cfg = readl(ebi2_cfg_ptr);
	if (machine_is_msm7x27a_rumi3() || 1/*machine_is_msm7x27a_surf()*/)
		ebi2_cfg |= (1 << 4); /* CS2 */

	writel(ebi2_cfg, ebi2_cfg_ptr);
	iounmap(ebi2_cfg_ptr);

	/* Enable A/D MUX[bit 31] from EBI2_XMEM_CS2_CFG1 */
	ebi2_cfg_ptr = ioremap_nocache(MSM_EBI2_XMEM_CS2_CFG1,
							 sizeof(uint32_t));
	if (!ebi2_cfg_ptr)
		return;

	ebi2_cfg = readl(ebi2_cfg_ptr);
	if (1/*machine_is_msm7x27a_surf()*/)
		ebi2_cfg |= (1 << 31);

	writel(ebi2_cfg, ebi2_cfg_ptr);
	iounmap(ebi2_cfg_ptr);
}
#ifdef CONFIG_TOUCHSCREEN_ATMEL_MAXTOUCH
#define ATMEL_TS_I2C_NAME "maXTouch"
static struct vreg *vreg_l12;
static struct vreg *vreg_s3;

#define ATMEL_TS_GPIO_IRQ 82

static int atmel_ts_power_on(bool on)
{
	int rc;

	rc = on ? vreg_enable(vreg_l12) : vreg_disable(vreg_l12);
	if (rc) {
		pr_err("%s: vreg %sable failed (%d)\n",
		       __func__, on ? "en" : "dis", rc);
		return rc;
	}

	rc = on ? vreg_enable(vreg_s3) : vreg_disable(vreg_s3);
	if (rc) {
		pr_err("%s: vreg %sable failed (%d) for S3\n",
		       __func__, on ? "en" : "dis", rc);
		!on ? vreg_enable(vreg_l12) : vreg_disable(vreg_l12);
		return rc;
	}
	/* vreg stabilization delay */
	msleep(50);
	return 0;
}

static int atmel_ts_platform_init(struct i2c_client *client)
{
	int rc;

	vreg_l12 = vreg_get(NULL, "gp2");
	if (IS_ERR(vreg_l12)) {
		pr_err("%s: vreg_get for L2 failed\n", __func__);
		return PTR_ERR(vreg_l12);
	}

	rc = vreg_set_level(vreg_l12, 2850);
	if (rc) {
		pr_err("%s: vreg set level failed (%d) for l2\n",
		       __func__, rc);
		goto vreg_put_l2;
	}

	vreg_s3 = vreg_get(NULL, "msme1");
	if (IS_ERR(vreg_s3)) {
		pr_err("%s: vreg_get for S3 failed\n", __func__);
		rc = PTR_ERR(vreg_s3);
		goto vreg_put_l2;
	}

	rc = vreg_set_level(vreg_s3, 1800);
	if (rc) {
		pr_err("%s: vreg set level failed (%d) for S3\n",
		       __func__, rc);
		goto vreg_put_s3;
	}

	rc = gpio_tlmm_config(GPIO_CFG(ATMEL_TS_GPIO_IRQ, 0,
				GPIO_CFG_INPUT, GPIO_CFG_PULL_UP,
				GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	if (rc) {
		pr_err("%s: gpio_tlmm_config for %d failed\n",
			__func__, ATMEL_TS_GPIO_IRQ);
		goto vreg_put_s3;
	}

	/* configure touchscreen interrupt gpio */
	rc = gpio_request(ATMEL_TS_GPIO_IRQ, "atmel_maxtouch_gpio");
	if (rc) {
		pr_err("%s: unable to request gpio %d\n",
			__func__, ATMEL_TS_GPIO_IRQ);
		goto ts_gpio_tlmm_unconfig;
	}

	rc = gpio_direction_input(ATMEL_TS_GPIO_IRQ);
	if (rc < 0) {
		pr_err("%s: unable to set the direction of gpio %d\n",
			__func__, ATMEL_TS_GPIO_IRQ);
		goto free_ts_gpio;
	}
	return 0;

free_ts_gpio:
	gpio_free(ATMEL_TS_GPIO_IRQ);
ts_gpio_tlmm_unconfig:
	gpio_tlmm_config(GPIO_CFG(ATMEL_TS_GPIO_IRQ, 0,
				GPIO_CFG_INPUT, GPIO_CFG_NO_PULL,
				GPIO_CFG_2MA), GPIO_CFG_DISABLE);
vreg_put_s3:
	vreg_put(vreg_s3);
vreg_put_l2:
	vreg_put(vreg_l12);
	return rc;
}

static int atmel_ts_platform_exit(struct i2c_client *client)
{
	gpio_free(ATMEL_TS_GPIO_IRQ);
	gpio_tlmm_config(GPIO_CFG(ATMEL_TS_GPIO_IRQ, 0,
				GPIO_CFG_INPUT, GPIO_CFG_NO_PULL,
				GPIO_CFG_2MA), GPIO_CFG_DISABLE);
	vreg_disable(vreg_s3);
	vreg_put(vreg_s3);
	vreg_disable(vreg_l12);
	vreg_put(vreg_l12);
	return 0;
}

static u8 atmel_ts_read_chg(void)
{
	return gpio_get_value(ATMEL_TS_GPIO_IRQ);
}

static u8 atmel_ts_valid_interrupt(void)
{
	return !atmel_ts_read_chg();
}

#define ATMEL_X_OFFSET 13
#define ATMEL_Y_OFFSET 0

static struct mxt_platform_data atmel_ts_pdata = {
	.numtouch = 4,
	.init_platform_hw = atmel_ts_platform_init,
	.exit_platform_hw = atmel_ts_platform_exit,
	.power_on = atmel_ts_power_on,
	.display_res_x = 480,
	.display_res_y = 864,
	.min_x = ATMEL_X_OFFSET,
	.max_x = (505 - ATMEL_X_OFFSET),
	.min_y = ATMEL_Y_OFFSET,
	.max_y = (863 - ATMEL_Y_OFFSET),
	.valid_interrupt = atmel_ts_valid_interrupt,
	.read_chg = atmel_ts_read_chg,
};

static struct i2c_board_info atmel_ts_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO(ATMEL_TS_I2C_NAME, 0x4a),
		.platform_data = &atmel_ts_pdata,
		.irq = MSM_GPIO_TO_INT(ATMEL_TS_GPIO_IRQ),
	},
};
#endif

#ifdef CONFIG_MACH_TURIES
#define KP_INDEX(row, col) ((row)*ARRAY_SIZE(kp_col_gpios) + (col))

#ifdef CONFIG_INPUT_GPIO_P752P
static unsigned int kp_row_gpios[] = {31,32,33,34,35,26,83,90,13,14};
static unsigned int kp_col_gpios[] = {42,41,40,39,38,37,36};


static const unsigned short keymap[ARRAY_SIZE(kp_col_gpios) *
					  ARRAY_SIZE(kp_row_gpios)] = {

	/*                       row, col   */
	[KP_INDEX(0, 0)] = KEY_FN, 
	[KP_INDEX(0, 1)] = KEY_RESERVED, 
	[KP_INDEX(0, 2)] = KEY_MENU,
	[KP_INDEX(0, 3)] = KEY_RESERVED, 
	[KP_INDEX(0, 4)] = KEY_RESERVED, 
	[KP_INDEX(0, 5)] = KEY_VOLUMEDOWN, 
	[KP_INDEX(0, 6)] = KEY_VOLUMEUP, 
	
	[KP_INDEX(1, 0)] = KEY_RESERVED, 
	[KP_INDEX(1, 1)] = KEY_Y, 
	[KP_INDEX(1, 2)] = KEY_RESERVED,
	[KP_INDEX(1, 3)] = KEY_U, 
	[KP_INDEX(1, 4)] = KEY_I, 
	[KP_INDEX(1, 5)] = KEY_O, 
	[KP_INDEX(1, 6)] = KEY_P, 
	
	[KP_INDEX(2, 0)] = KEY_RESERVED, 
	[KP_INDEX(2, 1)] = KEY_W, 
	[KP_INDEX(2, 2)] = KEY_RESERVED,
	[KP_INDEX(2, 3)] = KEY_X, 
	[KP_INDEX(2, 4)] = KEY_C, 
	[KP_INDEX(2, 5)] = KEY_V, 
	[KP_INDEX(2, 6)] = KEY_B, 
	
	[KP_INDEX(3, 0)] = KEY_RESERVED, 
	[KP_INDEX(3, 1)] = KEY_Q, 
	[KP_INDEX(3, 2)] = KEY_RESERVED,
	[KP_INDEX(3, 3)] = KEY_S, 
	[KP_INDEX(3, 4)] = KEY_D, 
	[KP_INDEX(3, 5)] = KEY_F, 
	[KP_INDEX(3, 6)] = KEY_G,	

	[KP_INDEX(4, 0)] = KEY_RESERVED, 
	[KP_INDEX(4, 1)] = KEY_A, 
	[KP_INDEX(4, 2)] = KEY_RESERVED,
	[KP_INDEX(4, 3)] = KEY_Z, 
	[KP_INDEX(4, 4)] = KEY_E, 
	[KP_INDEX(4, 5)] = KEY_R, 
	[KP_INDEX(4, 6)] = KEY_T, 

	[KP_INDEX(5, 0)] = KEY_RESERVED, 
	[KP_INDEX(5, 1)] = KEY_N, 
	[KP_INDEX(5, 2)] = KEY_RESERVED,
	[KP_INDEX(5, 3)] = KEY_M, 
	[KP_INDEX(5, 4)] = KEY_COMMA, 
	[KP_INDEX(5, 5)] = KEY_ENTER, 
	[KP_INDEX(5, 6)] = KEY_SEND, 

	[KP_INDEX(6, 0)] = KEY_RESERVED, 
	[KP_INDEX(6, 1)] = KEY_RESERVED, 
	[KP_INDEX(6, 2)] = KEY_RESERVED,
	[KP_INDEX(6, 3)] = KEY_END,  
	[KP_INDEX(6, 4)] = KEY_HOME, 
	[KP_INDEX(6, 5)] = KEY_RESERVED, 
	[KP_INDEX(6, 6)] = KEY_BACK, 

	[KP_INDEX(7, 0)] = KEY_RESERVED, 
	[KP_INDEX(7, 1)] = KEY_OK,   
	[KP_INDEX(7, 2)] = KEY_RESERVED,
	[KP_INDEX(7, 3)] = KEY_RIGHT,  
	[KP_INDEX(7, 4)] = KEY_DOWN, 
	[KP_INDEX(7, 5)] = KEY_UP, 
	[KP_INDEX(7, 6)] = KEY_LEFT, 

	[KP_INDEX(8, 0)] = KEY_LEFTSHIFT, 
	[KP_INDEX(8, 1)] = KEY_BACKSPACE,  
	[KP_INDEX(8, 2)] = KEY_SEARCH, 
	[KP_INDEX(8, 3)] = KEY_SPACE, 
	[KP_INDEX(8, 4)] = KEY_0, 
	[KP_INDEX(8, 5)] = KEY_COMPOSE,  
	[KP_INDEX(8, 6)] = KEY_CAMERA,

	[KP_INDEX(9, 0)] = KEY_MUTE, 
	[KP_INDEX(9, 1)] = KEY_H, 
	[KP_INDEX(9, 2)] = KEY_RESERVED,
	[KP_INDEX(9, 3)] = KEY_J, 
	[KP_INDEX(9, 4)] = KEY_K, 
	[KP_INDEX(9, 5)] = KEY_L, 
	[KP_INDEX(9, 6)] = KEY_DOT,

};


#else
static unsigned int kp_row_gpios[] = {31,32,33,34,35,26,83};
static unsigned int kp_col_gpios[] = {36, 37,38,39,40,41,42};


static const unsigned short keymap[ARRAY_SIZE(kp_col_gpios) *
					  ARRAY_SIZE(kp_row_gpios)] = {
	/*                       row, col   */
	[KP_INDEX(0, 0)] = KEY_VOLUMEUP, 
	[KP_INDEX(0, 1)] = KEY_VOLUMEDOWN, 
	[KP_INDEX(0, 2)] = KEY_F3,
	[KP_INDEX(0, 3)] = KEY_J, 
	[KP_INDEX(0, 4)] = KEY_U, 
	[KP_INDEX(0, 5)] = KEY_RESERVED, 
	[KP_INDEX(0, 6)] = KEY_RIGHT, 

	[KP_INDEX(1, 0)] = KEY_L, 
	[KP_INDEX(1, 1)] = KEY_C, 
	[KP_INDEX(1, 2)] = KEY_COMMA,
	[KP_INDEX(1, 3)] = KEY_F, 
	[KP_INDEX(1, 4)] = KEY_R, 
	[KP_INDEX(1, 5)] = KEY_K, 
	[KP_INDEX(1, 6)] = KEY_UP, 	

	[KP_INDEX(2, 0)] = KEY_BACKSPACE, 
	[KP_INDEX(2, 1)] = KEY_X, 
	[KP_INDEX(2, 2)] = KEY_FN,
	[KP_INDEX(2, 3)] = KEY_D, 
	[KP_INDEX(2, 4)] = KEY_E, 
	[KP_INDEX(2, 5)] = KEY_P, 
	[KP_INDEX(2, 6)] = KEY_LEFT, 

	[KP_INDEX(3, 0)] = KEY_O, 
	[KP_INDEX(3, 1)] = KEY_Z, 
	[KP_INDEX(3, 2)] = KEY_CAMERA,
	[KP_INDEX(3, 3)] = KEY_S, 
	[KP_INDEX(3, 4)] = KEY_W, 
	[KP_INDEX(3, 5)] = KEY_I, 
	[KP_INDEX(3, 6)] = KEY_MENU, 

	[KP_INDEX(4, 0)] = KEY_BACK, 
	[KP_INDEX(4, 1)] = KEY_COMPOSE, 
	[KP_INDEX(4, 2)] = KEY_LEFTSHIFT,
	[KP_INDEX(4, 3)] = KEY_A, 
	[KP_INDEX(4, 4)] = KEY_Q, 
	[KP_INDEX(4, 5)] = KEY_SEARCH, 
	[KP_INDEX(4, 6)] = KEY_HOME,

	[KP_INDEX(5, 0)] = KEY_ENTER, 
	[KP_INDEX(5, 1)] = KEY_V, 
	[KP_INDEX(5, 2)] = KEY_SPACE,
	[KP_INDEX(5, 3)] = KEY_G, 
	[KP_INDEX(5, 4)] = KEY_T, 
	[KP_INDEX(5, 5)] = KEY_F2,
	[KP_INDEX(5, 6)] = KEY_OK,

	[KP_INDEX(6, 0)] = KEY_M, 
	[KP_INDEX(6, 1)] = KEY_B, 
	[KP_INDEX(6, 2)] = KEY_DOT,
	[KP_INDEX(6, 3)] = KEY_H,  
	[KP_INDEX(6, 4)] = KEY_Y, 
	[KP_INDEX(6, 5)] = KEY_N, 
	[KP_INDEX(6, 6)] = KEY_DOWN, 

};
#endif
/* SURF keypad platform device information */
static struct gpio_event_matrix_info kp_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keymap,
	.output_gpios	= kp_row_gpios,
	.input_gpios	= kp_col_gpios,
	.noutputs	= ARRAY_SIZE(kp_row_gpios),
	.ninputs	= ARRAY_SIZE(kp_col_gpios),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,//row scan interval time
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.debounce_delay.tv.nsec = 10 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_DRIVE_INACTIVE |
			  GPIOKPF_PRINT_UNMAPPED_KEYS|GPIOKPF_DEBOUNCE,
};

static struct gpio_event_info *kp_info[] = {
	&kp_matrix_info.info
};

static struct gpio_event_platform_data kp_pdata = {
	.name		= "turies_keypad",
	.info		= kp_info,
	.info_count	= ARRAY_SIZE(kp_info)
};

static struct platform_device kp_pdev = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &kp_pdata,
	},
};

#else

#define KP_INDEX(row, col) ((row)*ARRAY_SIZE(kp_col_gpios) + (col))

static unsigned int kp_row_gpios[] = {31, 32};
static unsigned int kp_col_gpios[] = {36, 37, 38};

static const unsigned short keymap[ARRAY_SIZE(kp_col_gpios) *
					  ARRAY_SIZE(kp_row_gpios)] = {
	[KP_INDEX(0, 0)] = KEY_VOLUMEUP,
	[KP_INDEX(0, 1)] = KEY_VOLUMEDOWN,
	[KP_INDEX(0, 2)] = KEY_RESERVED,

	[KP_INDEX(1, 0)] = KEY_RESERVED,
	[KP_INDEX(1, 1)] = KEY_RESERVED,
	[KP_INDEX(1, 2)] = KEY_RESERVED,
};

/* SURF keypad platform device information */
static struct gpio_event_matrix_info kp_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keymap,
	.output_gpios	= kp_row_gpios,
	.input_gpios	= kp_col_gpios,
	.noutputs	= ARRAY_SIZE(kp_row_gpios),
	.ninputs	= ARRAY_SIZE(kp_col_gpios),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.debounce_delay.tv.nsec = 10 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_DRIVE_INACTIVE |
			  GPIOKPF_PRINT_UNMAPPED_KEYS|GPIOKPF_DEBOUNCE,
};

static struct gpio_event_info *kp_info[] = {
	&kp_matrix_info.info
};

static struct gpio_event_platform_data kp_pdata = {
	.name		= "7x27a_kp",
	.info		= kp_info,
	.info_count	= ARRAY_SIZE(kp_info)
};

static struct platform_device kp_pdev = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &kp_pdata,
	},
};

#endif

static struct msm_handset_platform_data hs_platform_data = {
	.hs_name = "7k_handset",
	.pwr_key_delay_ms = 500, /* 0 will disable end key */
};

static struct platform_device hs_pdev = {
	.name   = "msm-handset",
	.id     = -1,
	.dev    = {
		.platform_data = &hs_platform_data,
	},
};

static void set_camera_reset_low(void)
{
    int rc = 0;

    rc = gpio_request(27, NULL);
    if (0 == rc)
    {
        /* ignore "rc" */
        rc = gpio_direction_output(27, 0);

        mdelay(10);
    }

    gpio_free(27);
    mdelay(10);
}
#ifdef CONFIG_TOUCHSCREEN_VIRTUAL_KEYS
struct kobject *android_touch_kobj;
static void touch_sysfs_init(void)
{
	android_touch_kobj = kobject_create_and_add("board_properties", NULL);
	if (android_touch_kobj == NULL) {
		printk(KERN_ERR "%s: subsystem_register failed\n", __func__);
	}
}
#endif

#ifdef CONFIG_ZTE_PLATFORM
static ssize_t debug_global_read(struct file *file, char __user *buf,
				    size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;
  ssize_t size;

  size = sizeof(smem_global);

	if (pos >= size)
		return 0;

	count = min(len, (size_t)(size - pos));
	if (copy_to_user(buf, (char *)global + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static struct file_operations debug_global_file_ops = {
	.owner = THIS_MODULE,
	.read = debug_global_read,
};
#endif

#if defined(CONFIG_BCM_WIFI)
static int wlan_init_power(void)
{
    int ret = 0;

    do {
        ret = gpio_tlmm_config(GPIO_CFG(92, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
        if(ret) {
            printk(KERN_ERR"WLAN_WAKE_UP config failed\n");
	    break;
        } 

        ret = gpio_tlmm_config(GPIO_CFG(93, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
        if(ret) {
            printk(KERN_ERR"WLAN_REG_ON config failed\n");
	    break;
        } 
	
		gpio_request(93, "WLAN_REG_ON");
		gpio_direction_output(93,0);
		gpio_free(93);

        return 0;

    } while (0);

    return -1;
}

#elif defined(CONFIG_ATH_WIFI)
#define WLAN_CHIP_PWD_PIN     93
#define WLAN_CHIP_WOW_PIN     92
//#define WLAN_3V3_EN		90
//#define WLAN_1V8_EN		88
	
#define ZTE_PROC_COMM_CMD3_PMIC_GPIO 16	
static int wlan_init_power(void)
	{
    int ret = 0;

	unsigned int pmic_gpio_config_3_3 = (PMIC_GPIO_9 << 28)|	\
									(CONFIG_CMOS << 24)|	\
									(PMIC_GPIO_VIN7 << 20)|	\
									(SOURCE_GND << 16)|		\
									(BUFFER_MEDIUM << 12)|	\
									(0 << 8)|				\
									(1 << 4)|				\
									0x0;
									
	unsigned int pmic_gpio_config_1_8 = (PMIC_GPIO_1 << 28)|	\
									(CONFIG_CMOS << 24)|	\
									(PMIC_GPIO_VIN7 << 20)|	\
									(SOURCE_GND << 16)|		\
									(BUFFER_MEDIUM << 12)|	\
									(0 << 8)|				\
									(1 << 4)|				\
									0x0;
									
	unsigned int pmic_gpio_cmd = ZTE_PROC_COMM_CMD3_PMIC_GPIO;
	
    do {
        pr_info("%s() enter\n", __func__);
		ret = gpio_tlmm_config(GPIO_CFG(WLAN_CHIP_WOW_PIN, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
        if(ret) {
            printk(KERN_ERR"WLAN_CHIP_WOW_PIN config failed\n");
	    break;
        } 
	gpio_request(WLAN_CHIP_WOW_PIN, "WLAN_WOW");
//	gpio_direction_output(WLAN_CHIP_WOW_PIN, 0);
        gpio_free(WLAN_CHIP_WOW_PIN);        

        pr_info("%s() LDO 3.3v On\n", __func__);
        printk("PCOM_CUSTOMER_CMD3 send/n");
		ret = msm_proc_comm(PCOM_CUSTOMER_CMD3, &pmic_gpio_config_3_3, &pmic_gpio_cmd);
		if(ret) {
            printk(KERN_ERR"Failed to turn on LDO 3.3v\n");
        }
		mdelay(50);

		ret = msm_proc_comm(PCOM_CUSTOMER_CMD3, &pmic_gpio_config_1_8, &pmic_gpio_cmd);

        if(ret) {
            printk(KERN_ERR"Failed to turn on LDO 1.8v\n");
			break;
        } 

        pr_info("%s() VREG 1.8v On\n", __func__);
        mdelay(100);
        
        pr_info("%s() Pull low CHIP PWD\n", __func__);
        /*
         * Pull low Chip power down pin
         */		
		ret = gpio_tlmm_config(GPIO_CFG(WLAN_CHIP_PWD_PIN, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
        if(ret) {
            printk(KERN_ERR"WLAN_CHIP_PWD_PIN config failed\n");
	    break;
        } 
	gpio_request(WLAN_CHIP_PWD_PIN, "WLAN_CHIP_PWD");
	gpio_direction_output(WLAN_CHIP_PWD_PIN, 0);
	gpio_free(WLAN_CHIP_PWD_PIN);
		
	return 0;

    } while (0);

    return ret;
}
#else
static int wlan_init_power(void)
{
	return 0;
}
#endif

#if defined (CONFIG_MACH_TURIES)
static void touch_vdd(void)
{
	struct vreg *vreg_s3_sp=NULL;
	int ret;
	vreg_s3_sp = vreg_get(NULL, "msme1");
	if (IS_ERR(vreg_s3_sp)) 
	{
		pr_err("%s: vreg_get for S3 failed\n", __func__);
	}
	else
	{
		vreg_disable(vreg_s3_sp);
	}
	gpio_tlmm_config(GPIO_CFG(131, 0, GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(132, 0, GPIO_CFG_OUTPUT,
				GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	
	ret = gpio_request(131,"I2C_SCL");
	if (ret) printk(KERN_ERR "gpio_request: %d failed!\n", 131);			
	gpio_direction_output(131, 0);
	ret = gpio_request(132,"I2C_SDA");
	if (ret) printk(KERN_ERR "gpio_request: %d failed!\n", 132);	
	gpio_direction_output(132, 0);
	ret = gpio_request(107,"TCHN_3V_EN");
	if (ret) printk(KERN_ERR "gpio_request: %d failed!\n", 107);	
	gpio_direction_output(107, 0);
	ret = gpio_request(82,"CAP_TS_INT");
	if (ret) printk(KERN_ERR "gpio_request: %d failed!\n", 82);	
	gpio_direction_output(82, 0);
	msleep(500);
	gpio_direction_output(107, 1);
	if (!IS_ERR(vreg_s3_sp)) 
	{
		vreg_enable(vreg_s3_sp);
	}
	gpio_direction_input(82);		
	gpio_free(131);
	gpio_free(132);
	gpio_free(107);
	gpio_free(82);
		
}
#endif

#define LED_GPIO_PDM		96
#define UART1DM_RX_GPIO		45
static void __init msm7x2x_init(void)
{
#ifdef CONFIG_ZTE_PLATFORM
	struct proc_dir_entry *entry;
#endif

	msm7x2x_misc_init();
	
#if defined (CONFIG_MACH_TURIES)
	touch_vdd();
#endif

	msm_device_i2c_init();
	
	wlan_init_power();  
#ifdef CONFIG_ATH_WIFI
        platform_device_register(&msm_wlan_ar6000_pm_device);
#endif	
//	msm7x27a_init_mmc();
	msm7x27a_init_ebi2();
	msm7x27a_cfg_uart2dm_serial();
#ifdef CONFIG_SERIAL_MSM_HS
	msm_uart_dm1_pdata.wakeup_irq = gpio_to_irq(UART1DM_RX_GPIO);
	msm_device_uart_dm1.dev.platform_data = &msm_uart_dm1_pdata;
#endif

	set_camera_reset_low();

	if (machine_is_msm7x27a_rumi3()) {
		platform_add_devices(rumi_sim_devices,
				ARRAY_SIZE(rumi_sim_devices));
	}
	if (1/*machine_is_msm7x27a_surf()*/ || machine_is_msm7x27a_ffa()) {
#ifdef CONFIG_USB_MSM_OTG_72K
		msm_otg_pdata.swfi_latency =
			msm7x27a_pm_data
		[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].latency;
		msm_device_otg.dev.platform_data = &msm_otg_pdata;
#endif
		msm_device_gadget_peripheral.dev.platform_data =
							&msm_gadget_pdata;
		msm7x27a_cfg_smsc911x();
		platform_add_devices(msm_footswitch_devices,
			     msm_num_footswitch_devices);
		platform_add_devices(surf_ffa_devices,
				ARRAY_SIZE(surf_ffa_devices));
		msm_fb_add_devices();
#ifdef CONFIG_USB_EHCI_MSM_72K
		msm7x2x_init_host();
#endif
	}
        /*
         * register sdcc device after dmov device(in surf_ffa_devices)
         * The reason is when system do suspend AR6000 wifi need to issue
         * some sdio commands through sdcc. sdcc will turn on the dmov for
         * transfer. So dmov devices should be suspended after sdcc device
         * to make sure ADM_CLK can be turned off. Ming
         */
	msm7x27a_init_mmc();

	msm_pm_set_platform_data(msm7x27a_pm_data,
				ARRAY_SIZE(msm7x27a_pm_data));

#ifdef CONFIG_TOUCHSCREEN_VIRTUAL_KEYS
	touch_sysfs_init();
#endif


#if defined(CONFIG_I2C) && defined(CONFIG_GPIO_SX150X)
	register_i2c_devices();
#endif
#if defined(CONFIG_BT) && defined(CONFIG_MARIMBA_CORE)
	bt_power_init();
#endif

#ifdef CONFIG_TOUCHSCREEN_ATMEL_MAXTOUCH
	if (cpu_is_msm7x25a() || cpu_is_msm7x25aa()) {
		atmel_ts_pdata.min_x = 0;
		atmel_ts_pdata.max_x = 480;
		atmel_ts_pdata.min_y = 0;
		atmel_ts_pdata.max_y = 320;
	}

	i2c_register_board_info(MSM_GSBI1_QUP_I2C_BUS_ID,
		atmel_ts_i2c_info,
		ARRAY_SIZE(atmel_ts_i2c_info));
#endif	
	i2c_register_board_info(MSM_GSBI1_QUP_I2C_BUS_ID,
		i2c_touch_devices,
		ARRAY_SIZE(i2c_touch_devices));
	
	i2c_register_board_info(MSM_GSBI0_QUP_I2C_BUS_ID,
			i2c_camera_devices,
			ARRAY_SIZE(i2c_camera_devices));
	platform_device_register(&kp_pdev);
	platform_device_register(&hs_pdev);

	/* configure it as a pdm function*/
	if (gpio_tlmm_config(GPIO_CFG(LED_GPIO_PDM, 3,
				GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
				GPIO_CFG_8MA), GPIO_CFG_ENABLE))
		pr_err("%s: gpio_tlmm_config for %d failed\n",
			__func__, LED_GPIO_PDM);
#ifndef CONFIG_ZTE_PLATFORM
	else
	{
		platform_device_register(&led_pdev);
	}
#endif

#ifdef CONFIG_MSM_RPC_VIBRATOR
	//if (machine_is_msm7x27a_ffa())
		msm_init_pmic_vibrator();
#endif
	/*7x25a kgsl initializations*/
	msm7x25a_kgsl_3d0_init();
	
#ifdef CONFIG_ZTE_PLATFORM	
	global = ioremap(SMEM_LOG_GLOBAL_BASE, sizeof(smem_global));
	if (!global) {
		printk(KERN_ERR "ioremap failed with SCL_SMEM_LOG_RAM_BASE\n");
		return;
	}
	entry = create_proc_entry("smem_global", S_IFREG | S_IRUGO, NULL);
	if (!entry) {
		printk(KERN_ERR "smem_global: failed to create proc entry\n");
		return;
	}
	entry->proc_fops = &debug_global_file_ops;
	entry->size = sizeof(smem_global);
#endif	

}

static void __init msm7x2x_init_early(void)
{
#ifdef CONFIG_ZTE_FTM_FLAG_SUPPORT
	zte_ftm_set_value(g_zte_ftm_flag_fixup);
#endif
	msm_msm7x2x_allocate_memory_regions();
}

MACHINE_START(MSM7X27A_RUMI3, "QCT MSM7x27a RUMI3")
	.boot_params	= PHYS_OFFSET + 0x100,
	.map_io		= msm_common_io_init,
	.reserve	= msm7x27a_reserve,
	.init_irq	= msm_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
	.init_early     = msm7x2x_init_early,
MACHINE_END
MACHINE_START(MSM7X27A_SURF, "QCT MSM7x27a SURF")
	.boot_params	= PHYS_OFFSET + 0x100,
	.map_io		= msm_common_io_init,
	.reserve	= msm7x27a_reserve,
	.init_irq	= msm_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
	.init_early     = msm7x2x_init_early,
#ifdef CONFIG_ZTE_PLATFORM
	.fixup          = zte_fixup,
#endif	
MACHINE_END
MACHINE_START(MSM7X27A_FFA, "QCT MSM7x27a FFA")
	.boot_params	= PHYS_OFFSET + 0x100,
	.map_io		= msm_common_io_init,
	.reserve	= msm7x27a_reserve,
	.init_irq	= msm_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
	.init_early     = msm7x2x_init_early,
#ifdef CONFIG_ZTE_PLATFORM
	.fixup          = zte_fixup,
#endif	
MACHINE_END

MACHINE_START(TURIES, "turies")
	.boot_params	= PHYS_OFFSET + 0x100,
	.map_io		= msm_common_io_init,
	.reserve	= msm7x27a_reserve,
	.init_irq	= msm_init_irq,
	.init_machine	= msm7x2x_init,
	.timer		= &msm_timer,
	.init_early     = msm7x2x_init_early,
#ifdef CONFIG_ZTE_PLATFORM
	.fixup          = zte_fixup,
#endif	
MACHINE_END
