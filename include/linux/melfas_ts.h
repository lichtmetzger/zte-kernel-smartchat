//--------------------------------------------------------
//
//
//	Melfas MCS8000 Series Download base v1.0 2011.08.26
//
//
//--------------------------------------------------------

/*
 * include/linux/melfas_ts.h - platform data structure for MCS Series sensor
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

#ifndef _LINUX_MELFAS_TS_H
#define _LINUX_MELFAS_TS_H

#define MELFAS_TS_NAME "melfas-ts"
#include <linux/earlysuspend.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>

struct melfas_ts_data
{
    uint16_t addr;
    struct i2c_client *client;
    struct input_dev *input_dev;
    struct work_struct  work;
    uint32_t flags;
    int (*power)(int on);
    struct early_suspend early_suspend;
};

#endif /* _LINUX_MELFAS_TS_H */
