/* Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
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
#ifndef __ADRENO_H
#define __ADRENO_H

#include "kgsl_device.h"
#include "adreno_drawctxt.h"
#include "adreno_ringbuffer.h"

#define DEVICE_3D_NAME "kgsl-3d"
#define DEVICE_3D0_NAME "kgsl-3d0"

#define ADRENO_DEVICE(device) \
		KGSL_CONTAINER_OF(device, struct adreno_device, dev)

/* Flags to control command packet settings */
#define KGSL_CMD_FLAGS_NONE             0x00000000
#define KGSL_CMD_FLAGS_PMODE		0x00000001
#define KGSL_CMD_FLAGS_NO_TS_CMP	0x00000002
#define KGSL_CMD_FLAGS_NOT_KERNEL_CMD	0x00000004

/* Command identifiers */
#define KGSL_CONTEXT_TO_MEM_IDENTIFIER	0xDEADBEEF
#define KGSL_CMD_IDENTIFIER		0xFEEDFACE

#ifdef CONFIG_MSM_SCM
#define ADRENO_DEFAULT_PWRSCALE_POLICY  (&kgsl_pwrscale_policy_tz)
#else
#define ADRENO_DEFAULT_PWRSCALE_POLICY  NULL
#endif

#define ADRENO_ISTORE_START 0x5000 /* Istore offset */

#define ADRENO_NUM_CTX_SWITCH_ALLOWED_BEFORE_DRAW	50

enum adreno_gpurev {
	ADRENO_REV_UNKNOWN = 0,
	ADRENO_REV_A200 = 200,
	ADRENO_REV_A203 = 203,
	ADRENO_REV_A205 = 205,
	ADRENO_REV_A220 = 220,
	ADRENO_REV_A225 = 225,
	ADRENO_REV_A320 = 320,
};

struct adreno_gpudev;

struct adreno_device {
	struct kgsl_device dev;    /* Must be first field in this struct */
	unsigned int chip_id;
	enum adreno_gpurev gpurev;
	struct kgsl_memregion gmemspace;
	struct adreno_context *drawctxt_active;
	const char *pfp_fwfile;
	unsigned int *pfp_fw;
	size_t pfp_fw_size;
	const char *pm4_fwfile;
	unsigned int *pm4_fw;
	size_t pm4_fw_size;
	struct adreno_ringbuffer ringbuffer;
	unsigned int mharb;
	struct adreno_gpudev *gpudev;
	unsigned int wait_timeout;
	unsigned int istore_size;
	unsigned int pix_shader_start;
	unsigned int instruction_size;
	unsigned int ib_check_level;
};

struct adreno_gpudev {
	/* keeps track of when we need to execute the draw workaround code */
	int ctx_switches_since_last_draw;
	/*
	 * These registers are in a different location on A3XX,  so define
	 * them in the structure and use them as variables.
	 */
	unsigned int reg_rbbm_status;
	unsigned int reg_cp_pfp_ucode_data;
	unsigned int reg_cp_pfp_ucode_addr;

	/* GPU specific function hooks */
	int (*ctxt_create)(struct adreno_device *, struct adreno_context *);
	void (*ctxt_save)(struct adreno_device *, struct adreno_context *);
	void (*ctxt_restore)(struct adreno_device *, struct adreno_context *);
	void (*ctxt_draw_workaround)(struct adreno_device *);
	irqreturn_t (*irq_handler)(struct adreno_device *);
	void (*irq_control)(struct adreno_device *, int);
	void * (*snapshot)(struct adreno_device *, void *, int *, int);
	void (*rb_init)(struct adreno_device *, struct adreno_ringbuffer *);
	void (*start)(struct adreno_device *);
	unsigned int (*busy_cycles)(struct adreno_device *);
};

extern struct adreno_gpudev adreno_a2xx_gpudev;
extern struct adreno_gpudev adreno_a3xx_gpudev;

/* A2XX register sets defined in adreno_a2xx.c */
extern const unsigned int a200_registers[];
extern const unsigned int a220_registers[];
extern const unsigned int a200_registers_count;
extern const unsigned int a220_registers_count;

/* A3XX register set defined in adreno_a3xx.c */
extern const unsigned int a3xx_registers[];
extern const unsigned int a3xx_registers_count;

int adreno_idle(struct kgsl_device *device, unsigned int timeout);
void adreno_regread(struct kgsl_device *device, unsigned int offsetwords,
				unsigned int *value);
void adreno_regwrite(struct kgsl_device *device, unsigned int offsetwords,
				unsigned int value);

struct kgsl_memdesc *adreno_find_region(struct kgsl_device *device,
						unsigned int pt_base,
						unsigned int gpuaddr,
						unsigned int size);

uint8_t *adreno_convertaddr(struct kgsl_device *device,
	unsigned int pt_base, unsigned int gpuaddr, unsigned int size);

void *adreno_snapshot(struct kgsl_device *device, void *snapshot, int *remain,
		int hang);

static inline int adreno_is_a200(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A200);
}

static inline int adreno_is_a203(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A203);
}

static inline int adreno_is_a205(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A205);
}

static inline int adreno_is_a20x(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev <= 209);
}

static inline int adreno_is_a220(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A220);
}

static inline int adreno_is_a225(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev == ADRENO_REV_A225);
}

static inline int adreno_is_a22x(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev  == ADRENO_REV_A220 ||
		adreno_dev->gpurev == ADRENO_REV_A225);
}

static inline int adreno_is_a2xx(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev <= 299);
}

static inline int adreno_is_a3xx(struct adreno_device *adreno_dev)
{
	return (adreno_dev->gpurev >= 300);
}

/**
 * adreno_encode_istore_size - encode istore size in CP format
 * @adreno_dev - The 3D device.
 *
 * Encode the istore size into the format expected that the
 * CP_SET_SHADER_BASES and CP_ME_INIT commands:
 * bits 31:29 - istore size as encoded by this function
 * bits 27:16 - vertex shader start offset in instructions
 * bits 11:0 - pixel shader start offset in instructions.
 */
static inline int adreno_encode_istore_size(struct adreno_device *adreno_dev)
{
	unsigned int size;
	/* in a225 the CP microcode multiplies the encoded
	 * value by 3 while decoding.
	 */
	if (adreno_is_a225(adreno_dev))
		size = adreno_dev->istore_size/3;
	else
		size = adreno_dev->istore_size;

	return (ilog2(size) - 5) << 29;
}

#endif /*__ADRENO_H */
