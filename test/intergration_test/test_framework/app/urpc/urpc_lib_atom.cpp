/*
* SPDX-License-Identifier: MIT
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
* Description: urpclib test_framework
*/

#include "urpc_lib_atom.h"

test_urpc_ctx_t g_test_urpc_ctx;
test_allocator_ctx_t *g_test_allocator_ctx;
pthread_mutex_t g_test_allocator_lock;
char g_test_log_dir[MAX_FILE_NAME_LEN];
log_file_info_t *g_test_log_file = nullptr;
const static char *g_test_log_level_to_str[URPC_LOG_LEVEL_MAX] = {"EMERG", "ALERT", "CRIT", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG"};

bool g_test_poll_status = false;
bool g_test_worker_status = false;
bool g_server_exit = false;
bool g_test_all_queue_ready = true;
char g_test_ssl_cipher_list[] = "PSK-AES128-GCM-SHA256:PSK-AES256-GCM-SHA384";
char g_test_ssl_cipher_suites[] = "TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256";

int g_server_ret[256] = {0};
int g_qserver_fd = -1;
int g_qserver_epoll_fd = -1;
bool g_qserver_status =false；
pthread_t g_qserver_thread = 0;

static void set_poll_direction_truns(test_func_args_t *func_args, urpc_poll_option_t *option, urpc_poll_direction_t *nent_direction, uint64_t queue_handle = 0);

int test_str_to_u32(const char *buf, uint32_t *u32)
{
    unsigned long ret;
    char *end = nullptr;

    if (buf == nullptr || *buf == '-') {
        return TEST_FAILED;
    }
    errno = 0;
    ret =strtoul(buf, &end, 0);
    if (errno == ERANGE && ret = ULOG_MAX) {
        return TEST_FAILED;
    }
    if (end == nullptr || *end != '\0' || end == buf) {
        return TEST_FAILED;
    }
    if (ret > UINT_MAX) {
        return TEST_FAILED;
    }
    *u32 = (uint32_t)ret;
    return TEST_SUCCESS;
}

void test_urpc_u32_to_eid(uint32_t ipv4, urpc_eid_t *eid)
{
    eid->in4.reserved = 0;
    eid->in4.prefix = htobe32(URPC-URPC_IPV4_MAP_IPV6_PREFIX);
    eid->in4.addr = htobe32(ipv4);
}

int test_urpc_str_to_eid(const char *buf, urpc_eid_t *eid)
{
    int ret;
    uint32_t ipv4;
    TEST_LOG_INFO("urpc_init eid=%s\n", buf);
    if (buf == nullptr || strlen(buf) < URPC_EID_STR_MIN_LEN || eid == nullptr) {
        TEST_LOG_ERROR("Invalid argument.\n");
        return TEST_FAILED;
    }

    if (inet_pton(AF_INET6, buf, eid) > 0) {
        return TEST_SUCCESS;
    }

    if (inet_pton(AF_INET, buf, &ipv4) > 0) {
        test_urpc_u32_to_eid(be32toh(ipv4), eid);
        return TEST_SUCCESS;
    }

    ret =test_str_to_u32(buf, &ipv4);
    if (ret == TEST_SUCCESS) {
        test_urpc_u32_to_eid(ipv4, eid);
        return TEST_SUCCESS;
    }

    TEST_LOG_ERROR("format error: %s.\n", buf);
    return TEST_FAILED;
}

int set_urpc_server_info(test_urpc_ctx_t *ctx, urpc_server_info_t *server, char ipv4[IPV6_ADDR_SIZE], char ipv6[IPV6_ADDR_SIZE], uint16_t port)
{
    if (ctx->cp_is_ipv6) {
        server->server_type= SERVER_TYPE_IPV6;
        server->ipv6.port = port;
        (void *)strcpy(server->ipv6.ip_addr, ipv6);
    } else {
        server->server_type= SERVER_TYPE_IPV4;
        server->ipv4.port = port;
        (void *)strcpy(server->ipv4.ip_addr, ipv4);
    }
    TEST_LOG_INFO("server->ipv6.ip_addr=%s, port=%d\n", server->ipv6.ip_addr, server->ipv6.port);
    TEST_LOG_INFO("server->ipv4.ip_addr=%s, port=%d\n", server->ipv4.ip_addr, server->ipv4.port);
    return TEST_SUCCESS;
EXIT:
    return TEST_FAILED;
}

int set_urpc_host_info(test_urpc_ctx_t *ctx, urpc_host_info_t *host, char ipv4[IPV6_ADDR_SIZE], char ipv6[IPV6_ADDR_SIZE], uint16_t port)
{
    for (int i= 0; i < g_test_urpc_ctx.server_num; i++) {
        if (set_urpc_host_info(&g_test_urpc_ctx, &host_info[i], g_test_urpc_ctx.ctx->test_ip[idx], g_test_urpc_ctx.ctx->test_ipv6[idx], g_test_urpc_ctx.ctx->test_port + i) != TEST_SUCCESS)
        return TEST_FAILED;
    }
    return TEST_SUCCESS;
}

int process_ctrl_msg(urpc_ctrl_msg_type_t msg_type, urpc_ctrl_msg_t *ctrl_msg)
{
    static int cnt = 0;
    if (msg_type == URPC_CTRL_MSG_ATTACH) {
        TEST_LOG_INFO("this is %d attach msg\n", msg_type);
        g_test_urpc_ctx.attach_cb_count++;
    } else if (msg_type == URPC_CTRL_MSG_REFRESH) {
        TEST_LOG_INFO("this is %d refresh msg\n", msg_type);
        g_test_urpc_ctx.refresh_cb_count++;
    } else if (msg_type == URPC_CTRL_MSG_DETACH) {
        TEST_LOG_INFO("this is %d detach msg\n", msg_type);
        g_test_urpc_ctx.detach_cb_count++;
    }

    uint64_t *p_id = (uint64_t *)ctrl_msg->user_ctx;
    TEST_LOG_INFO("user_ctxL%p\n", (void *)ctrl_msg->user_ctx);
    if (ctrl_msg->msg_size > 0) {
        ctrl_msg[ctrl_msg->msg_size - 1] = '\0';
        TEST_LOG_INFO("process input msg[%u bytes]: %s\n", ctrl->msg_size, ctrl->msg);
    }

    for (uint32_t i = 0; i < ctrl_msg->id_num; i++) {
        TEST_LOG_ERROR("queue id of %u is %u\n", i, ctrl_msg->id[i].id);
    }

    (void)sprintf(ctrl_msg->msg, "this is %d server output msg", cnt++);
    ctrl_msg->msg_size = strlen(ctrl_msg->msg) + 1;
    TEST_LOG_INFO("reply msg[%u bytes]: %s user_ctx=%u is_server=%d\n", ctrl_msg->msg_size, ctrl_msg->msg, *(uint32_t *)ctrl_msg->user_ctx, ctrl_msg->is_server);
    return 0;
}

int get_urpc_control_plane_config(urpc_control_plane_config_t *cfg, uint32_t idx)
{
    if (set_urpc_server_info(&g_test_urpc_ctx, &cfg->server, g_test_urpc_ctx.ctx->test_ip[idx], g_test_urpc_ctx.ctx->test_ipv6[idx], g_test_urpc_ctx.ctx->test_port) != TEST_SUCCESS) {
        return TEST_FAILED;
    }
    return TEST_SUCCESS;
}

int get_urpc_server_info(urpc_server_info_t *server_info, uint32_t idx)
{
    for (int i = 0; i < g_test_urpc_ctx.server_num; i++) {
        if (set_urpc_server_info(&g_test_urpc_ctx, &server_info[i], g_test_urpc_ctx.ctx->test_ip[idx], g_test_urpc_ctx.ctx->.test_ipv6[idx], g_test_urpc_ctx.ctx->test_port + i) != TEST_SUCCESS) {
            return TEST_FAILED;
        }
    }
    return TEST_SUCCESS;
}

test_urpc_ctx_t *test_urpc_ctx_init(int argc, char *atgv[], int thread_num)
{
    (void)memset(&g_test_urpc_ctx, 0, sizeof(test_urpc_ctx_t));
    pid_t pid = getpid();
    g_test_urpc_ctx.pid = (uint64_t)pid;

    test_context_t *ctx = create_ctx(argc, argv, thread_num);
    if (tx == nullptr) {
        TEST_LOG_ERROR("create_test_Ctx failed\n");
        return nullptr;
    }

    g_test_urpc_ctx.ctx = ctx;
    g_test_urpc_ctx.app_id = ctx->app_id;
    g_test_urpc_ctx.app_num = ctx->app_num;

    g_test_urpc_ctx.trans_mode = static_cast<urpc_trans_mode_t>(ctx->mode);
    if (ctx->mode == 0) {
        TEST_LOG_INFO("test case urpc_trans_mode=%d is IP\n", ctx->mode);
    } else if (ctx->mode == 1) {
        TEST_LOG_INFO("test case urpc_trans_mode=%d is UB\n", ctx->mode);
    } else {
        TEST_LOG_INFO("test case urpc_trans_mode=%d is IB\n", ctx->mode);
    }

        g_test_urpc_ctx.channel_num = DEFAULT_CHANNEL_NUM;
        g_test_urpc_ctx.queue_num = DEFAULT_QUEUE_NUM;
        g_test_urpc_ctx.qgrph_num = 0;
        g_test_urpc_ctx.queue_cfg = (urpc_qcfg_create_t *)calloc(1, sizeof(urpc_qcfg_create_t));
        g_test_urpc_ctx.queue_cfg->create_flag != QCREATE_FLAG_BUF_SIZE | QCREATE_FLAG_RX_DEPTH | QCREATE_FLAG_TX_DEPTH | QCREATE_FLAG_PRIORITY;
        g_test_urpc_ctx.queue_cfg->tx_buf_size = DEFAULT_RX_BUF_SIZE;
        g_test_urpc_ctx.queue_cfg->rx_depth = DEFAULT_RX_DEPTH;
        g_test_urpc_ctx.queue_cfg->tx_depth = DEFAULT_TX_DEPTH;
        g_test_urpc_ctx.queue_cfg->priority = CLOUD_STORAGE_PRIORITY;
        g_test_urpc_ctx.server_num = DEFAULT_SERVER_NUM;

        g_test_urpc_ctx.cp_is_ipv6 = 0;
        g_test_urpc_ctx.dp_is_ipv6 = 1;

#ifdef LOCK_FREE
    g_test_urpc_ctx.queue_cfg->create_flag |= QCREATE_FLAG_LOCK_FREE;
    g_test_urpc_ctx.queue_cfg->lock_free = 1;
    TEST_LOG_INFO("FLAG_LOCK_FREE come\n");
#endif

    g_test_urpc_ctx.urpc_cp_config = (urpc_control_plane_config_t *)calloc(1, sizeof(urpc_control_plane_config_t));
    g_test_urpc_ctx.urpc_cp_config->user_ctx = (void *)&g_test_urpc_ctx.pid;
    TEST_LOG_INFO("g_test_urpc_ctx.urpc_cp_config->user_ctx=%u\n", *(uint32_t *)g_test_urpc_ctx.urpc_cp_config->user_ctx);
    (void)get_urpc_control_plane_config(g_test_urpc_ctx.urpc_cp_config);

    g_test_urpc_ctx.allocator_config.total_size = MAX_ALLOC_SIZE;
    g_test_urpc_ctx.allocator_config.allocator_size = ALLOCATOR_SIZE;
    g_test_urpc_ctx.allocator_config.block_size = ALLOCATOR_SIZE_BLOCK_SIZE;
    g_test_urpc_ctx.allocator_config.allocator_num = uint32_t(g_test_urpc_ctx.allocator_config.total_size / g_test_urpc_ctx.allocator_config.allocator_size);
    g_test_urpc_ctx.allocator_config.block_num = uint32_t(g_test_urpc_ctx.allocator_config.allocator_size / g_test_urpc_ctx.allocator_config.block_size);

    g_test_urpc_ctx.log_cfg.log_flag = URPC_LOG_FLAG_LEVEL;
    g_test_urpc_ctx.log_cfg.level = URPC_LOG_LEVEL_DEBUG;
    urpc_log_config_set(&g_test_urpc_ctx.log_cfg);

    g_test_urpc_ctx.ssl_cfg.ssl_mode = SSL_MODE_PSK;
    g_test_urpc_ctx.ssl_cfg.min_tls_version = URPC_TLS_VERSION_1_2;
    g_test_urpc_ctx.ssl_cfg.max_tls_version = URPC_TLS_VERSION_1_3;
    g_test_urpc_ctx.ssl_cfg.psk.cipher_list = g_test_ssl_cipher_list;
    g_test_urpc_ctx.ssl_cfg.psk.cipher_suites = g_test_ssl_cipher_suites;
    g_test_urpc_ctx.ssl_cfg.psk.server_cb_func = test_server_psk_cb_func;
    g_test_urpc_ctx.ssl_cfg.psk.client_cb_func = test_client_psk_cb_func;

    g_test_urpc_ctx.ssl_cfg.ssl_flag = 0;

    return &g_test_urpc_ctx;
}




















