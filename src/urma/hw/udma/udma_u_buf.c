// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 */

#include <stdio.h>
#include "udma_u_buf.h"

bool udma_u_alloc_queue_buf(struct udma_u_jetty_queue *q, uint32_t max_entry_cnt,
			    uint32_t baseblk_size, uint32_t page_size,
			    bool wrid_en)
{
	uint32_t buf_shift;
	uint32_t entry_cnt;

	entry_cnt = roundup_pow_of_two(max_entry_cnt);
	buf_shift = align_power2(entry_cnt * baseblk_size);
	q->baseblk_shift = align_power2(baseblk_size);
	q->qbuf_size = align((1U << buf_shift), page_size);
	q->baseblk_cnt = q->qbuf_size >> q->baseblk_shift;
	q->baseblk_mask = q->baseblk_cnt - 1U;

	if (wrid_en) {
		q->wrid = (uintptr_t *)malloc(q->baseblk_cnt * sizeof(uint64_t));
		if (!q->wrid) {
			UDMA_LOG_ERR("failed to alloc buffer for wrid.\n");
			return false;
		}
	}

	q->qbuf = udma_u_alloc_buf(q->qbuf_size);
	if (!q->qbuf) {
		UDMA_LOG_ERR("failed to alloc queue buffer.\n");
		if (wrid_en) {
			free(q->wrid);
			q->wrid = NULL;
		}
		return false;
	}
	q->qbuf_curr = q->qbuf;
	q->qbuf_end = q->qbuf + q->qbuf_size;

	return true;
}

void udma_u_free_queue_buf(struct udma_u_jetty_queue *q)
{
	if (q->wrid != NULL) {
		free(q->wrid);
		q->wrid = NULL;
	}

	if (q->qbuf != NULL) {
		udma_u_free_buf(q->qbuf, q->qbuf_size);
		q->qbuf = NULL;
		q->qbuf_curr = NULL;
		q->qbuf_end = NULL;
	}
}

void *udma_u_alloc_buf(uint32_t buf_size)
{
	void *buf;
	int ret;

	buf = mmap(NULL, buf_size, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (buf == MAP_FAILED) {
		UDMA_LOG_ERR("mmap failed, buf_size=%u.\n", buf_size);
		return NULL;
	}

	ret = madvise(buf, buf_size, MADV_DONTFORK);
	if (ret) {
		(void)munmap(buf, buf_size);
		UDMA_LOG_ERR("buf madvise failed! ret = %d\n", ret);
		return NULL;
	}

	return buf;
}
