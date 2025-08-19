// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "udma_u_common.h"
#include "udma_u_db.h"
#include "udma_u_jfr.h"
#include "udma_u_ctl.h"

static void udma_u_uninit_queue_buf(struct udma_u_jetty_queue *q)
{
	if (q->wrid) {
		free(q->wrid);
		q->wrid = NULL;
	}

	q->qbuf = NULL;
	q->qbuf_curr = NULL;
	q->qbuf_end = NULL;
}

static int udma_u_init_queue_buf(struct udma_u_jetty_queue *q, uint32_t max_entry_cnt,
				 uint32_t baseblk_size, uint32_t page_size, void *qbuff)
{
	uint32_t buf_shift;
	uint32_t entry_cnt;

	entry_cnt = roundup_pow_of_two(max_entry_cnt);
	buf_shift = align_power2(entry_cnt * baseblk_size);
	q->baseblk_shift = align_power2(baseblk_size);
	q->qbuf_size = align((1U << buf_shift), page_size);
	q->baseblk_cnt = q->qbuf_size >> q->baseblk_shift;
	q->baseblk_mask = q->baseblk_cnt - 1U;

	q->wrid = (uintptr_t *)malloc(q->baseblk_cnt * sizeof(uint64_t));
	if (!q->wrid) {
		UDMA_LOG_ERR("failed to alloc STARS buffer for wrid!\n");
		return ENOMEM;
	}

	q->qbuf = qbuff;
	q->qbuf_curr = q->qbuf;
	q->qbuf_end = q->qbuf + q->qbuf_size;

	return 0;
}

static int udma_u_create_idx_que_ex(struct udma_u_jfr *jfr, struct udma_u_jfr_cfg_ex *cfg_ex)
{
	struct udma_u_jfr_cstm_cfg *cstm_cfg = &cfg_ex->cstm_cfg;
	struct udma_u_jfr_idx_que *idx_que = &jfr->idx_que;

	idx_que->entry_shift = udma_u_ilog32(UDMA_JFR_IDX_QUE_ENTRY_SZ);
	idx_que->buf.length = (uint32_t)align(jfr->wqe_cnt << idx_que->entry_shift,
					      UDMA_HW_PAGE_SIZE);

	if (idx_que->buf.length != cstm_cfg->idx_que.buff_size) {
		UDMA_LOG_ERR("idx_que length is wrong, size = %u.\n", idx_que->buf.length);
		return EINVAL;
	}

	idx_que->buf.buf = cstm_cfg->idx_que.buff;

	idx_que->bitmap = udma_bitmap_alloc(jfr->wqe_cnt, &idx_que->bitmap_cnt);
	if (!idx_que->bitmap)
		return ENOMEM;

	return 0;
}

static int udma_u_create_rq_ex(struct udma_u_context *udma_ctx, struct udma_u_jfr *jfr,
			       struct udma_u_jfr_cfg_ex *cfg_ex)
{
	struct udma_u_jfr_cstm_cfg *cstm_cfg = &cfg_ex->cstm_cfg;
	struct udma_u_jetty_queue *rq = &jfr->rq;
	uint32_t sge_per_wqe;
	uint32_t wqebb_cnt;
	uint32_t ret;

	sge_per_wqe = min(jfr->max_sge, udma_ctx->jfr_sge);
	wqebb_cnt = sge_per_wqe * jfr->wqe_cnt;
	ret = udma_u_init_queue_buf(rq, wqebb_cnt, UDMA_JFR_WQEBB,
				    UDMA_HW_PAGE_SIZE, cstm_cfg->rq.buff);
	if (ret) {
		UDMA_LOG_ERR("init queue buf error, ret = %u.\n", ret);
		return EINVAL;
	}

	if (rq->qbuf_size != cstm_cfg->rq.buff_size) {
		UDMA_LOG_ERR("user cfg buffsize is error, rq->qbuf_size = %u.\n",
			     rq->qbuf_size);
		udma_u_uninit_queue_buf(rq);
		return EINVAL;
	}

	return 0;
}

static int udma_u_verify_que_cstm_cfg(struct udma_u_que_cfg_ex *que_cfg)
{
	if (que_cfg->buff == NULL || que_cfg->buff_size == 0) {
		UDMA_LOG_ERR("cstm cfg is invalid, que_cfg->buff_size = %u.\n",
			     que_cfg->buff_size);
		return EINVAL;
	}

	if (((uint64_t)que_cfg->buff & (uint64_t)PARTITION_ALIGNMENT) != 0) {
		UDMA_LOG_ERR("queue addr is not partition alignment.\n");
		return EINVAL;
	}

	return 0;
}

static int udma_u_verify_jfr_cstm_cfg(struct udma_u_jfr_cstm_cfg *cstm_cfg)
{
	if (!cstm_cfg->flag.bs.idxq_cstm || udma_u_verify_que_cstm_cfg(&cstm_cfg->idx_que)) {
		UDMA_LOG_ERR("jfr idxq cstm cfg is invalid, idxq_cstm = %d.\n",
			     cstm_cfg->flag.bs.idxq_cstm);
		return EINVAL;
	}

	if (!cstm_cfg->flag.bs.rq_cstm || udma_u_verify_que_cstm_cfg(&cstm_cfg->rq)) {
		UDMA_LOG_ERR("jfr rq cstm cfg is invalid, rq_cstm = %d.\n",
			     cstm_cfg->flag.bs.rq_cstm);
		return EINVAL;
	}

	if (!cstm_cfg->flag.bs.swdb_cstm || cstm_cfg->sw_db == NULL) {
		UDMA_LOG_ERR("jfr swdb cstm cfg is invalid, swdb_cstm = %d.\n",
			     cstm_cfg->flag.bs.swdb_cstm);
		return EINVAL;
	}

	return 0;
}

static int udma_u_verify_jfr_param_ex(urma_context_t *ctx, struct udma_u_jfr_cfg_ex *cfg_ex)
{
	if (cfg_ex == NULL || ctx == NULL) {
		UDMA_LOG_ERR("jfr ctx or cfg is null.\n");
		return EINVAL;
	}

	if (udma_u_verify_jfr_param(ctx, &cfg_ex->base_cfg))
		return EINVAL;

	return udma_u_verify_jfr_cstm_cfg(&cfg_ex->cstm_cfg);
}

static void udma_u_init_jfr_param_ex(struct udma_u_jfr *udma_jfr,
				     struct udma_u_jfr_cfg_ex *cfg_ex)
{
	struct udma_u_jfr_cstm_cfg *cstm_cfg;
	urma_jfr_cfg_t *base_cfg;

	base_cfg = &cfg_ex->base_cfg;
	cstm_cfg = &cfg_ex->cstm_cfg;
	udma_jfr->rq.cstm = cstm_cfg->flag.bs.rq_cstm;
	udma_jfr->idx_que.cstm = cstm_cfg->flag.bs.idxq_cstm;
	udma_jfr->swdb_cstm = cstm_cfg->flag.bs.swdb_cstm;
	udma_u_init_jfr_param(udma_jfr, base_cfg);
}

static urma_jfr_t *udma_u_create_jfr_ex(urma_context_t *ctx,
					struct udma_u_jfr_cfg_ex *cfg_ex)
{
	struct udma_u_context *udma_ctx = to_udma_u_ctx(ctx);
	struct udma_u_jfr *udma_jfr;
	int ret;

	if (udma_u_verify_jfr_param_ex(ctx, cfg_ex))
		return NULL;

	udma_jfr = (struct udma_u_jfr *)calloc(1, sizeof(*udma_jfr));
	if (!udma_jfr) {
		UDMA_LOG_ERR("alloc jfr ex failed.\n");
		return NULL;
	}

	udma_u_init_jfr_param_ex(udma_jfr, cfg_ex);

	if (!udma_jfr->lock_free &&
		pthread_spin_init(&udma_jfr->lock, PTHREAD_PROCESS_PRIVATE))
		goto err_spin_init;

	if (udma_u_create_idx_que_ex(udma_jfr, cfg_ex)) {
		UDMA_LOG_ERR("failed to create jfr idx que.\n");
		goto err_create_idx_que;
	}

	if (udma_u_create_rq_ex(udma_ctx, udma_jfr, cfg_ex)) {
		UDMA_LOG_ERR("failed to create jfr rqe buf.\n");
		goto err_create_rq;
	}

	udma_jfr->sw_db = cfg_ex->cstm_cfg.sw_db;

	ret = exec_jfr_create_cmd(ctx, udma_jfr, &cfg_ex->base_cfg);
	if (ret) {
		UDMA_LOG_ERR("urma cmd create jfr failed, ret = %d.\n", ret);
		goto err_exec_cmd;
	}

	if (udma_u_jetty_queue_insert(udma_ctx, &udma_jfr->rq, UDMA_U_JFR_TBL))
		goto err_insert_node;

	return &udma_jfr->base;

err_insert_node:
	(void)urma_cmd_delete_jfr(&udma_jfr->base);
err_exec_cmd:
	if (udma_jfr->rq.wrid)
		free(udma_jfr->rq.wrid);
err_create_rq:
	udma_bitmap_free(udma_jfr->idx_que.bitmap);
err_create_idx_que:
	if (!udma_jfr->lock_free)
		(void)pthread_spin_destroy(&udma_jfr->lock);
err_spin_init:
	free(udma_jfr);

	return NULL;
}

static int udma_u_jfr_ops_ex(urma_context_t *ctx, urma_user_ctl_in_t *in,
			     urma_user_ctl_out_t *out, enum udma_u_user_ctl_opcode op)
{
	struct udma_u_jfr_cfg_ex cfg_ex;
	urma_jfr_t *jfr = NULL;

	if (op == UDMA_U_USER_CTL_CREATE_JFR_EX) {
		if (!udma_u_user_ctl_check_param(in->addr, in->len, (uint32_t)sizeof(struct udma_u_jfr_cfg_ex), op) ||
		    !udma_u_user_ctl_check_param(out->addr, out->len, (uint32_t)sizeof(urma_jfr_t *), op))
			return EINVAL;

		(void)memcpy(&cfg_ex, (void *)in->addr, sizeof(struct udma_u_jfr_cfg_ex));
		jfr = udma_u_create_jfr_ex(ctx, &cfg_ex);
		if (jfr == NULL)
			return EFAULT;

		memcpy((void *)out->addr, &jfr, sizeof(urma_jfr_t *));
		atomic_fetch_add(&ctx->ref.atomic_cnt, 1);
	} else {
		if (!udma_u_user_ctl_check_param(in->addr, in->len, (uint32_t)sizeof(urma_jfr_t *), op))
			return EINVAL;

		(void)memcpy(&jfr, (void *)in->addr, sizeof(urma_jfr_t *));
		if (jfr == NULL)
			return EINVAL;

		if (udma_u_delete_jfr(jfr))
			return EFAULT;

		atomic_fetch_sub(&ctx->ref.atomic_cnt, 1);
	}

	return 0;
}

static udma_u_user_ctl_ops g_udma_u_user_ctl_ops[] = {
	[UDMA_U_USER_CTL_CREATE_JFR_EX] = udma_u_jfr_ops_ex,
	[UDMA_U_USER_CTL_DELETE_JFR_EX] = udma_u_jfr_ops_ex,
};

bool udma_u_user_ctl_check_param(uint64_t addr, uint32_t in_len, uint32_t len,
				 enum udma_u_user_ctl_opcode opcode)
{
	if (addr == 0 || (in_len != len)) {
		UDMA_LOG_ERR("parameter invalid in user ctl process,"
			     "opcode = %u, len = %u.\n", opcode, in_len);
		return false;
	}

	return true;
}

int udma_u_user_ctl(urma_context_t *ctx, urma_user_ctl_in_t *in,
		    urma_user_ctl_out_t *out)
{
	if (ctx == NULL || in == NULL || out == NULL) {
		UDMA_LOG_ERR("parameter invalid in urma_user_ctl.\n");
		return URMA_EINVAL;
	}

	if (in->opcode >= UDMA_U_USER_CTL_MAX) {
		UDMA_LOG_ERR("invalid opcode: 0x%x.\n", in->opcode);
		return URMA_ENOPERM;
	}

	return g_udma_u_user_ctl_ops[in->opcode](ctx, in, out,
		(enum udma_u_user_ctl_opcode)in->opcode);
}
