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

static bool udma_check_sge_num_and_opcode(urma_opcode_t opcode, struct udma_u_jetty_queue *sq,
					  urma_jfs_wr_t *wr, uint8_t *udma_opcode)
{
	switch (opcode) {
	case URMA_OPC_SEND:
		*udma_opcode = UDMA_OPCODE_SEND;
		goto send_sge_check;
	default:
		UDMA_LOG_ERR("Invalid opcode :%u\n", (uint8_t)opcode);
		return true;
	}

send_sge_check:
	return wr->send.src.num_sge > sq->max_sge_num;
}

static int udma_fill_send_sqe(struct udma_jfs_sqe_ctl *ctrl, urma_jfs_wr_t *wr,
			      struct udma_u_jetty_queue *sq, urma_target_jetty_t *tjetty, uint32_t *wqe_cnt)
{
	struct udma_u_target_jetty *udma_tjetty;
	struct udma_wqe_sge *sge;
	uint32_t sge_num = 0;
	urma_sge_t *sgl;
	uint32_t i;

	sgl = wr->send.src.sge;

	sge = (struct udma_wqe_sge *)udma_inc_ptr_wrap((uint8_t *)ctrl, (uint32_t)sizeof(struct udma_jfs_sqe_ctl),
						       (uint8_t *)sq->qbuf, (uint8_t *)sq->qbuf_end);

	for (i = 0; i < wr->send.src.num_sge; i++) {
		if (sgl[i].len == 0)
			continue;
		sge->length = sgl[i].len;
		sge->va = sgl[i].addr;
		sge = (struct udma_wqe_sge *)udma_inc_ptr_wrap((uint8_t *)sge,
								(uint32_t)sizeof(struct udma_wqe_sge),
								(uint8_t *)sq->qbuf, (uint8_t *)sq->qbuf_end);
		sge_num++;
	}
	*wqe_cnt = (SQE_NORMAL_CTL_LEN + (sge_num - 1) * UDMA_SGE_SIZE) / UDMA_JFS_WQEBB + 1;

	ctrl->sge_num = sge_num;
	udma_tjetty = to_udma_u_target_jetty(tjetty);
	ctrl->rmt_jetty_or_seg_id = tjetty->id.id;
	ctrl->token_en = udma_tjetty->token_value_valid;
	ctrl->rmt_token_value = udma_tjetty->token_value;
	ctrl->target_hint = wr->send.target_hint;

	return 0;
}

static int udma_parse_jfs_wr(struct udma_jfs_sqe_ctl *wqe_ctl,
			     urma_jfs_wr_t *wr, struct udma_u_jetty_queue *sq,
			     struct udma_wqe_info *wqe_info, urma_target_jetty_t *tjetty)
{
	switch (wqe_info->opcode) {
	case UDMA_OPCODE_SEND:
		return udma_fill_send_sqe(wqe_ctl, wr, sq, tjetty, &wqe_info->wqe_cnt);
	default:
		return 0;
	}
}

static bool udma_check_sq_overflow(struct udma_u_jetty_queue *sq, urma_jfs_wr_t *wr,
				   struct udma_wqe_info *wqe_info)
{
	uint32_t wqe_bb_cnt = MAX_SQE_BB_NUM;
	uint32_t wqe_ctrl_len = 0;
	uint32_t num_sge_wr = 0;
	uint32_t udma_opcode;
	uint32_t sge_num = 0;
	urma_sge_t *sgl;
	uint32_t i;

	if (!udma_sq_overflow(sq, wqe_bb_cnt))
		return false;

	udma_opcode = wqe_info->opcode;

	wqe_ctrl_len = SQE_NORMAL_CTL_LEN;

	if (udma_opcode == UDMA_OPCODE_SEND) {
		num_sge_wr = wr->send.src.num_sge;
		sgl = wr->send.src.sge;
	}

	for (i = 0; i < num_sge_wr; i++)
		sgl[i].len == 0 ? 0 : sge_num++;

	wqe_bb_cnt = (wqe_ctrl_len + (sge_num - 1) * UDMA_SGE_SIZE) / UDMA_JFS_WQEBB + 1;

	return udma_sq_overflow(sq, wqe_bb_cnt);
}

static urma_status_t udma_set_sqe(struct udma_jfs_sqe_ctl *wqe_ctl,
				  struct udma_u_jetty_queue *sq,
				  urma_jfs_wr_t *wr, struct udma_wqe_info *wqe_info)
{
	struct udma_u_target_jetty *udma_tjetty;
	urma_target_jetty_t *tjetty;

	if (udma_check_sq_overflow(sq, wr, wqe_info)) {
		UDMA_LOG_ERR("JFS overflow.\n");
		return URMA_EINVAL;
	}

	(void)memset(wqe_ctl, 0, sizeof(*wqe_ctl));
	wqe_ctl->opcode = wqe_info->opcode;
	wqe_ctl->flag = wr->flag.value;
	wqe_ctl->owner = ((sq->pi & sq->baseblk_cnt) == 0 ? 1 : 0);

	if (sq->trans_mode == URMA_TM_RC)
		tjetty = &sq->tjetty->urma_tjetty;
	else
		tjetty = wr->tjetty;

	udma_tjetty = to_udma_u_target_jetty(tjetty);

	wqe_ctl->tp_id = tjetty->tp.tpn;

	memcpy(wqe_ctl->rmt_eid, &udma_tjetty->le_eid.raw, sizeof(uint8_t) *
	       URMA_EID_SIZE);

	wqe_ctl->rmt_jetty_type = (uint8_t)(tjetty->type);
	if (udma_parse_jfs_wr(wqe_ctl, wr, sq, wqe_info, tjetty) != 0) {
		UDMA_LOG_ERR("Failed to parse wr\n");
		return URMA_EINVAL;
	}

	return URMA_SUCCESS;
}

urma_status_t udma_u_post_one_wr(struct udma_u_context *udma_ctx,
				 struct udma_u_jetty_queue *sq,
				 urma_jfs_wr_t *wr,
				 struct udma_jfs_sqe_ctl **wqe_addr,
				 bool *dwqe_enable)
{
	struct udma_wqe_info wqe_info = {};
	uint32_t wqebb_cnt;
	urma_status_t ret;
	uint32_t i;

	if (udma_check_sge_num_and_opcode(wr->opcode, sq, wr, &wqe_info.opcode)) {
		UDMA_LOG_ERR("wr sge num or opcode is invalid.\n");
		return URMA_EINVAL;
	}

	ret = udma_set_sqe((struct udma_jfs_sqe_ctl *)sq->qbuf_curr, sq, wr, &wqe_info);
	if (ret)
		return ret;

	wqebb_cnt = wqe_info.wqe_cnt;

	*wqe_addr = (struct udma_jfs_sqe_ctl *)sq->qbuf_curr;

	sq->qbuf_curr = udma_inc_ptr_wrap((uint8_t *)sq->qbuf_curr,
					  wqebb_cnt << sq->baseblk_shift,
					  (uint8_t *)sq->qbuf,
					  (uint8_t *)sq->qbuf_end);
	for (i = 0; i < wqebb_cnt; i++)
		sq->wrid[(sq->pi + i) & (sq->baseblk_cnt - 1)] = wr->user_ctx;
	sq->pi += wqebb_cnt;

	return URMA_SUCCESS;
}

urma_status_t udma_u_post_sq_wr(struct udma_u_context *udma_ctx,
				struct udma_u_jetty_queue *sq, urma_jfs_wr_t *wr,
				urma_jfs_wr_t **bad_wr)
{
	struct udma_jfs_sqe_ctl *wqe_addr;
	urma_status_t ret = URMA_SUCCESS;
	bool dwqe_enable = false;
	urma_jfs_wr_t *it;
	int wr_cnt = 0;

	(void)pthread_spin_lock(&sq->lock);

	for (it = wr; it != NULL; it = (urma_jfs_wr_t *)(void *)it->next) {
		ret = udma_u_post_one_wr(udma_ctx, sq, it, &wqe_addr, &dwqe_enable);
		if (ret) {
			*bad_wr = (urma_jfs_wr_t *)it;
			break;
		}
		wr_cnt++;
	}

	if (wr_cnt) {
		udma_to_device_barrier();
		udma_update_sq_db(sq);
	}

	(void)pthread_spin_unlock(&sq->lock);

	return ret;
}

urma_status_t udma_u_post_jfs_wr(urma_jfs_t *jfs, urma_jfs_wr_t *wr,
				 urma_jfs_wr_t **bad_wr)
{
	struct udma_u_context *udma_ctx;
	struct udma_u_jfs *udma_jfs;
	urma_status_t ret;

	udma_jfs = to_udma_u_jfs(jfs);
	udma_ctx = to_udma_u_ctx(jfs->urma_ctx);

	ret = udma_u_post_sq_wr(udma_ctx, &udma_jfs->sq, wr, bad_wr);
	if (ret)
		UDMA_LOG_ERR("JFS post sq wr failed, jfs id = %u.\n",
			     udma_jfs->sq.idx);

	return ret;
}
