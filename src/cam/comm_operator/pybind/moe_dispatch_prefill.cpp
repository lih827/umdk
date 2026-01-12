/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: add moe_dispatch_prefill pybind extention file
 * Create: 2026-01-08
 * Note:
 * History: 2026-01-08 create moe_dispatch_prefill pybind extention file
 */

#include "pytorch_npu_helper.hpp"
#include "torch_npu/csrc/core/npu/NPUStream.h"
#include "utils.h"
#include <hccl/hccl.h>
#include <iostream>
#include <torch/csrc/autograd/custom_function.h>
#include <torch/extension.h>
#include <unistd.h>

using torch::autograd::AutogradContext;
using torch::autograd::Function;
using tensor_list = std::vector<at::Tensor>;
using namespace at;
using namespace std;

const uint32_t NO_SCALES = 0;
const uint32_t DYNAMIC_SCALES = 2;
const uint32_t EXPAND_IDX_COUNT_PER_GROUP = 3;

std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor>
MoeDispatchPrefillImplNpu(const at::Tensor &x, const at::Tensor &topk_idx, const at::Tensor &topk_weights,
                          const at::Tensor &num_tokens_per_expert, const at::Tensor &send_token_idx_small,
                          c10::string_view groupEp, int64_t rank, int64_t num_ranks, bool use_quant)
{
    std::vector<char> group_ep_chrs(groupEp.begin(), groupEp.end());
    group_ep_chrs.push_back('\0');
    char *group_ep_ptr = &group_ep_chrs[0];

    // Convert topk_idx to int32 if necessary
    at::Tensor topk_idx_int32 = topk_idx.scalar_type() == at::kInt ? topk_idx : topk_idx.to(at::kInt);
    at::Tensor expert_ids = topk_idx_int32;
    int64_t tp_size = 1;
    int64_t tp_rank = 0;
    int64_t quant_mode = use_quant ? DYNAMIC_SCALES : NO_SCALES;

    // Type checks
    TORCH_BIND_ASSERT(num_tokens_per_expert.scalar_type() == at::kInt);

    // Shape and contiguous checks
    TORCH_BIND_ASSERT(x.dim() == 2 and x.is_contiguous());
    TORCH_BIND_ASSERT(num_tokens_per_expert.dim() == 1 and num_tokens_per_expert.is_contiguous());
    TORCH_BIND_ASSERT(num_tokens_per_expert.size(0) % num_ranks == 0);

    auto num_tokens = static_cast<int>(x.size(0));
    auto hidden = static_cast<int>(x.size(1));
    auto num_experts = static_cast<int64_t>(num_tokens_per_expert.size(0));
    auto num_local_experts = static_cast<int>(num_experts / num_ranks);

    // Top-k checks
    int num_topk = static_cast<int>(topk_idx_int32.size(1));
    TORCH_BIND_ASSERT(num_experts > 0);
    TORCH_BIND_ASSERT(topk_idx_int32.dim() == 2 and topk_idx_int32.is_contiguous());
    TORCH_BIND_ASSERT(topk_weights.dim() == 2 and topk_weights.is_contiguous());
    TORCH_BIND_ASSERT(num_tokens == topk_idx_int32.size(0));
    TORCH_BIND_ASSERT(num_topk == topk_weights.size(1));

    int send_per_group = EXPAND_IDX_COUNT_PER_GROUP; // (send_to_expert_num, send_to_expert_offset, send_rank_tokens)

    auto send_data = torch::empty({num_experts * send_per_group}, at::dtype(at::kInt).device(x.device()));
    int64_t send_count = send_per_group * num_local_experts * num_ranks;

    auto send_data_offset = torch::empty({num_experts}, at::dtype(at::kInt).device(x.device()));
    at::Tensor recv_data = torch::empty({num_experts * send_per_group}, at::dtype(at::kInt).device(x.device()));
    at::Tensor total_recv_token_ = torch::empty({1}, at::dtype(at::kInt).device(x.device()));
    at::Tensor recv_count_ = torch::empty({num_experts}, at::dtype(at::kInt).device(x.device()));
    at::Tensor recv_offset_ = torch::empty({num_experts}, at::dtype(at::kInt).device(x.device()));
    at::Tensor max_bs_ = torch::empty({1}, at::dtype(at::kInt).device(x.device()));
    at::Tensor recv_tokens_per_expert_ = torch::empty({num_local_experts}, at::dtype(at::kLong).device(x.device()));

    int64_t local_rank_size = num_ranks;
    int64_t local_rank_id = rank % local_rank_size;

    at::Tensor dispatch_wait_recv_cost_stats_out;

    EXEC_NPU_CMD(aclnnNotifyDispatch, send_data, num_tokens_per_expert, send_count, num_tokens,
                 group_ep_ptr, // commGroup
                 num_ranks,    // rankSize
                 rank,         // rankId
                 local_rank_size, local_rank_id, send_data_offset, recv_data, total_recv_token_, recv_count_,
                 recv_offset_, max_bs_, recv_tokens_per_expert_);

    int64_t gBs = max_bs_.item<int>() * num_ranks;
    int64_t trt = total_recv_token_.item<int>();
    int num_recv_tokens = (trt == 0) ? 1 : trt;
    auto expandx_out = use_quant ? torch::empty({num_recv_tokens, hidden}, at::dtype(at::kChar).device(x.device()))
                                 : torch::empty({num_recv_tokens, hidden}, x.options());
    auto dynamic_scales_out = torch::empty({num_recv_tokens}, at::dtype(at::kFloat).device(x.device()));
    auto expand_idx_out = torch::empty({num_recv_tokens * 3}, at::dtype(at::kInt).device(x.device()));

    EXEC_NPU_CMD(aclnnMoeDispatchNormal, x, expert_ids, send_data_offset, send_token_idx_small, recv_offset_,
                 recv_count_, group_ep_ptr,
                 num_ranks, // rankSize
                 rank,      // rankId
                 group_ep_ptr, tp_size, tp_rank, num_experts, quant_mode, gBs, expandx_out, dynamic_scales_out,
                 expand_idx_out, dispatch_wait_recv_cost_stats_out);

    // Return values
    return {expandx_out, dynamic_scales_out, expand_idx_out, recv_count_, recv_tokens_per_expert_};
}

tensor_list MoeDispatchPrefillBackwardImplNpu(const at::Tensor &self)
{
    at::Tensor result = at::Tensor(self); // 创建输出内存
    return {result, result, result, result, result};
}

/* Normal类算子形状无法提前推导，不支持meta设备注册不支持GE使用 */
std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor>
MoeDispatchPrefillImpl(const at::Tensor &x, const at::Tensor &topk_idx, const at::Tensor &topk_weights,
                       const at::Tensor &num_tokens_per_expert, const at::Tensor &send_token_idx_small,
                       c10::string_view groupEp, int64_t rank, int64_t num_ranks, bool use_quant)
{
    static auto op = torch::Dispatcher::singleton()
                         .findSchemaOrThrow("umdk_cam_op_lib::moe_dispatch_prefill", "")
                         .typed<decltype(MoeDispatchPrefillImpl)>();
    return op.call(x, topk_idx, topk_weights, num_tokens_per_expert, send_token_idx_small, groupEp, rank, num_ranks,
                   use_quant);
}

// 通过继承torch::autograd::Function类实现前反向绑定
class ExtMoeDispatchPrefill : public torch::autograd::Function<ExtMoeDispatchPrefill> {
  public:
    static tensor_list forward(AutogradContext *ctx, const at::Tensor &x, const at::Tensor &topk_idx,
                               const at::Tensor &topk_weights, const at::Tensor &num_tokens_per_expert,
                               const at::Tensor &send_token_idx_small, c10::string_view groupEp, int64_t rank,
                               int64_t num_ranks, bool use_quant)
    {
        at::AutoDispatchBelowADInplaceOrView guard;
        auto result = MoeDispatchPrefillImpl(x, topk_idx, topk_weights, num_tokens_per_expert, send_token_idx_small,
                                             groupEp, rank, num_ranks, use_quant);

        return {std::get<0>(result), std::get<1>(result), std::get<2>(result), std::get<3>(result),
                std::get<4>(result)};
    }

    static tensor_list backward(AutogradContext *ctx, tensor_list grad_outputs)
    {
        return {at::Tensor(), at::Tensor(), at::Tensor(), at::Tensor(), at::Tensor()};
    }
};

std::tuple<at::Tensor, at::Tensor, at::Tensor, at::Tensor, at::Tensor>
MoeDispatchPrefillImplAutograd(const at::Tensor &x, const at::Tensor &topk_idx, const at::Tensor &topk_weights,
                               const at::Tensor &num_tokens_per_expert, const at::Tensor &send_token_idx_small,
                               c10::string_view groupEp, int64_t rank, int64_t num_ranks, bool use_quant)
{
    auto result = ExtMoeDispatchPrefill::apply(x, topk_idx, topk_weights, num_tokens_per_expert, send_token_idx_small,
                                               groupEp, rank, num_ranks, use_quant);
    return std::make_tuple(result[0], result[1], result[2], result[3], result[4]);
}

// moe_dispatch_prefill
TORCH_LIBRARY_IMPL(umdk_cam_op_lib, PrivateUse1, m)
{
    m.impl("moe_dispatch_prefill", &MoeDispatchPrefillImplNpu);
    m.impl("moe_dispatch_prefill_backward", &MoeDispatchPrefillBackwardImplNpu);
}

TORCH_LIBRARY_IMPL(umdk_cam_op_lib, AutogradPrivateUse1, m)
{
    m.impl("moe_dispatch_prefill", &MoeDispatchPrefillImplAutograd);
}