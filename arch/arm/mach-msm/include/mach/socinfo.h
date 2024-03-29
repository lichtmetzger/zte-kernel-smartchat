/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _ARCH_ARM_MACH_MSM_SOCINFO_H_
#define _ARCH_ARM_MACH_MSM_SOCINFO_H_

#include <linux/init.h>

#include <asm/cputype.h>
#include <asm/mach-types.h>
/*
 * SOC version type with major number in the upper 16 bits and minor
 * number in the lower 16 bits.  For example:
 *   1.0 -> 0x00010000
 *   2.3 -> 0x00020003
 */
#define SOCINFO_VERSION_MAJOR(ver) ((ver & 0xffff0000) >> 16)
#define SOCINFO_VERSION_MINOR(ver) (ver & 0x0000ffff)

enum msm_cpu {
	MSM_CPU_UNKNOWN = 0,
	MSM_CPU_7X01,
	MSM_CPU_7X25,
	MSM_CPU_7X27,
	MSM_CPU_8X50,
	MSM_CPU_8X50A,
	MSM_CPU_7X30,
	MSM_CPU_8X55,
	MSM_CPU_8X60,
	MSM_CPU_8960,
	MSM_CPU_7X27A,
	FSM_CPU_9XXX,
	MSM_CPU_7X25A,
	MSM_CPU_7X25AA,
	MSM_CPU_7X25AB,
	MSM_CPU_8064,
	MSM_CPU_8930,
	MSM_CPU_7X27AA,
	MSM_CPU_8625,
};

#ifdef CONFIG_ZTE_PLATFORM
#ifdef CONFIG_ZTE_FTM_FLAG_SUPPORT
void zte_ftm_set_value(int val);
#endif
#endif


enum msm_cpu socinfo_get_msm_cpu(void);
uint32_t socinfo_get_id(void);
uint32_t socinfo_get_version(void);
char *socinfo_get_build_id(void);
uint32_t socinfo_get_platform_type(void);
uint32_t socinfo_get_platform_subtype(void);
uint32_t socinfo_get_platform_version(void);
int __init socinfo_init(void) __must_check;

static inline int get_core_count(void)
{
	if (!(read_cpuid_mpidr() & BIT(31)))
		return 1;

	if (read_cpuid_mpidr() & BIT(30) &&
		!machine_is_msm8960_sim() &&
		!machine_is_apq8064_sim())
		return 1;

	/* 1 + the PART[1:0] field of MIDR */
	return ((read_cpuid_id() >> 4) & 3) + 1;
}

static inline int read_msm_cpu_type(void)
{
	if (machine_is_msm8960_sim())
		return MSM_CPU_8960;

	switch (read_cpuid_id()) {
	case 0x510F02D0:
	case 0x510F02D2:
	case 0x510F02D4:
		return MSM_CPU_8X60;

	case 0x510F04D0:
	case 0x510F04D1:
	case 0x510F04D2:
		return MSM_CPU_8960;

	case 0x511F04D0:
		if (get_core_count() == 2)
			return MSM_CPU_8960;
		else
			return MSM_CPU_8930;

	case 0x510F06F0:
		return MSM_CPU_8064;

	default:
		return MSM_CPU_UNKNOWN;
	};
}

static inline int cpu_is_msm7x01(void)
{
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X01;
}

static inline int cpu_is_msm7x25(void)
{
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X25;
}

static inline int cpu_is_msm7x27(void)
{
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X27;
}

static inline int cpu_is_msm7x27a(void)
{
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X27A;
}

static inline int cpu_is_msm7x27aa(void)
{
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X27AA;
}

static inline int cpu_is_msm7x25a(void)
{
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X25A;
}

static inline int cpu_is_msm7x25aa(void)
{
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X25AA;
}

static inline int cpu_is_msm7x25ab(void)
{
#ifdef CONFIG_ARCH_MSM7X27A
enum msm_cpu cpu = socinfo_get_msm_cpu();

       BUG_ON(cpu == MSM_CPU_UNKNOWN);
       pr_err(" Returned from cpu check \n");
       return cpu == MSM_CPU_7X25AB;
#else
       return 0;
#endif
}

static inline int cpu_is_msm7x30(void)
{
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X30;
}

static inline int cpu_is_qsd8x50(void)
{
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_8X50;
}

static inline int cpu_is_msm8x55(void)
{
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_8X55;
}

static inline int cpu_is_msm8x60(void)
{
	return read_msm_cpu_type() == MSM_CPU_8X60;
}

static inline int cpu_is_msm8960(void)
{
	return read_msm_cpu_type() == MSM_CPU_8960;
}

static inline int cpu_is_apq8064(void)
{
	return read_msm_cpu_type() == MSM_CPU_8064;
}

static inline int cpu_is_msm8930(void)
{
	return read_msm_cpu_type() == MSM_CPU_8930;
}

static inline int cpu_is_fsm9xxx(void)
{
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == FSM_CPU_9XXX;
}

static inline int cpu_is_msm8625(void)
{
#ifdef CONFIG_ARCH_MSM8625
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_8625;
#else
	return 0;
#endif
}

#endif
