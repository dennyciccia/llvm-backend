#ifndef LLVM_TRANSFORMS_LOOPINVCODEMOT_H
#define LLVM_TRANSFORMS_LOOPINVCODEMOT_H
#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"

namespace llvm {
    
    class LoopInvCodeMot: public PassInfoMixin<LoopInvCodeMot> {
        public:
            PreservedAnalyses run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU);
    };

} // namespace llvm

#endif // LLVM_TRANSFORMS_LOOPINVCODEMOT _H
