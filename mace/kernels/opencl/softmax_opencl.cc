//
// Copyright (c) 2017 XiaoMi All rights reserved.
//

#include "mace/kernels/softmax.h"
#include "mace/core/runtime/opencl/cl2_header.h"
#include "mace/core/runtime/opencl/opencl_runtime.h"
#include "mace/kernels/opencl/helper.h"
#include "mace/utils/utils.h"
#include "mace/utils/tuner.h"

namespace mace {
namespace kernels {

template<typename T>
void SoftmaxFunctor<DeviceType::OPENCL, T>::operator()(const Tensor *logits,
                                                       Tensor *output,
                                                       StatsFuture *future) {
  const index_t batch = logits->dim(0);
  const index_t height = logits->dim(1);
  const index_t width = logits->dim(2);
  const index_t channels = logits->dim(3);
  const index_t channel_blocks = RoundUpDiv4(channels);
  const int remain_channels = channel_blocks * 4 - channels;

  auto runtime = OpenCLRuntime::Global();

  std::set<std::string> built_options;
  std::string kernel_name = MACE_OBFUSCATE_SYMBOL("softmax");
  built_options.emplace("-Dsoftmax=" + kernel_name);
  auto dt = DataTypeToEnum<T>::value;
  built_options.emplace("-DDATA_TYPE=" + DtToUpstreamCLDt(dt));
  built_options.emplace("-DCMD_DATA_TYPE=" + DtToUpstreamCLCMDDt(dt));
  cl::Kernel softmax_kernel = runtime->BuildKernel("softmax", kernel_name, built_options);

  uint32_t idx = 0;
  softmax_kernel.setArg(idx++, *(static_cast<const cl::Image2D *>(logits->buffer())));
  softmax_kernel.setArg(idx++, static_cast<int>(channels));
  softmax_kernel.setArg(idx++, remain_channels);
  softmax_kernel.setArg(idx++, *(static_cast<cl::Image2D *>(output->buffer())));
  const uint32_t gws[3] = {static_cast<uint32_t>(channel_blocks),
                           static_cast<uint32_t>(width),
                           static_cast<uint32_t>(height * batch)};
  const std::vector<uint32_t> lws = {8, 16, 8, 1};
  std::stringstream ss;
  ss << "softmax_opencl_kernel_"
     << output->dim(0) << "_"
     << output->dim(1) << "_"
     << output->dim(2) << "_"
     << output->dim(3);
  TuningOrRun3DKernel(softmax_kernel, ss.str(), gws, lws, future);
}

template
struct SoftmaxFunctor<DeviceType::OPENCL, float>;
template
struct SoftmaxFunctor<DeviceType::OPENCL, half>;
}  // namespace kernels
}  // namespace mace