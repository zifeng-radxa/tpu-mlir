//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu-mlir/Transforms/StructuredTransform.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/IR/AffineExprVisitor.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/STLExtras.h"

namespace tpu_mlir {
using namespace mlir;

bool operator==(const ComputePattern &lhs, const ComputePattern &rhs) {
  if (lhs.indexingMaps.size() != rhs.indexingMaps.size())
    return false;
  for (auto [a, b] : llvm::zip(lhs.indexingMaps, rhs.indexingMaps)) {
    if (a != b)
      return false;
  }
  for (auto [a, b] : llvm::zip(lhs.iteratorTypes, rhs.iteratorTypes)) {
    if (a != b)
      return false;
  }
  return true;
}

bool operator!=(const ComputePattern &lhs, const ComputePattern &rhs) {
  return !(lhs == rhs);
}

std::optional<ComputePattern> Unroll::run(const ComputePattern &source) {
  auto c0 = getAffineConstantExpr(0, source.indexingMaps[0].getContext());
  SmallVector<AffineMap> outMaps;
  for (auto map : source.indexingMaps) {
    llvm::SmallBitVector bitMask(map.getNumDims());
    bitMask.set(dimention.getPosition());
    auto processed =
        map.replace(dimention, c0, map.getNumDims(), map.getNumSymbols());
    processed = projectDims(processed, bitMask, true);
    SmallVector<int64_t, 8> constMask;
    for (auto [index, expr] : llvm::enumerate(processed.getResults())) {
      if (expr == c0)
        constMask.push_back(index);
    }
    if (constMask.size() == processed.getNumResults())
      return std::nullopt;
    outMaps.push_back(processed.dropResults(constMask));
  }
  SmallVector outIterator(source.iteratorTypes);
  outIterator.erase(outIterator.begin() + dimention.getPosition());
  return ComputePattern{outMaps, outIterator};
}

std::optional<ComputePattern> DropSymbol::run(const ComputePattern &source) {
  auto c0 = getAffineConstantExpr(0, source.indexingMaps[0].getContext());
  auto c1 = getAffineConstantExpr(1, source.indexingMaps[0].getContext());
  SmallVector<AffineMap> outMaps;
  for (auto map : source.indexingMaps) {
    llvm::SmallBitVector bitMask(map.getNumSymbols());
    bitMask.set(symbol.getPosition());
    auto processed =
        map.replace(symbol, c1, map.getNumDims(), map.getNumSymbols());
    processed = projectSymbols(processed, bitMask, true);
    // processed = compressUnusedSymbols(processed);
    SmallVector<int64_t, 8> constMask;
    for (auto [index, expr] : llvm::enumerate(processed.getResults())) {
      if (expr == c0)
        constMask.push_back(index);
    }
    if (constMask.size() == processed.getNumResults())
      return std::nullopt;
    outMaps.push_back(processed.dropResults(constMask));
  }
  return ComputePattern{outMaps, source.iteratorTypes};
}

std::optional<ComputePattern> MergeDims::run(const ComputePattern &source) {
  auto &iteratorTypes = source.iteratorTypes;
  if (iteratorTypes[dim1.getPosition()] != iteratorTypes[dim2.getPosition()])
    return std::nullopt;
  for (auto map : source.indexingMaps) {
    // check if binary expression contain dim1 or dim2
    if (auto first = map.getResultPosition(dim1)) {
      if (auto next = map.getResultPosition(dim2)) {
        // should have both dim1 and dim2
        if (first.value() + 1 == next.value())
          continue;          // success
        return std::nullopt; // position error
      } else {
        return std::nullopt; // dim missing error
      }
    } else if (auto next = map.getResultPosition(dim2)) {
      // only have one of dim1 and dim2.
      return std::nullopt; // dim missing error
    }
    for (auto dim : map.getResults()) {
      if (dim.isFunctionOfDim(dim1.getPosition()) ||
          dim.isFunctionOfDim(dim2.getPosition()))
        return std::nullopt; // dim in function error.
    }
    // success, does not contain dim1 and dim2.
  }
  auto dropDim = Unroll(cast<AffineDimExpr>(dim2));
  return dropDim.run(source);
}

std::optional<ComputePattern> Permutation::run(const ComputePattern &source) {
  if (llvm::all_of(runOnItems, [](bool i) { return i == false; }))
    return std::nullopt;
  // check project permutation
  SmallVector<AffineMap> outMaps;
  auto context = source.indexingMaps[0].getContext();
  for (auto [map, action] : llvm::zip(source.indexingMaps, runOnItems)) {
    if (action) {
      auto result = SmallVector<AffineExpr>(map.getResults());
      if (permuteDims[0] >= result.size() || permuteDims[1] >= result.size())
        return std::nullopt;
      std::iter_swap(result.begin() + permuteDims[0],
                     result.begin() + permuteDims[1]);
      outMaps.push_back(AffineMap::get(map.getNumDims(), map.getNumSymbols(),
                                       result, context));
    } else
      outMaps.push_back(map);
  }
  auto out = ComputePattern{outMaps, source.iteratorTypes};
  return out;
}

std::optional<ComputePattern> ExpandDims::run(const ComputePattern &source) {
  if (llvm::all_of(runOnItems, [](bool i) { return i == false; }))
    return std::nullopt;

  SmallVector<AffineMap> outMaps;
  auto context = source.indexingMaps[0].getContext();
  auto d0 = getAffineDimExpr(0, context);
  auto dx = getAffineDimExpr(source.indexingMaps[0].getNumDims(), context);
  SmallVector outIterator(source.iteratorTypes);
  if (iteratorType == utils::IteratorType::parallel) {
    outIterator.insert(outIterator.begin(), utils::IteratorType::parallel);
  } else {
    outIterator.push_back(utils::IteratorType::reduction);
  }

  for (auto [map, action] : llvm::zip(source.indexingMaps, runOnItems)) {
    if (iteratorType == utils::IteratorType::parallel) {
      auto processed = map.shiftDims(1);
      if (action) {
        outMaps.push_back(processed.insertResult(d0, 0));
      } else
        outMaps.push_back(processed);
    } else { // append reduction to the end.
      auto processed = AffineMap::get(map.getNumDims() + 1, map.getNumSymbols(),
                                      map.getResults(), context);
      if (action) {
        outMaps.push_back(
            processed.insertResult(dx, processed.getNumResults()));
      } else
        outMaps.push_back(processed);
    }
  }
  return ComputePattern{outMaps, outIterator};
}

static void getSummandExprs(AffineExpr expr, SmallVector<AffineExpr> &result) {
  auto addExpr = dyn_cast<AffineBinaryOpExpr>(expr);
  if (!addExpr || addExpr.getKind() != AffineExprKind::Add) {
    result.push_back(expr);
    return;
  }
  getSummandExprs(addExpr.getLHS(), result);
  getSummandExprs(addExpr.getRHS(), result);
}

std::optional<ComputePattern> DecomposeExpr::run(const ComputePattern &source) {
  SmallVector<AffineMap> outMaps;
  bool changed = false;
  auto context = source.indexingMaps[0].getContext();
  for (auto map : source.indexingMaps) {
    if (position >= map.getNumResults()) {
      outMaps.push_back(map);
      continue;
    }
    if (auto addExpr = dyn_cast<AffineBinaryOpExpr>(map.getResult(position))) {
      SmallVector<AffineExpr> result;
      getSummandExprs(addExpr, result);
      auto exprs = llvm::to_vector<4>(map.getResults());
      exprs.erase(exprs.begin() + position);
      exprs.insert(exprs.begin() + position, result.begin(), result.end());
      outMaps.push_back(AffineMap::get(map.getNumDims(), map.getNumSymbols(),
                                       exprs, context));
      changed = true;
      continue;
    }
    outMaps.push_back(map);
  }
  if (!changed)
    return std::nullopt;

  return ComputePattern{outMaps, source.iteratorTypes};
}

SmallVector<bool> itov(uint64_t num, int width) {
  SmallVector<bool> boolVector;
  for (int i = width - 1; i >= 0; --i) {
    boolVector.push_back((num & (1 << i)) != 0);
  }
  return boolVector;
}

void Solver::driver(const ComputePattern &source, Transforms &transforms,
                    int64_t depth) {
  if (depth >= maxDepth) {
    return transforms.erase();
  }
  auto const &iterSpace = source.indexingMaps[0];
  auto const sourceDimSize = iterSpace.getNumDims();
  auto const symbSize = iterSpace.getNumSymbols();
  auto const targetDimSize = target.indexingMaps[0].getNumDims();

  // try different configuration
  if (sourceDimSize > targetDimSize) {
    // try Unroll
    for (int i = 0, n = sourceDimSize; i < n; i++) {
      auto dim = cast<AffineDimExpr>(getAffineDimExpr(i, context));
      auto transform = Unroll(dim);
      driver(transform, source, transforms, depth);
    }
    // try mergeDim
    for (int i = 0, n = sourceDimSize; i < n; i++) {
      for (int j = i + 1; j < n; j++) {
        auto dim1 = cast<AffineDimExpr>(getAffineDimExpr(i, context));
        auto dim2 = cast<AffineDimExpr>(getAffineDimExpr(j, context));
        auto transform = MergeDims(dim1, dim2);
        driver(transform, source, transforms, depth);
      }
    }
  }

  for (int i = 0, n = symbSize; i < n; i++) {
    auto symb = cast<AffineSymbolExpr>(getAffineSymbolExpr(i, context));
    auto transform = DropSymbol(symb);
    driver(transform, source, transforms, depth);
  }

  auto mapSize = source.indexingMaps.size();
  for (int i = 0, n = sourceDimSize; i < n; i++) {
    for (int j = i + 1; j < n; j++) {
      for (int k = (1 << mapSize) - 1; k > 0; k--) {
        auto mask = itov(k, mapSize);
        auto transform = Permutation({i, j}, mask);
        driver(transform, source, transforms, depth);
      }
    }
  }

  if (targetDimSize > sourceDimSize) {
    for (auto iType :
         {utils::IteratorType::parallel, utils::IteratorType::reduction}) {
      for (int k = (1 << mapSize) - 1; k > 0; k--) {
        auto mask = itov(k, mapSize);
        auto transform = ExpandDims(iType, mask);
        driver(transform, source, transforms, depth);
      }
    }
  }

  for (auto [index, expr] :
       llvm::enumerate(source.indexingMaps[0].getResults())) {
    if (expr.getKind() == AffineExprKind::Add) {
      auto transform = DecomposeExpr(index);
      driver(transform, source, transforms, depth);
    }
  }
  // clean up failed branch.
  if (transforms.getChildren().empty())
    transforms.erase();
}

Transforms Solver::solve(const ComputePattern source) {
  if (source == target) {
    return Transforms();
  }

  auto start = Transforms();
  driver(source, start);
  return std::move(start);
}

} // namespace tpu_mlir
