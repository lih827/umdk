/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: add get_dispatch_layout pybind extention file
 * Create: 2026-01-06
 * Note:
 * History: 2026-01-06 create get_dispatch_layout pybind extention file
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
using TensorVector = std::vector<at::Tensor>;
using namespace at;
using namespace std;

const int LOCAL_RANK_SIZE = 8;
const int MAX_BATCH_SIZE = 4096;
const int EXPERT_DATA_SIZE = 1 + MAX_BATCH_SIZE; // 4097

std::tuple<at::Tensor, at::Tensor> GetDispatchLayoutImplNpu(const at::Tensor &topkIdx, int64_t numExperts,
                                                            int64_t numRanks)
{
    // Convert topk_idx to int64 if necessary
    at::Tensor topk_idx_int64 = topkIdx.scalar_type() == at::kLong ? topkIdx : topkIdx.to(at::kLong);

    TORCH_BIND_ASSERT(topk_idx_int64.dim() == 2);
    TORCH_BIND_ASSERT(topk_idx_int64.is_contiguous());
    TORCH_BIND_ASSERT(numExperts > 0);

    const int num_tokens = topk_idx_int64.size(0);
    const int num_topk = topk_idx_int64.size(1);
    const int local_ranksize = LOCAL_RANK_SIZE;
    auto server_num = numRanks / local_ranksize;

    auto device = topk_idx_int64.device();
    auto num_tokens_per_expert = at::zeros({numExperts}, at::dtype(at::kInt).device(device));
    auto num_tokens_per_rank = at::zeros({numRanks}, at::dtype(at::kInt).device(device));
    auto is_token_in_rank = at::zeros({num_tokens, numRanks}, at::dtype(at::kInt).device(device));
    const int notify_send_data_size =
        numExperts * EXPERT_DATA_SIZE + server_num + MAX_BATCH_SIZE * (1 + 2 * server_num + numExperts);
    auto send_token_idx_small = at::zeros({num_tokens, num_topk}, at::dtype(at::kInt).device(device));
    auto notify_send_data = at::zeros({notify_send_data_size}, at::dtype(at::kInt).device(device));
    EXEC_NPU_CMD(aclnnDispatchLayout, topk_idx_int64, num_tokens, numRanks, numExperts, num_topk, local_ranksize,
                 num_tokens_per_rank, num_tokens_per_expert, is_token_in_rank, notify_send_data, send_token_idx_small);

    return std::make_tuple(num_tokens_per_expert, send_token_idx_small);
}

TensorVector GetDispatchLayoutBackwardImplNpu(const at::Tensor &self)
{
    at::Tensor result = at::Tensor(self); // 创建输出内存
    return {result, result};
}

std::tuple<at::Tensor, at::Tensor> GetDispatchLayoutImpl(const at::Tensor &topkIdx, int64_t numExperts,
                                                         int64_t numRanks)
{
    static auto op = torch::Dispatcher::singleton()
                         .findSchemaOrThrow("umdk_cam_op_lib::get_dispatch_layout", "")
                         .typed<decltype(GetDispatchLayoutImpl)>();
    return op.call(topkIdx, numExperts, numRanks);
}

// 通过继承torch::autograd::Function类实现前反向绑定
class ExtGetDispatchLayout : public torch::autograd::Function<ExtGetDispatchLayout> {
  public:
    static TensorVector forward(AutogradContext *ctx, const at::Tensor &topkIdx, int64_t numExperts, int64_t numRanks)
    {
        auto result = GetDispatchLayoutImpl(topkIdx, numExperts, numRanks);

        return {std::get<0>(result), std::get<1>(result)};
    }

    static TensorVector backward(AutogradContext *ctx, TensorVector grad_outputs)
    {
        return {at::Tensor(), at::Tensor()};
    }
};

std::tuple<at::Tensor, at::Tensor> GetDispatchLayoutImplAutograd(const at::Tensor &topkIdx, int64_t numExperts,
                                                                 int64_t numRanks)
{
    auto result = ExtGetDispatchLayout::apply(topkIdx, numExperts, numRanks);
    return std::make_tuple(result[0], result[1]);
}

// get_dispatch_layout
TORCH_LIBRARY_IMPL(umdk_cam_op_lib, PrivateUse1, m)
{
    m.impl("get_dispatch_layout", &GetDispatchLayoutImplNpu);
    m.impl("get_dispatch_layout_backward", &GetDispatchLayoutBackwardImplNpu);
}

TORCH_LIBRARY_IMPL(umdk_cam_op_lib, AutogradPrivateUse1, m)
{
    m.impl("get_dispatch_layout", &GetDispatchLayoutImplAutograd);
}