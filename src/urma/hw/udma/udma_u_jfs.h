/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef __UDMA_U_JFS_H__
#define __UDMA_U_JFS_H__

#include "urma_types.h"
#include "urma_provider.h"
#include "udma_u_common.h"

#define MAX_SQE_BB_NUM 4
#define SQE_WRITE_NOTIFY_CTL_LEN 80
#define UDMA_MAX_PRIORITY 16

static inline void udma_u_init_sq_param(struct udma_u_jetty_queue *sq,
					urma_jfs_cfg_t *cfg)
{
	sq->max_inline_size = cfg->max_inline_data;
	sq->pi = 0;
	sq->ci = 0;
}

static inline uint32_t sq_cal_wqebb_num(uint32_t sqe_ctl_len, uint32_t sge_num,
					uint32_t wqebb_size)
{
	return (sqe_ctl_len + (sge_num - (uint32_t)1) * (uint32_t)UDMA_SGE_SIZE) /
		wqebb_size + (uint32_t)1;
}

int udma_u_create_sq(struct udma_u_jetty_queue *sq, urma_jfs_cfg_t *cfg);
urma_jfs_t *udma_u_create_jfs(urma_context_t *ctx, urma_jfs_cfg_t *cfg);
urma_status_t udma_u_delete_jfs(urma_jfs_t *jfs);
void udma_u_delete_sq(struct udma_u_jetty_queue *sq);
int udma_u_exec_jfs_create_cmd(urma_context_t *ctx,
			       struct udma_u_jfs *jfs,
			       urma_jfs_cfg_t *cfg);

#endif /* __UDMA_U_JFS_H__ */
