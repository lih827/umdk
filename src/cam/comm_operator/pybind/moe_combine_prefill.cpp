/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: add moe_combine_prefill pybind extention file
 * Create: 2026-01-08
 * Note:
 * History: 2026-01-08 create moe_combine_prefill pybind extention file
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

at::Tensor MoeCombinePrefillImplNpu(const at::Tensor &x, const at::Tensor &topk_idx, const at::Tensor &topk_weights,
                                    const at::Tensor &src_idx, const at::Tensor &send_head, c10::string_view groupEp,
                                    int64_t rank, int64_t num_ranks)
{
    std::vector<char> group_ep_chrs(groupEp.begin(), groupEp.end());
    group_ep_chrs.push_back('\0');
    char *group_ep_ptr = &group_ep_chrs[0];

    TORCH_BIND_ASSERT(x.dim() == 2 and x.is_contiguous());

    // Convert topk_idx to int32 if necessary
    at::Tensor topk_idx_int32 = topk_idx.scalar_type() == at::kInt ? topk_idx : topk_idx.to(at::kInt);
    at::Tensor token_src_info = src_idx;
    at::Tensor ep_send_counts = send_head;
    auto device = x.device();

    const int num_tokens = topk_idx_int32.size(0);
    const int num_topk = topk_idx_int32.size(1);

    // Convert topk_weights to float if necessary
    at::Tensor expert_scales = topk_weights.scalar_type() == at::kFloat ? topk_weights : topk_weights.to(at::kFloat);

    int64_t hidden = static_cast<int>(x.size(1));
    at::Tensor tp_send_counts = at::empty({1}, at::dtype(at::kInt).device(device));
    int64_t tp_world_size = 1;
    int64_t tp_rankId = 0;
    int64_t moe_expert_number = send_head.size(0);
    int64_t global_bs = topk_idx_int32.size(0) * num_ranks;

    // Create combine_send_cost_stats_out tensor (optional output for performance monitoring)
    at::Tensor combine_send_cost_stats_out;

    // Combine data
    auto combined_x = torch::empty({expert_scales.size(0), hidden}, x.options());

    EXEC_NPU_CMD(aclnnMoeCombineNormal, x, token_src_info, ep_send_counts, expert_scales, tp_send_counts, group_ep_ptr,
                 num_ranks, rank, group_ep_ptr, tp_world_size, tp_rankId, moe_expert_number, global_bs, combined_x,
                 combine_send_cost_stats_out);

    return combined_x;
}

tensor_list MoeCombinePrefillBackwardImplNpu(const at::Tensor &self)
{
    return {at::Tensor()};
}

at::Tensor MoeCombinePrefillImpl(const at::Tensor &x, const at::Tensor &topk_idx, const at::Tensor &topk_weights,
                                 const at::Tensor &src_idx, const at::Tensor &send_head, c10::string_view groupEp,
                                 int64_t rank, int64_t num_ranks)
{
    static auto op = torch::Dispatcher::singleton()
                         .findSchemaOrThrow("umdk_cam_op_lib::moe_combine_prefill", "")
                         .typed<decltype(MoeCombinePrefillImpl)>();
    return op.call(x, topk_idx, topk_weights, src_idx, send_head, groupEp, rank, num_ranks);
}

// 通过继承torch::autograd::Function类实现前反向绑定
class ExtMoeCombinePrefill : public torch::autograd::Function<ExtMoeCombinePrefill> {
  public:
    static at::Tensor forward(AutogradContext *ctx, const at::Tensor &x, const at::Tensor &topk_idx,
                              const at::Tensor &topk_weights, const at::Tensor &src_idx, const at::Tensor &send_head,
                              c10::string_view groupEp, int64_t rank, int64_t num_ranks)
    {
        at::AutoDispatchBelowADInplaceOrView guard;
        auto result = MoeCombinePrefillImpl(x, topk_idx, topk_weights, src_idx, send_head, groupEp, rank, num_ranks);
        return result;
    }

    static tensor_list backward(AutogradContext *ctx, tensor_list grad_outputs)
    {
        return {at::Tensor()};
    }
};

at::Tensor MoeCombinePrefillImplAutograd(const at::Tensor &x, const at::Tensor &topkIdx, const at::Tensor &topkWeights,
                                         const at::Tensor &srcIdx, const at::Tensor &sendHead, c10::string_view groupEp,
                                         int64_t rank, int64_t numRanks)
{
    auto result = ExtMoeCombinePrefill::apply(x, topkIdx, topkWeights, srcIdx, sendHead, groupEp, rank, numRanks);
    return result;
}

// moe_combine_prefill
TORCH_LIBRARY_IMPL(umdk_cam_op_lib, PrivateUse1, m)
{
    m.impl("moe_combine_prefill", &MoeCombinePrefillImplNpu);
    m.impl("moe_combine_prefill_backward", &MoeCombinePrefillBackwardImplNpu);
}

TORCH_LIBRARY_IMPL(umdk_cam_op_lib, AutogradPrivateUse1, m)
{
    m.impl("moe_combine_prefill", &MoeCombinePrefillImplAutograd);
}