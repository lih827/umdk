// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "urma_provider.h"
#include "urma_private.h"
#include "udma_u_buf.h"
#include "udma_u_db.h"
#include "udma_u_jfc.h"

static int udma_u_check_jfc_cfg(urma_context_t *ctx, urma_jfc_cfg_t *cfg)
{
	urma_device_cap_t *cap = &ctx->dev->sysfs_dev->dev_attr.dev_cap;

	if (cfg->depth == 0 || cfg->depth > cap->max_jfc_depth) {
		UDMA_LOG_ERR("invalid jfc cfg depth = %u, cap depth = %u.\n",
			     cfg->depth, cap->max_jfc_depth);
		return EINVAL;
	}

	if (cfg->ceqn >= cap->ceq_cnt) {
		UDMA_LOG_ERR("invalid ceqn = %u, cap ceq cnt = %u.\n",
			     cfg->ceqn, cap->ceq_cnt);
		return EINVAL;
	}

	return 0;
}

static int udma_u_jfc_cmd(urma_context_t *ctx, struct udma_u_jfc *jfc,
			  urma_jfc_cfg_t *cfg)
{
	struct udma_create_jfc_ucmd cmd = {};
	urma_cmd_udrv_priv_t udata = {};

	cmd.buf_addr = (uintptr_t)jfc->cq.qbuf;
	cmd.db_addr = (uintptr_t)jfc->sw_db;
	cmd.buf_len = jfc->cq.qbuf_size;

	udma_u_set_udata(&udata, &cmd, sizeof(cmd), NULL, 0);

	return urma_cmd_create_jfc(ctx, &jfc->base, cfg, &udata);
}

urma_jfc_t *udma_u_create_jfc(urma_context_t *ctx, urma_jfc_cfg_t *cfg)
{
	struct udma_u_context *udma_ctx = to_udma_u_ctx(ctx);
	struct udma_u_jfc *jfc;
	uint32_t depth;
	int ret;

	ret = udma_u_check_jfc_cfg(ctx, cfg);
	if (ret)
		return NULL;

	jfc = (struct udma_u_jfc *)calloc(1, sizeof(*jfc));
	if (!jfc) {
		UDMA_LOG_ERR("failed to alloc user udma jfc memory.\n");
		return NULL;
	}

	if (pthread_spin_init(&jfc->cq.lock, PTHREAD_PROCESS_PRIVATE)) {
		UDMA_LOG_ERR("failed to init user udma jfc spinlock.\n");
		goto err_init_lock;
	}

	depth = cfg->depth < UDMA_U_MIN_JFC_DEPTH ? UDMA_U_MIN_JFC_DEPTH : cfg->depth;
	if (!udma_u_alloc_queue_buf(&jfc->cq, depth, udma_ctx->cqe_size,
				    UDMA_HW_PAGE_SIZE, false)) {
		UDMA_LOG_ERR("failed to alloc user jfc wqe buf.\n");
		goto err_alloc_buf;
	}

	jfc->sw_db = (uint32_t *)udma_u_alloc_sw_db(udma_ctx, UDMA_JFC_TYPE_DB);
	if (!jfc->sw_db) {
		UDMA_LOG_ERR("failed to create alloc user jfc sw db.\n");
		goto err_alloc_sw_db;
	}

	ret = udma_u_jfc_cmd(ctx, jfc, cfg);
	if (ret) {
		UDMA_LOG_ERR("udma jfc failed to create urma cmd.\n");
		goto err_create_jfc;
	}

	jfc->cq_shift = align_power2(jfc->cq.baseblk_cnt);
	jfc->cq.idx = jfc->base.jfc_id.id;
	jfc->arm_sn = 1;

	return &jfc->base;

err_create_jfc:
	udma_u_free_sw_db(udma_ctx, jfc->sw_db, UDMA_JFC_TYPE_DB);
err_alloc_sw_db:
	udma_u_free_queue_buf(&jfc->cq);
err_alloc_buf:
	(void)pthread_spin_destroy(&jfc->cq.lock);
err_init_lock:
	free(jfc);
	return NULL;
}

urma_status_t udma_u_delete_jfc(urma_jfc_t *jfc)
{
	struct udma_u_context *udma_ctx = to_udma_u_ctx(jfc->urma_ctx);
	struct udma_u_jfc *udma_jfc = to_udma_u_jfc(jfc);

	if (urma_cmd_delete_jfc(jfc)) {
		UDMA_LOG_ERR("ubcore delete jfc failed.\n");
		return URMA_FAIL;
	}

	udma_u_free_sw_db(udma_ctx, udma_jfc->sw_db, UDMA_JFC_TYPE_DB);
	udma_u_free_queue_buf(&udma_jfc->cq);
	(void)pthread_spin_destroy(&udma_jfc->cq.lock);
	free(udma_jfc);

	return URMA_SUCCESS;
}
