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

union udma_jfs_flag {
	struct {
		uint32_t sq_cstm : 1;
		uint32_t db_cstm : 1;
		uint32_t db_ctl_cstm : 1;
		uint32_t reserved : 29;
	} bs;
	uint32_t value;
};

struct udma_u_jfs_cstm_cfg {
	union udma_jfs_flag flag;
	struct udma_u_que_cfg_ex sq;
	uint32_t tgid;
};

enum udma_u_jetty_type {
	UDMA_U_CACHE_LOCK_DWQE_JETTY_TYPE,
	UDMA_U_CCU_JETTY_TYPE,
	UDMA_U_NORMAL_JETTY_TYPE,
};

struct udma_u_jfs_cfg_ex {
	uint32_t id;
	urma_jfs_cfg_t base_cfg;
	struct udma_u_jfs_cstm_cfg cstm_cfg;
	enum udma_u_jetty_type jetty_type;
	bool pi_type;
	uint32_t sqebb_num;
};

struct udma_u_jetty_cfg_ex {
	urma_jetty_cfg_t base_cfg;
	struct udma_u_jfr_cstm_cfg jfr_cstm; /* control noshare jfr of jetty */
	struct udma_u_jfs_cstm_cfg jfs_cstm; /* control jfs of jetty */
	enum udma_u_jetty_type jetty_type;
	bool pi_type;
	uint32_t sqebb_num;
};

enum udma_u_jfc_type {
	UDMA_U_NORMAL_JFC_TYPE,
	UDMA_U_STARS_JFC_TYPE,
	UDMA_U_CCU_JFC_TYPE,
};

struct udma_u_jfc_cfg_ex {
	urma_jfc_cfg_t base_cfg;
	enum udma_u_jfc_type jfc_mode;
};

struct udma_u_jfs_wr_ex {
	urma_jfs_wr_t wr;
	bool reduce_en;
	uint8_t reduce_opcode;
	uint8_t reduce_data_type;
};

struct udma_u_wr_ex {
	bool is_jetty;
	bool db_en;
	union {
		urma_jfs_t *jfs;
		urma_jetty_t *jetty;
	};
	struct udma_u_jfs_wr_ex *wr;
	struct udma_u_jfs_wr_ex **bad_wr;
};

struct udma_u_post_info {
	uint64_t *dwqe_addr;
	void volatile *db_addr;
	uint64_t *ctrl;
	uint32_t pi;
};

struct udma_u_jetty_info {
	urma_jetty_t *jetty;
	void *dwqe_addr;
	void *db_addr;
};

struct udma_u_jfs_info {
	urma_jfs_t *jfs;
	void *dwqe_addr;
	void *db_addr;
};

enum udma_u_user_ctl_opcode {
	UDMA_U_USER_CTL_CREATE_JFR_EX,
	UDMA_U_USER_CTL_DELETE_JFR_EX,
	UDMA_U_USER_CTL_CREATE_JFS_EX,
	UDMA_U_USER_CTL_DELETE_JFS_EX,
	UDMA_U_USER_CTL_CREATE_JFC_EX,
	UDMA_U_USER_CTL_DELETE_JFC_EX,
	UDMA_U_USER_CTL_CREATE_JETTY_EX,
	UDMA_U_USER_CTL_DELETE_JETTY_EX,
	UDMA_U_USER_CTL_POST_WR,
	UDMA_U_USER_CTL_MAX,
};

#endif /* __UDMA_U_CTL_H__ */
