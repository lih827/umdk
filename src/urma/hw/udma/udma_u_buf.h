/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef __UDMA_U_BUF_H__
#define __UDMA_U_BUF_H__

#include <sys/mman.h>
#include "udma_u_common.h"

bool udma_u_alloc_queue_buf(struct udma_u_jetty_queue *q, uint32_t max_entry_cnt,
			    uint32_t baseblk_size, uint32_t page_size, bool wrid_en);
void udma_u_free_queue_buf(struct udma_u_jetty_queue *q);
void *udma_u_alloc_buf(uint32_t buf_size);

static inline void udma_u_free_buf(void *buf, uint32_t buf_size)
{
	(void)munmap(buf, buf_size);
}

#endif /* __UDMA_U_BUF_H__ */
