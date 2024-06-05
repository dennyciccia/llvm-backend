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
        return true;
    }

    // gli operandi sono Value*, castati a istruzione diventano già la loro reaching definition
    if(auto *instOp = dyn_cast<Instruction>(op)){
        // controllo che la reaching definition sia fuori dal loop
        if(!L.contains(instOp->getParent())){
            return true;
        } else {
            // se è dentro al loop controllo che la definizione sia loop invariant
            if(instOp->getMetadata("isLoopInvariant")){
                return true;
            }
            return false;
        }
    }

    return true;
}

PreservedAnalyses LoopInvCodeMot::run(Loop &L, LoopAnalysisManager &LAM, LoopStandardAnalysisResults &LAR, LPMUpdater &LU) {
    // iterazione sui BB del loop
    for(auto iterBB = L.block_begin(); iterBB != L.block_end(); iterBB++){
        BasicBlock *BB = *iterBB;

        // iterazione sulle istruzioni del BB
        for(auto iterI = BB->begin(); iterI != BB->end(); iterI++){
            Instruction &I = *iterI;
            // se è un'istruzione binaria
            if(I.getOpcode() == Instruction::Add || I.getOpcode() == Instruction::Sub || I.getOpcode() == Instruction::Mul || I.getOpcode() == Instruction::SDiv){
                // se è loop invariant
                if(isLoopInvariantOperand(L, I.getOperand(0)) && isLoopInvariantOperand(L, I.getOperand(1))){
                    //MDNode agganciato al context dell'instruction
                    LLVMContext &instrContext = I.getContext();
                    MDString* metadato = MDString::get(instrContext, "isLoopInvariant");
                    MDNode *mdnode = MDNode::get(instrContext, metadato);
                    I.setMetadata("isLoopInvariant", mdnode);
                    //outs() << I << "\n";
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

        for (unsigned int i = 0; i < term->getNumSuccessors(); i++) {
            BasicBlock *succ = term->getSuccessor(i);
            if (!L.contains(succ)) {
            exits.push_back(succ);
            }
        }
    }

    std::vector<Instruction*> movable;

    // Iteriamo sulle istruzioni per vedere se sono candidate alla code motion
    for(auto iterBB = L.block_begin(); iterBB != L.block_end(); iterBB++){
        BasicBlock *BB = *iterBB;

        // iterazione sulle istruzioni del BB
        for(auto iterI = BB->begin(); iterI != BB->end(); iterI++){
            Instruction &I = *iterI;
            
            // Controlla che il BB dell'istruzione domina tutte le uscite
            bool domina_tutte_le_uscite = true;
            for(long unsigned int i=0; i<exits.size(); i++){
                if(!DT.dominates(I.getParent(), exits[i])){
                    domina_tutte_le_uscite = false;
                }
            }

            // controlla che la variabile definita dall'istruzione sia dead fuori dal loop (è dead se non viene mai usata fuori dal loop)
            bool istruzioni_dead = true;
            for(auto user = I.user_begin(); user != I.user_end(); ++user){
                if(Instruction *userInst = dyn_cast<Instruction>(*user)){
                    if(!L.contains(userInst->getParent())){
                        istruzioni_dead = false;
                    }
                }
            }

            // controlla che l'istruzione domina tutti i blocchi nel loop che usano la variabile a cui si sta assegnando un valore
            // praticamente scorri gli users e quando ne trovi uno che non è dominato metti false
            bool domina_users_nel_loop = true;
            for(auto user = I.user_begin(); user != I.user_end(); ++user){
                if(Instruction *userInst = dyn_cast<Instruction>(*user)){
                    if(!DT.dominates(I.getParent(), userInst->getParent())){
                        domina_users_nel_loop = false;
                    }
                }   
            }

            if(I.getMetadata("isLoopInvariant") && (domina_tutte_le_uscite || istruzioni_dead) && domina_users_nel_loop){
                //MDNode agganciato al context dell'instruction
                LLVMContext &instrContext = I.getContext();
                MDString* metadato = MDString::get(instrContext, "isMovable");
                MDNode* mdnode = MDNode::get(instrContext, metadato);
                I.setMetadata("isMovable", mdnode);
                Instruction *inst = dyn_cast<Instruction>(iterI);
                movable.push_back(inst);
            }
        }
    }

    // spostamento nel preheader
    BasicBlock* preheader = L.getLoopPreheader();
    for(auto iter = movable.begin(); iter != movable.end(); iter++){
        Instruction *inst = *iter;
        outs() << "Istruzione spostata: " << *inst << "\n";
        inst->moveBefore(preheader->getTerminator());
    }

    return PreservedAnalyses::all();
}