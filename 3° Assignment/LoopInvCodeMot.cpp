#include "llvm/Transforms/Utils/LoopInvCodeMot.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include <llvm/IR/Constants.h>
#include <vector>

using namespace llvm;

bool isLoopInvariantOperand(Loop& L, Value* op){
    // controllo che l'operando è costante
    if(dyn_cast<Constant>(op)){
        outs() << "funz 1" << "\n";
        return true;
    }

    // gli operandi sono Value*, castati a istruzione diventano già la loro reaching definition
    if(auto *instOp = dyn_cast<Instruction>(op)){
        outs() << "funz 4" << "\n";
        // controllo che la reaching definition sia fuori dal loop
        if(!L.contains(instOp->getParent())){
            outs() << "funz 2" << "\n";
            return true;
        } else {
            // controllo che la definizione sia loop invariant
            if(instOp->getMetadata("isLoopInvariant")){
                outs() << "funz 3" << "\n";
                return true;
            }
            return false;
        }
    }

    return true;
}

PreservedAnalyses LoopInvCodeMot::run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU) {
    outs() << "Entrato1" << "\n";
    // iterazione sulle sui BB del loop
    for(auto iterBB = L.block_begin(); iterBB != L.block_end(); iterBB++){
        BasicBlock *BB = *iterBB;

        // iterazione sulle istruzioni del BB
        for(auto iterI = BB->begin(); iterI != BB->end(); iterI++){
            Instruction &I = *iterI;
            // se è un'istruzione binaria
            if(I.getOpcode() == Instruction::Add || I.getOpcode() == Instruction::Sub || I.getOpcode() == Instruction::Mul || I.getOpcode() == Instruction::SDiv){
                outs() << "entrato2" << "\n";
                // se è loop invariant
                if(isLoopInvariantOperand(L, I.getOperand(0)) && isLoopInvariantOperand(L, I.getOperand(1))){
                    //MDNode agganciato al context dell'instruction
                    LLVMContext &instrContext = I.getContext();
                    MDString* metadato = MDString::get(instrContext, "isLoopInvariant");
                    MDNode *mdnode = MDNode::get(instrContext, metadato);
                    I.setMetadata("isLoopInvariant", mdnode);
                    outs() << I << "\n";
                }
            }
        }
    }

    // Dominance Tree
    DominatorTree &DT = LAR.DT;

    // Trovare le uscite del loop
    std::vector<BasicBlock *> exits;
    for (auto iterBB = L.block_begin(); iterBB != L.block_end(); iterBB++) {
        BasicBlock *BB = *iterBB;
        auto term = BB->getTerminator();

        for (auto i = 0; i < term->getNumSuccessors(); i++) {
            BasicBlock *succ = term->getSuccessor(i);
            if (!L.contains(succ)) {
            exits.push_back(succ);
            }
        }
    }

    // Iteriamo sulle istruzioni per vedere se sono candidate alla code motion
    for(auto iterBB = L.block_begin(); iterBB != L.block_end(); iterBB++){
        BasicBlock *BB = *iterBB;

        // iterazione sulle istruzioni del BB
        for(auto iterI = BB->begin(); iterI != BB->end(); iterI++){
            Instruction &I = *iterI;
            
            // Controlla che il BB dell'istruzione domina tutte le uscite
            bool domina_tutte_le_uscite = true;
            for(int i=0; i<exits.size(); i++){
                if(!DT.dominates(I.getParent(), exits[i])){
                    domina_tutte_le_uscite = false;
                }
            }

            // controlla che la variabile definita dall'istruzione sia dead fuori dal loop (è dead se non viene mai usata fuori dal loop)
            bool istruzioni_dead = true;
            if(I.getNumUses() > 0){
                for(auto use = I.use_begin(); use != I.use_end(); use++){
                    decltype(auto) useInst = dyn_cast<Instruction>(use);
                    if(!L.contains(useInst.getParent())){
                        istruzioni_dead = false;
                    }
                }
            }

            // controlla che l'istruzione domina tutti i blocchi nel loop che usano la variabile a cui si sta assegnando un valore
            // praticamente scorri gli users e quando ne trovi uno che non è dominato metti false
            bool domina_uses_nel_loop = true;
            for(auto use = I.use_begin(); use != I.use_end(); use++){
                decltype(auto) useInst = dyn_cast<Instruction>(use);
                if(!DT.dominates(I.getParent(), useInst.getParent())){
                    domina_uses_nel_loop = false;
                } 
            }

            if(I.getMetadata("isLoopInvariant") && (domina_tutte_le_uscite || istruzioni_dead) && domina_uses_nel_loop){
                //MDNode agganciato al context dell'instruction
                LLVMContext &instrContext = I.getContext();
                MDString* metadato = MDString::get(instrContext, "isMovable");
                MDNode* mdnode = MDNode::get(instrContext, metadato);
                I.setMetadata("isMovable", mdnode);
            }
        }
    }

    // spostamento nel preheader
    BasicBlock* preheader = L.getLoopPreheader();
    for(auto iterBB = L.block_begin(); iterBB != L.block_end(); iterBB++){
        BasicBlock *BB = *iterBB;

        // iterazione sulle istruzioni del BB
        for(auto iterI = BB->begin(); iterI != BB->end(); iterI++){
            Instruction &I = *iterI;
            I.moveBefore(preheader->getTerminator());
        }
    }

    return PreservedAnalyses::all();
}