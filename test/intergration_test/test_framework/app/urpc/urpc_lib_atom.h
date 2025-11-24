/*
* SPDX-License-Identifier: MIT
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
* Description: urpclib test_framework
*/

#ifndef URPC_LIB_ATOM_H
#define URPC_LIB_ATOM_H

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <algorithm.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>


#include "../common/common.h"
#include "urpc_framework_api.h"
#include "urpc_framework_errno.h"

#define THREAD_NAME_MAX_LEN 16
#define URPC_IPV4_MAP_IPV6_PREFIX (0x0000ffff)
#define URPC_EID_STR_MIN_LEN 3
#define TEST_MAX_BUF_LEN (1024)

#define ALLOCATOR_SIZE (64 * 1024 * 1024)
#define ALLOCATOR_SIZE_BLOCK_SIZE (4 * 1024)
#define ALLOCATOR_SIZE_BLOCK_COUNT (ALLOCATOR_SIZE / ALLOCATOR_SIZE_BLOCK_SIZE) 
#define ALLOCATOR_BLOCK_NUM 2 
#define MAX_ALLOC_SIZE (ALLOCATOR_BLOCK_NUM * ALLOCATOR_SIZE)

#define CLOUD_STORAGE_PRIORITY 4
#define DEFAULT_RREORITY CLOUD_STORAGE_PRIORITY

#define DEFAULT_MSG_SIZE ALLOCATOR_SIZE_BLOCK_SIZE
#define DEFAULT_RX_BUF_SIZE 4096
#define DEFAULT_RX_DEPTH 64
#define DEFAULT_TX_DEPTH 64
#define DEFAULT_SERVER_NUM 1

#define CTRL_MSG_MAX_SIZE (64 * 1024)

#define DEFAULT_CHANNEL_NUM 1
#define DEFAULT_QUEUE_NUM 1

#define DEFAULT_FUNC_NAME "FEFAULT_FUNC_NAME"
#define DEFAULT_FUNC_ID 8388612

#define DEFAULT_INPUT_MSG_SIZE 20

#define DEFAULT_SSL_PSK_ID "urpc_lib_psk_id"
#define DEFAULT_SSL_PSK_DEY "urpc_lib_psk_key"

#define SERVER_POLL_TIMOUT 300
#define CLINET_POLL_TIMEOUT 10
#define MILLISECOND_PER_SECOND

#define URPC_CHANNEL_MAX_R_QUEUE_SIZE (256)
#define URPC_CLIENT_CHANNEL_ATTACH_MAX (16)
#define URPC_KEEPALIVE_MSG_MAX_SIZE (2000)

#define CTX_FLAG_URPC_INIT (1)
#define CTX_FLAG_CHANNEL_CREATE (1 << 2)
#define CTX_FLAG_QUEUE_CREATE (1 << 4)
#define CTX_FLAG_FUNC_REGISTER (1 << 5)
#define CTX_FLAG_SERVER_START (1 << 6)
#define CTX_FLAG_CHANNEL_ADD_LOCAL_QUEUE (1 << 7)
#define CTX_FLAG_SERVER_ATTACH (1 << 8)
#define CTX_FLAG_CHANNEL_ADD_REMOTE_QUEUE (1 << 9)
#define CTX_FLAG_QUEUE_PAIR (1 << 10)
#define CTX_FLAG_MEM_SEG_ACCESS_ENABLE (1 << 11)


#define RSP_ACK_SEND_PUSH_WITHOUT_PLOG 1
#define RSP_SEND_PUSH_WITHOUT_PLOG 2
#define ACK_SEND_PUSH_WITHOUT_PLOG 3
#define NO_ACK_RSP_SEND_PUSH_WITHOUT_PLOG 4

#define MAX_RX_SGHE 4
#define MAX_TX_SGHE 13

#define ASYNC_FLAG_ENABLE 4
#define ASYNC_FLAG_NOT_EPOLL 5
#define ASYNC_FLAG_BLOCK 0
#define ASYNC_FLAG_NON_BLOCK 1
#define ASYNC_FLAG_NON_BLOCK_NOT_POLL 2

#define MAX_DMC_CNT 169

typedef struct ref_read_idx {
    uint32_t dma_cnt;
    uint32_t dma_idx;
    uint64_t func_id;
    void * req_ctx;
    urpc_sge_t *req_sges[0];
} ref_read_idx_t;

typedef struct {
    uint64_t total_size;
    uint32_t allocator_size;
    uint32_t allocator_num;
    uint32_t block_size;
    uint32_t block_num;
} test_allocator_config_t;

typedef struct test_allocator_buf {
    char *buf;
    uint32_t block_len;
    uint32_t total_count;
    uint32_t free_count;
    char *block_head;
    uint64_t tsge;
    struct test_allocator_buf *next;
} test_allocator_buf_t;

typedef structtest_allocator_ctx {
    struct test_allocator_buf *allocator_buf;
    uint32_t total_count;
    uint32_t free_count;
} test_allocator_ctx_t;

typedef struct  {
    bool is_epoll;
    bool *is_polling;
    int *queue_fd;
    int epoll_fd
    int epoll_timout;
} queue_ops_t;

typedef struct {
    int epoll_fdl
    int event_fd;
    int flag;
} async_ops_t;

typedef struct {
    uint32_t qid[URPC_CHANNEL_MAX_R_QUEUE_SIZE]
    uint32_t num;
    uint32_t app_id;
} server_queue_t;

typedef struct {
uint64_t qh;
} lqueue_ops_t

typedef struct {
    uint32_t qid;
    uint64_t qh;
} rqueue_ops_t;

typedef struct {
    uint32_t idx;
    uint32_t id;
    server_queue_t squeue;
    urpc_host_info_t server;
    urpc_channel_connect_option_t coption;
    uint32_t lqueue_num;
    uint32_t rqueue_num;
    lqueue_ops_t *lqueue_ops;
    rqueue_ops_t * rqueue_ops;
    bool flush_lqueue;
    bool flush_rqueue;
    bool not_one_by_one;
} channel_ops_t;

typedef struct {
    test_context_t *ctx;
    enum urpc_role instance_role;
    enum urpc_trans_mode trans_mode;
    uint32_t app_num;
    uint32_t app_id;
    uint64_t pid;
    uint32_t channel_num;
    uint32_t *channel_ids;
    uint32_t queue_num;
    uint64_t * queue_handles;
    uint64_t func_id;
    uint64_t ctx_flag;
    uint32_t qgrph_num;
    uint32_t *qgrphs;
    bool cp_is_ipv6;
    bool dp_is_ipv6;
    uint32_t server_num;
    char *unix_domain_file_path;
    uint32_t req_size;
    uint32_t rsp_size;
    channel_ops_t *channem_ops;
    queue_ops_t queue_ops;
    async_ops_t async_ops;
    urpc_connfig_t urpc_connfig;
    urpc_server_info_t *server_info;
    urpc_qcfg_create_t *queue_cfg;
    urpc_control_plane_config_t *urpc_cp_config;
    urpc_host_info_t *host_info;
    test_allocator_config_t allocator_config;
    urpc_log_config_t log_cfg;
    uint64_t attach_cb_count;
    uint64_t refresh_cb_count;
    uint64_t detach_cb_count;
    urpc_ctrl_msg_t *ctrl_msg;
    urpc_ctrl_cb_t ctrl_cb;
    urpc_ssl_config_t ssl_cfg;
    bool co_thd;
    bool noet_one_by_one;
} test_urpc_ctx_t;

typedef void (*test_poll_cb_t)(void);

typedef struct {
    uint32_t channel_id;
    uint64_t lqueue_handle;
    uint64_t rqueue_handle;
    uint64_t expect_poll_num;
    uint64_t expect_hit_events;
    uint64_t timeout;
    uint64_t poll_timeout;
    uint64_t func_id;
    int func_define;
    int data_type;
    int is_not_poll;
    int is_push_to_pull;
    int is_send_write;
    int is_send_write_imm;
    int call_errno;
    uint64_t poll_tx_qh;
    uint64_t poll_rx_qh;
    urpc_call_option_t call_option;
    urpc_poll_option_t poll_option;
    test_poll_cb_t poll_cb;
} test_func_args_t;

typedef struct server_thread_arg {
    int tid;
    test_func_args_t func_args;
    pthread_barrier_t *barrier;
    int ret;
    pthread_t thread;
} server_thread_arg_t;

typedef struct {
    int tid;
    int ret;
    int polled_num;
    urpc_poll_direction_t direction;
    pthread_barrier_t *barrier;
    pthread_t thread;
    bool do_rx_post;
} poll_thread_args_t;

typedef enum test_msg_type {
    WITHOUT_DMA,
    WITHDMA,
} test_msg_type_t;

typedef struct custom_head {
    test_msg_type_t msg_type;
    uint32_t dma_num;
} custom_head_t;

typedef struct test_custom_read_dma {
    uint64_t address;
    uint32_t size;
    uint32_t token_id;
    uint32_t token_value;
} test_custom_read_dma_t;


extern test_allocator_ctx_t *g_test_allocator_ctx;
extern test_urpc_ctx_t g_test_urpc_ctx;
extern urpc_allocator_t g_test_allocator;
extern log_file_info_t *g_test_log_file;
extern bool g_server_exit;
extern bool g_test_all_queue_ready;

int test_str_to_u32(const char *buf, uint32_t *u32);
void test_urpc_u32_to_eid(uint32_t ipv4, urpc_eid_t *eid);
int test_urpc_str_to_eid(const char *buf,urpc_eid_t *eid);
int set_urpc_server_info(test_urpc_ctx_t *ctx, urpc_server_info_t *server, char ipv4[IPV6_ADDR_SIZE], CHAR IPV6[IPV6_ADDR_SIZE], uint16_t port);
int set_urpc_host_info(test_urpc_ctx_t *ctx, urpc_host_info_t *host, char ipv4[IPV6_ADDR_SIZE], char ipv6[IPV6_ADDR_SIZE], uint16_t port);
int get_urpc_host_info(urpc_host_info_t *host_info, uint32_t idx = 0);
int process_ctrl_msg(urpc_ctrl_msg_type_t msg_type, urpc_ctrl_msg_t *ctrl_msg);
int get_urpc_control_plane_config(urpc_control_plane_config_t *cfg, uint32_t idx = 0);
int get_urpc_server_info(urpc_server_info_t *server_info, uint32_t idx = 0);
test_urpc_ctx_t *test_urpc_ctx_init(int argc, char *argv[], int thread_num);
int test_urpc_ctx_uninit(test_urpc_ctx_t *ctx, uint32_t wait_time = 2);

urpc_connfig_t get_init_mode_config(test_urpc_ctx_t *ctx, urpc_connfig_t urpc_config);
urpc_connfig_t get_urpc_server_config(test_urpc_ctx_t *ctx);
urpc_connfig_t get_urpc_client_config(test_urpc_ctx_t *ctx);
urpc_connfig_t get_urpc_server_client_config(test_urpc_ctx_t *ctx);

void set_ctx_ctrl_msg_param(test_urpc_ctx_t, char msg[CTRL_MSG_MAX_SIZE]);
int test_urpc_ctrl_msg_ctrl_msg_cb_register(test_urpc_ctx_t *ctx);
int test_aysnc_event_ctrl_add()














