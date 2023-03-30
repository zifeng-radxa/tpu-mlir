//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Conversion/TopToTpu/LoweringBM1684X.h"
#include "tpu_mlir/Conversion/TopToTpu/TopLowering.h"
#include "tpu_mlir/Dialect/Tpu/IR/TpuOps.h"

namespace tpu_mlir {
namespace bm1684x {

void LoweringArg(PatternRewriter &rewriter, top::ArgOp op) {
  std::vector<Value> operands;
  operands.push_back(op.getInput());
  std::vector<NamedAttribute> attrs;
  for (auto &attr : op->getAttrs()) {
    attrs.push_back(attr);
  }
  std::vector<Type> new_types;
  if (!module::isNone(op.getIndices())) {
    auto shape = module::getShape(op.getIndices());
    auto new_type = RankedTensorType::get(shape, rewriter.getI32Type());
    new_types.push_back(new_type);
  } else {
    new_types.push_back(op.getIndices().getType());
  }
  new_types.push_back(op.getValues().getType());

  rewriter.replaceOpWithNewOp<tpu::ArgOp>(op, new_types, operands, attrs);
  return;
}

void ArgLowering::LoweringF32(PatternRewriter &rewriter, top::ArgOp op) const {
  LoweringArg(rewriter, op);
}
void ArgLowering::LoweringINT4(PatternRewriter &rewriter, top::ArgOp op,
                               bool asymmetric) const {
  LoweringArg(rewriter, op);
}
void ArgLowering::LoweringINT8(PatternRewriter &rewriter, top::ArgOp op,
                               bool asymmetric) const {
  LoweringArg(rewriter, op);
}

void ArgLowering::LoweringBF16(PatternRewriter &rewriter, top::ArgOp op) const {
  LoweringArg(rewriter, op);
}

void ArgLowering::LoweringF16(PatternRewriter &rewriter, top::ArgOp op) const {
  LoweringArg(rewriter, op);
}

void ArgLowering::LoweringQuantized(PatternRewriter &rewriter,
                                    top::ArgOp op) const {
  llvm_unreachable("Not Implemented");
}

} // namespace bm1684x
} // namespace tpu_mlir
