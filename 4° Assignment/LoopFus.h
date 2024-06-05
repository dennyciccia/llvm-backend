#ifndef LLVM_TRANSFORMS_LOOPFUS_H
#define LLVM_TRANSFORMS_LOOPFUS_H
#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/DependenceAnalysis.h"

namespace llvm {
    
    class LoopFus : public PassInfoMixin<LoopFus> {
        public:
            PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
    };

} // namespace llvm

#endif // LLVM_TRANSFORMS_LOOPFUS _H
