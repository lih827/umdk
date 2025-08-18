/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef __UDMA_U_JETTY_H__
#define __UDMA_U_JETTY_H__

#include "urma_types.h"
#include "udma_u_common.h"

#define INVALID_TPN UINT32_MAX
#define MAX_JETTY_IN_GRP 32

urma_jetty_t *udma_u_create_jetty(urma_context_t *ctx, urma_jetty_cfg_t *cfg);
urma_status_t udma_u_delete_jetty(urma_jetty_t *jetty);

urma_status_t udma_u_unbind_jetty(urma_jetty_t *jetty);
urma_jetty_grp_t *udma_u_create_jetty_grp(urma_context_t *ctx,
					  urma_jetty_grp_cfg_t *cfg);
urma_status_t udma_u_delete_jetty_grp(urma_jetty_grp_t *jetty_grp);
int exec_jetty_create_cmd(urma_context_t *ctx, struct udma_u_jetty *jetty,
			  urma_jetty_cfg_t *cfg);
int init_jetty_trans_mode(struct udma_u_jetty *jetty,
			  urma_jetty_cfg_t *cfg);
int add_jetty_to_grp(struct udma_u_jetty *jetty, urma_jetty_cfg_t *cfg);
void remove_jetty_from_grp(struct udma_u_jetty *jetty);
#endif /* __UDMA_U_JETTY_H__ */
