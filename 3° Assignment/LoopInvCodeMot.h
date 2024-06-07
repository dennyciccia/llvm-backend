#ifndef LLVM_TRANSFORMS_LOOPINVCODEMOT_H
#define LLVM_TRANSFORMS_LOOPINVCODEMOT_H
#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include <llvm/IR/Constants.h>
#include <vector>

namespace llvm {
    
    class LoopInvCodeMot: public PassInfoMixin<LoopInvCodeMot> {
        public:
            PreservedAnalyses run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU);
    };

} // namespace llvm

#endif // LLVM_TRANSFORMS_LOOPINVCODEMOT _H
