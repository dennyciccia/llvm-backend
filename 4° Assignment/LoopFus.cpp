#include "llvm/Transforms/Utils/LoopFus.h"

using namespace llvm;
using namespace std;

BasicBlock* nonLoopSuccessor(BranchInst* guard, Loop* L){
    for(unsigned int i=0; i<guard->getNumSuccessors(); i++){
        if(guard->getSuccessor(i) != L->getLoopPreheader())
            return guard->getSuccessor(i);
    }
    return nullptr;
}

bool areNext(Loop* L0, Loop* L1){
    bool adiacenti = false;
    
    if(L0->isGuarded() && L1->isGuarded()){
        //outs() << "guarded\n";
        if(nonLoopSuccessor(L0->getLoopGuardBranch(), L0) == L1->getLoopPreheader()){
            adiacenti = true;
        }
    } else if(!L0->isGuarded() && !L1->isGuarded()){
        //outs() << "unguarded\n";
        //outs() << L0->getExitBlock()->getName() << "\n";
        //outs() << L1->getLoopPreheader()->getName() << "\n";
        if(L0->getExitBlock() == L1->getLoopPreheader()){
            adiacenti = true;
        }
    }
    outs() << "Adiacenti: " << adiacenti << "\n";

    return adiacenti;
}

bool areTripCountsEqual(Loop* L0, Loop* L1, ScalarEvolution &SE){
    const SCEV* tripCountL0 = SE.getBackedgeTakenCount(L0);
    const SCEV* tripCountL1 = SE.getBackedgeTakenCount(L1);
    //outs() << *tripCountL0 << "\n";
    //outs() << *tripCountL1 << "\n";

    bool stessoNumIter = false;

    if(!isa<SCEVCouldNotCompute>(tripCountL0) && !isa<SCEVCouldNotCompute>(tripCountL1)){
        //outs() << "prova equal tripcount\n";
        stessoNumIter = tripCountL0 == tripCountL1;
    }

    outs() << "Stesso numero di iterazioni: " << stessoNumIter << "\n";

    return stessoNumIter;
}

bool areControlFlowEquivalent(Loop* L0, Loop* L1, DominatorTree &DT, PostDominatorTree& PDT){
    bool controlFlowEquivalent = false;

    //outs() << L0->getLoopPreheader()->getName() << "\n";
    //outs() << L1->getLoopPreheader()->getName() << "\n";
    
    if(DT.dominates(L0->getLoopPreheader(), L1->getLoopPreheader())){
        //outs() << "L0 domina L1\n";
        if(PDT.dominates(L1->getLoopPreheader(), L0->getLoopPreheader())){
            //outs() << "L1 postdomina L0\n";
            controlFlowEquivalent = true;
        }
    }

    outs() << "Control flow equivalent: " << controlFlowEquivalent << "\n";

    return controlFlowEquivalent;
}

bool nonNegativeDistance(Loop* L0, Loop* L1, DependenceInfo &DI, ScalarEvolution &SE){
    //outs() << "nonNegativeDistance()\n";
    bool nonNegDist = true;
    //itera sui blocchi del loop
    for(auto iterBB = L0->getBlocks().begin(); iterBB != L0->getBlocks().end(); ++iterBB){
        BasicBlock* BB = *iterBB;
        
        //itera sulle istruzioni del blocco del loop
        for(auto iterInst = BB->begin(); iterInst != BB->end(); ++iterInst){
            //Instruction& instr = *iterInst;
            //outs() << "Istruzione: " << instr << "\n";
            
            //quando trovi una store
            if(StoreInst *inst = dyn_cast<StoreInst>(iterInst)){
                //outs() << "store\n";
                //prendi l'istruzione che accede all'elemento dell'array
                auto arrayidx = inst->getPointerOperand();
                auto arrayidxInst = dyn_cast<GetElementPtrInst>(arrayidx);
                
                //prendi l'operando puntatore all'array e l'operando che indica l'offset
                auto ptrA = arrayidxInst->getPointerOperand();
                //outs() << "ptrA: " << *ptrA << "\n";
                auto idxprom = arrayidxInst->getOperand(1);
                //outs() << "idxprom: " << *idxprom << "\n";

                //calcola le ScalarEvolution dell'offset
                const SCEV *offsetBase = SE.getSCEV(idxprom);
                //outs() << "offsetBase: " << *offsetBase << "\n";

                //itera sugli users GEP dell'array nel loop L1
                for(auto user = ptrA->user_begin(); user != ptrA->user_end(); ++user){
                    if(auto userInst = dyn_cast<GetElementPtrInst>(*user)){
                        if(L1->contains(userInst->getParent())){
                            //prendi l'istruzione che accede all'array e l'offset che usa
                            //outs() << "user: " << *userInst << "\n";
                            auto idxpromUser = userInst->getOperand(1);
                            //outs() << "idxpromUser: " << *idxpromUser << "\n";

                            //calcola le ScalarEvolution dell'offset
                            const SCEV *offsetUser = SE.getSCEV(idxpromUser);
                            //outs() << "offsetUser: " << *offsetUser << "\n";

                            // Normalizza la SCEV per confrontarla nel contesto di L0
                            const SCEVAddRecExpr *offsetUserAR = dyn_cast<SCEVAddRecExpr>(offsetUser);
                            if (offsetUserAR->getLoop() == L1) {
                                // Trasforma offsetUserAR nel contesto di L0
                                const SCEV *offsetUserInL0 = SE.getAddRecExpr(offsetUserAR->getStart(), offsetUserAR->getStepRecurrence(SE), L0, offsetUserAR->getNoWrapFlags());
                                //outs() << "offsetUserInL0: " << *offsetUserInL0 << "\n";
                                //controlla che l'offset dell'istruzione sia maggiore o uguale dell'offset dell'user
                                if(SE.isKnownPredicate(ICmpInst::ICMP_SGE, offsetBase, offsetUserInL0)){
                                    //outs() << "result: GE\n";
                                    nonNegDist = true;
                                }else{
                                    nonNegDist = false;
                                    //outs() << "result: not GE\n";
                                }
                            }else{
                                nonNegDist = false;
                            }

                            /*
                            auto dep = DI.depends(inst, userInst, true);
                            outs() << "dep: \n";
                            
                            if(dep)
                                for(unsigned int i=1; i <= dep->getLevels(); i++){
                                    auto dir = dep->getDirection(i);
                                    outs() << "direction " << i << ": " << dir << "\n";
                                    bool condiz = dir == Dependence::DVEntry::LT;
                                    outs() << "dir == Dependence::DVEntry::LT -> " << condiz << "\n";
                                }
                            */
                        }
                    }
                }
            }
        }
    }
    outs() << "Dipendenze con distanze non negative: " << nonNegDist << "\n";
    return nonNegDist;
}

BasicBlock* getEndBody(Loop* L){
    BasicBlock *latch = L->getLoopLatch();
    for(auto iter = L->getBlocks().begin(); iter != L->getBlocks().end(); ++iter){
        BasicBlock *BB = *iter;
        if(BB->getTerminator()->getSuccessor(0) == latch){
            return BB;
        }
    }
    return nullptr;
}

bool fuseLoops(Loop* L0, Loop* L1){
    //modificare gli usi della induction variable di L1 con l'induction variable di L0
    PHINode *indVarL0 = L0->getCanonicalInductionVariable();
    PHINode *indVarL1 = L1->getCanonicalInductionVariable();
    indVarL1->replaceAllUsesWith(indVarL0);

    //Modifica del CFG

    //l'uscita dell'header di L0 è l'exitblock di L1
    Instruction *terminatorheaderL0 = L0->getHeader()->getTerminator();
    BasicBlock *exitL1 = L1->getExitBlock();
    terminatorheaderL0->setSuccessor(1, exitL1);

    //il successore del body di L0 è il body di L1
    BasicBlock *endBodyL0 = getEndBody(L0);
    if(!endBodyL0) return false;
    BasicBlock *bodyL1 = L1->getHeader()->getTerminator()->getSuccessor(0);
    endBodyL0->getTerminator()->setSuccessor(0, bodyL1);
    //endBodyL0->getTerminator()->setSuccessor(0, bodyL1->getTerminator()->getSuccessor(0));

    //il successore del body di L1 è il latch di L0
    BasicBlock *endBodyL1 = getEndBody(L1);
    if(!endBodyL1) return false;
    BasicBlock *latchL0 = L0->getLoopLatch();
    endBodyL1->getTerminator()->setSuccessor(0, latchL0);

    //il successore nel loop dell'header di L1 è il latch di L1
    Instruction *terminatorheaderL1 = L1->getHeader()->getTerminator();
    BasicBlock *latchL1 = L1->getLoopLatch();
    terminatorheaderL1->setSuccessor(0, latchL1);

    return true;
}

vector<Loop*> getLoopsInLevel(LoopInfo &LI, unsigned int livello){
    vector<Loop*> loops;
    for(auto iterL = LI.getLoopsInPreorder().begin(); iterL != LI.getLoopsInPreorder().end(); ++iterL){
        Loop *L = *iterL;
        if(L->getLoopDepth() == livello){
            loops.push_back(L);
        }
    }
    return loops;
}

PreservedAnalyses LoopFus::run(Function &F, FunctionAnalysisManager &AM){
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    bool uniti;
    bool finitiLivelli = false;
    unsigned int livello = 1;

    //itera sui livelli
    while(!finitiLivelli){
        //prendi i loop nel livello corrente
        vector<Loop*> loopsInLevel = getLoopsInLevel(LI, livello);
        //se non ce ne sono esci
        if(loopsInLevel.empty())
            finitiLivelli = true;
        
        uniti = true;
            
        //itera finchè non viene unito nessun loop
        while(uniti){
            uniti = false;

            //scorri i loop nel livello corrente ed eventualmente unisci
            for (auto iterL = loopsInLevel.begin(); iterL != loopsInLevel.end(); ++iterL){
                //se sei all'ultimo loop che non ha niente dopo esci dal ciclo perchè è finito
                if(iterL == loopsInLevel.end()-1) break;

                Loop* L0 = *iterL;
                Loop* L1 = *(iterL+1);

                outs() << "Livello: " << livello << "\n";
                outs() << "L0: " << *L0 << "\n";
                outs() << "L1: " << *L1 << "\n";

                // condizione 1: loop adiacenti
                bool adiacenti = areNext(L0, L1);

                // condizione 2: stesso numero di iterazioni
                ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
                bool equalTripCount = areTripCountsEqual(L0, L1, SE);

                // condizione 3: loop control flow equivalent
                DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
                PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
                bool CFEquivalent = areControlFlowEquivalent(L0, L1, DT, PDT);

                // condizione 4: non ci possono essere distanze negative nelle dipendenze tra i due loop
                DependenceInfo &DI = AM.getResult<DependenceAnalysis>(F);
                bool nonNegativeDist = nonNegativeDistance(L0, L1, DI, SE);

                // trasformazione del codice
                if(adiacenti && equalTripCount && CFEquivalent && nonNegativeDist){
                    if(fuseLoops(L0, L1)){
                        outs() << "loop fusi\n\n";
                        uniti = true;
                        loopsInLevel.erase(iterL+1);
                        break;
                    } else {
                        outs() << "loop non fusi\n\n";
                    }
                } else {
                    outs() << "non ci sono le condizioni per la fusione\n\n";
                }
            }
        }
        livello++;
    }
    
    return PreservedAnalyses::all();
}