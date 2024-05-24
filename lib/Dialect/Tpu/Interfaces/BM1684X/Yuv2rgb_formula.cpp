//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Dialect/Tpu/Transforms/Codegen/Dynamic/DynamicLayer.hpp"

using namespace tpu_mlir::backend;

// =========================================
// GlobalGenInterface
// =========================================

void tpu::Yuv2rgbFormulaOp::codegen_global_bm1684x() {
  auto op = getOperation();
  auto input_spec = BM168x::get_input_spec(op);
  auto output_spec = BM168x::get_output_spec(op);

  yuv2rgb_formula_spec_t spec = {0};
  spec.width = static_cast<unsigned int>(getWidth());
  spec.height = static_cast<unsigned int>(getHeight());
  spec.src_format = static_cast<unsigned int>(getSrcFormat());
  spec.dst_format = static_cast<unsigned int>(getDstFormat());
  spec.output_data_format =
      static_cast<image_data_format_ext>(getImageFormat());
  spec.formula_mode = static_cast<formula_mode>(getFormulaMode());
  spec.round_mode = static_cast<ROUND_MODE_T>(getRoundMode());

  BM168x::call_global_func("backend_api_cv_yuv2rgb_formula_global", &spec,
                           sizeof(yuv2rgb_formula_spec_t), input_spec->data(),
                           output_spec->data());
}

// ======================================
// Dynamic GlobalGenInterface
// ======================================
int64_t tpu::Yuv2rgbFormulaOp::dyn_codegen_global_bm1684x(void *buffer) {
  llvm_unreachable("Not Implemented");
}

int64_t tpu::Yuv2rgbFormulaOp::get_fw_type_bm1684x() { return -1; }
