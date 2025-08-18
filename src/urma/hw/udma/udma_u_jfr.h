/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef __UDMA_U_JFR_H__
#define __UDMA_U_JFR_H__

#include "udma_u_common.h"

#define UDMA_JFR_IDX_QUE_ENTRY_SZ 4
#define UDMA_U_MIN_JFR_DEPTH 64

urma_jfr_t *udma_u_create_jfr(urma_context_t *ctx, urma_jfr_cfg_t *cfg);
urma_status_t udma_u_delete_jfr(urma_jfr_t *jfr);
urma_status_t udma_u_unimport_jfr(urma_target_jetty_t *target_jfr);
int udma_u_verify_jfr_param(urma_context_t *ctx, urma_jfr_cfg_t *cfg);
void udma_u_init_jfr_param(struct udma_u_jfr *jfr, urma_jfr_cfg_t *cfg);
int exec_jfr_create_cmd(urma_context_t *ctx, struct udma_u_jfr *jfr,
			urma_jfr_cfg_t *cfg);
urma_target_jetty_t *udma_u_import_jfr_ex(urma_context_t *ctx,
					  urma_rjfr_t *rjfr,
					  urma_token_t *token_value,
					  urma_active_tp_cfg_t *active_tp_cfg);
#endif /* __UDMA_U_JFR_H__ */
