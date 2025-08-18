// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "udma_u_buf.h"
#include "udma_u_db.h"
#include "udma_u_jfs.h"

int udma_u_exec_jfs_create_cmd(urma_context_t *ctx, struct udma_u_jfs *jfs,
			       urma_jfs_cfg_t *cfg)
{
	struct udma_create_jetty_ucmd cmd = {};
	urma_cmd_udrv_priv_t udata = {};
	int ret;

	if (cfg->priority >= UDMA_MAX_PRIORITY) {
		UDMA_LOG_ERR("user mode jfs priority is out of range, priority is %u.\n",
			     cfg->priority);
		return EINVAL;
	}

	cmd.buf_addr = (uintptr_t)jfs->sq.qbuf;
	cmd.buf_len = jfs->sq.qbuf_size;
	cmd.jetty_addr = (uintptr_t)&jfs->sq;
	cmd.sqe_bb_cnt = jfs->sq.sqe_bb_cnt;
	cmd.jetty_type = jfs->jfs_type;
	cmd.jfs_id = jfs->sq.db.id;
	udma_u_set_udata(&udata, &cmd, (uint32_t)sizeof(cmd), NULL, 0);
	ret = urma_cmd_create_jfs(ctx, &jfs->base, cfg, &udata);
	if (ret != 0)
		UDMA_LOG_ERR("failed to urma cmd create jfs, ret is %d.\n", ret);

	return ret;
}

int udma_u_create_sq(struct udma_u_jetty_queue *sq, urma_jfs_cfg_t *cfg)
{
	uint32_t sqe_bb_cnt;

	if (pthread_spin_init(&sq->lock, PTHREAD_PROCESS_PRIVATE))
		goto err_init_lock;

	udma_u_init_sq_param(sq, cfg);

	sqe_bb_cnt = sq_cal_wqebb_num(SQE_WRITE_NOTIFY_CTL_LEN,
				      cfg->max_sge, UDMA_JFS_WQEBB);
	if (sqe_bb_cnt > MAX_SQE_BB_NUM)
		sqe_bb_cnt = MAX_SQE_BB_NUM;
	sq->sqe_bb_cnt = sqe_bb_cnt;
	sq->max_sge_num = cfg->max_sge;
	if (!udma_u_alloc_queue_buf(sq, sqe_bb_cnt * cfg->depth,
				    UDMA_JFS_WQEBB, UDMA_HW_PAGE_SIZE, true)) {
		UDMA_LOG_ERR("failed to alloc jfs wqe buf.\n");
		goto err_alloc_buf;
	}

	return 0;

err_alloc_buf:
	(void)pthread_spin_destroy(&sq->lock);
err_init_lock:
	return EINVAL;
}

void udma_u_delete_sq(struct udma_u_jetty_queue *sq)
{
	udma_u_free_queue_buf(sq);
	(void)pthread_spin_destroy(&sq->lock);
}

urma_jfs_t *udma_u_create_jfs(urma_context_t *ctx, urma_jfs_cfg_t *cfg)
{
	struct udma_u_jfs *jfs;

	if (cfg->trans_mode == URMA_TM_RC) {
		UDMA_LOG_ERR("jfs not support RC transmode.\n");
		return NULL;
	}

	jfs = (struct udma_u_jfs *)calloc(1, sizeof(struct udma_u_jfs));
	if (jfs == NULL) {
		UDMA_LOG_ERR("memory allocation failed.\n");
		return NULL;
	}

	if (udma_u_create_sq(&jfs->sq, cfg)) {
		UDMA_LOG_ERR("failed to create sq.\n");
		goto err_create_sq;
	}

	jfs->jfs_type = UDMA_URMA_NORMAL_JETTY_TYPE;
	if (udma_u_exec_jfs_create_cmd(ctx, jfs, cfg))
		goto err_exec_cmd;

	jfs->sq.db.id = jfs->base.jfs_id.id;
	jfs->sq.db.type = UDMA_MMAP_JETTY_DSQE;
	if (udma_u_alloc_db(ctx, &jfs->sq.db))
		goto err_alloc_db;

	jfs->sq.dwqe_addr = (void *)jfs->sq.db.addr;

	return &jfs->base;
err_alloc_db:
	urma_cmd_delete_jfs(&jfs->base);
err_exec_cmd:
	udma_u_delete_sq(&jfs->sq);
err_create_sq:
	free(jfs);

	return NULL;
}

static void udma_u_free_jfs(urma_jfs_t *jfs)
{
	struct udma_u_jfs *udma_jfs = to_udma_u_jfs(jfs);

	udma_u_free_db(jfs->urma_ctx, &udma_jfs->sq.db);
	udma_u_delete_sq(&udma_jfs->sq);
	free(udma_jfs);
}

urma_status_t udma_u_delete_jfs(urma_jfs_t *jfs)
{
	if (urma_cmd_delete_jfs(jfs))
		return URMA_FAIL;

	udma_u_free_jfs(jfs);

	return URMA_SUCCESS;
}
