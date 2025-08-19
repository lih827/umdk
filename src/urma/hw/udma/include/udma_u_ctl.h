/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef __UDMA_U_CTL_H__
#define __UDMA_U_CTL_H__

#include "urma_types.h"

#define REDUCE_OPCODE_MIN 8
#define REDUCE_OPCODE_MAX 11
#define REDUCE_DATA_TYPE_MAX 9
#define PARTITION_ALIGNMENT 0xfff
#define UDMA_USER_CTL_QUERY_TP_SPORT 16

struct udma_u_que_cfg_ex {
	uint32_t buff_size;
	void *buff;
};

union udma_jfr_flag {
	struct {
		uint32_t idxq_cstm  : 1;
		uint32_t rq_cstm    : 1;
		uint32_t swdb_cstm    : 1;
		uint32_t swdb_ctl_cstm :1;
		uint32_t reserved       : 28;
	} bs;
	uint32_t value;
};

struct udma_u_jfr_cstm_cfg {
	union udma_jfr_flag flag;
	struct udma_u_que_cfg_ex idx_que;
	struct udma_u_que_cfg_ex rq;
	uint32_t *sw_db;
};

struct udma_u_jfr_cfg_ex {
	urma_jfr_cfg_t base_cfg;
	struct udma_u_jfr_cstm_cfg cstm_cfg;
};

enum udma_u_user_ctl_opcode {
	UDMA_U_USER_CTL_CREATE_JFR_EX,
	UDMA_U_USER_CTL_DELETE_JFR_EX,
	UDMA_U_USER_CTL_MAX,
};

#endif /* __UDMA_U_CTL_H__ */
