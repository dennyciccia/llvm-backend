//===-- LocalOpts.cpp - Example Transformations --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/LocalOpts.h"

using namespace llvm;

bool algebraicIdentity(Instruction &inst) {
  bool transformed = false;
  if (inst.getOpcode() == Instruction::Add) {
    // Algebric identity: x + 0 = x
    for (int op = 0; op < 2; op++) {
      if (ConstantInt *c = dyn_cast<ConstantInt>(inst.getOperand(op))) {
        if (c->getValue().isZero()) {
          inst.replaceAllUsesWith(inst.getOperand(op == 1 ? 0 : 1));
          outs() << "Sostituzione di " << inst << " con "
                 << *inst.getOperand(op == 1 ? 0 : 1) << '\n';
          transformed = true;
        }
      }
    }
  } else if (inst.getOpcode() == Instruction::Mul) {
    // Algebric identity: x * 1 = x
    for (int op = 0; op < 2; op++) {
      if (ConstantInt *c = dyn_cast<ConstantInt>(inst.getOperand(op))) {
        if (c->getValue().isOne()) {
          inst.replaceAllUsesWith(inst.getOperand(op == 1 ? 0 : 1));
          outs() << "Sostituzione di " << inst << " con "
                 << *inst.getOperand(op == 1 ? 0 : 1) << '\n';
          transformed = true;
        }
      }
    }
  }
  return transformed;
}

bool strengthReduction(Instruction &inst) {
  bool transformed = false;
  if (inst.getOpcode() == Instruction::Mul) {
    // Moltiplicazione per potenza di 2 e per potenza di 2 + 1 o - 1
    for (int op = 0; op < 2; op++) {
      ConstantInt *c = dyn_cast<ConstantInt>(inst.getOperand(op));
      if (c) {
        int value = c->getSExtValue();
        if ((value & (value - 1)) == 0) {
          // Potenza di due
          Instruction *shl = BinaryOperator::Create(
              Instruction::Shl, inst.getOperand(op == 1 ? 0 : 1),
              ConstantInt::get(c->getType(), c->getValue().logBase2()));
          shl->insertAfter(&inst);
          inst.replaceAllUsesWith(shl);
          outs() << "Sostituzione di " << inst << " con " << *shl << '\n';
          transformed = true;
        } else if (((value - 1) & (value - 1 - 1)) == 0) {
          // Potenza di due + 1
          Instruction *shl = BinaryOperator::Create(
              Instruction::Shl, inst.getOperand(op == 1 ? 0 : 1),
              ConstantInt::get(c->getType(), APInt(32, value - 1).logBase2()));
          Instruction *add = BinaryOperator::Create(
              Instruction::Add, shl, inst.getOperand(op == 1 ? 0 : 1));
          shl->insertAfter(&inst);
          add->insertAfter(shl);
          inst.replaceAllUsesWith(add);
          outs() << "Sostituzione di " << inst << " con " << *shl << " e con "
                 << *add << '\n';
          transformed = true;
        } else if (((value + 1) & (value - 1 + 1)) == 0) {
          // Potenza di due - 1
          Instruction *shl = BinaryOperator::Create(
              Instruction::Shl, inst.getOperand(op == 1 ? 0 : 1),
              ConstantInt::get(c->getType(), APInt(32, value + 1).logBase2()));
          Instruction *sub = BinaryOperator::Create(
              Instruction::Sub, shl, inst.getOperand(op == 1 ? 0 : 1));
          shl->insertAfter(&inst);
          sub->insertAfter(shl);
          inst.replaceAllUsesWith(sub);
          outs() << "Sostituzione di " << inst << " con " << *shl << " e con "
                 << *sub << '\n';
          transformed = true;
        }
      }
    }
  } else if (inst.getOpcode() == Instruction::SDiv) {
    // Divisione per potenza di 2
    if (ConstantInt *c = dyn_cast<ConstantInt>(inst.getOperand(1))) {
      if (c->getValue().isPowerOf2()) {
        // crea un instruzione shl che fa lo shift del logaritmo del valore
        // dell'operando costante
        Instruction *lshr = BinaryOperator::Create(
            Instruction::LShr, inst.getOperand(0),
            ConstantInt::get(c->getType(), c->getValue().logBase2()));
        // inserimento dell'istruzione lshr dopo la mul e aggiornamento
        // utilizzi
        lshr->insertAfter(&inst);
        inst.replaceAllUsesWith(lshr);
        outs() << "Sostituzione di " << inst << " con " << *lshr << '\n';
        transformed = true;
      }
    }
  }
  return transformed;
}

bool multiInstructionOptimization(Instruction &inst1) {
  BasicBlock &B = *inst1.getParent();
  BasicBlock::iterator it = inst1.getIterator();
  BasicBlock::iterator nextIt = ++it;

  if (nextIt == B.end())
    return false;

  Instruction &inst2 = *nextIt;

  bool transformed = false;

  if (inst1.getOpcode() == Instruction::Add &&
      inst2.getOpcode() == Instruction::Sub) {
    ConstantInt *c0inst1 = dyn_cast<ConstantInt>(inst1.getOperand(0));
    ConstantInt *c1inst1 = dyn_cast<ConstantInt>(inst1.getOperand(1));
    ConstantInt *c0inst2 = dyn_cast<ConstantInt>(inst2.getOperand(0));
    ConstantInt *c1inst2 = dyn_cast<ConstantInt>(inst2.getOperand(1));

    // a = b + 1, c = a - 1 => a = b + 1, c = b
    if ((!c0inst1 && c1inst1) && (!c0inst2 && c1inst2) &&
        (&inst1 == inst2.getOperand(0)) &&
        (c1inst1->getValue() == c1inst2->getValue())) {
      inst2.replaceAllUsesWith(inst1.getOperand(0));
      outs() << "Sostituzione di " << inst2 << " con " << *inst1.getOperand(0)
             << '\n';
      transformed = true;
      // a = 1 + b, c = a - 1 => a = 1 + b, c = b
    } else if ((c0inst1 && !c1inst1) && (!c0inst2 && c1inst2) &&
               (&inst1 == inst2.getOperand(0)) &&
               (c0inst1->getValue() == c1inst2->getValue())) {
      inst2.replaceAllUsesWith(inst1.getOperand(1));
      outs() << "Sostituzione di " << inst2 << " con " << *inst1.getOperand(1)
             << '\n';
      transformed = true;
    }
  } else if (inst1.getOpcode() == Instruction::Sub &&
             inst2.getOpcode() == Instruction::Add) {
    ConstantInt *c0inst1 = dyn_cast<ConstantInt>(inst1.getOperand(0));
    ConstantInt *c1inst1 = dyn_cast<ConstantInt>(inst1.getOperand(1));
    ConstantInt *c0inst2 = dyn_cast<ConstantInt>(inst2.getOperand(0));
    ConstantInt *c1inst2 = dyn_cast<ConstantInt>(inst2.getOperand(1));
    // a = b - 1, c = a + 1 => a = b - 1, c = b
    if ((!c0inst1 && c1inst1) && (!c0inst2 && c1inst2) &&
        (&inst1 == inst2.getOperand(0)) &&
        (c1inst1->getValue() == c1inst2->getValue())) {
      inst2.replaceAllUsesWith(inst1.getOperand(0));
      outs() << "Sostituzione di " << inst2 << " con " << *inst1.getOperand(0)
             << '\n';
      transformed = true;
      // a = b - 1, c = 1 + a => a = b - 1, c = b
    } else if ((!c0inst1 && c1inst1) && (c0inst2 && !c1inst2) &&
               (&inst1 == inst2.getOperand(1)) &&
               (c1inst1->getValue() == c0inst2->getValue())) {
      inst2.replaceAllUsesWith(inst1.getOperand(0));
      outs() << "Sostituzione di " << inst2 << " con " << *inst1.getOperand(0)
             << '\n';
      transformed = true;
    }
  }

  return transformed;
}

bool runOnBasicBlock(BasicBlock &B) {

  //   // Preleviamo le prime due istruzioni del BB
  //   Instruction &Inst1st = *B.begin(), &Inst2nd = *(++B.begin());

  //   // L'indirizzo della prima istruzione deve essere uguale a quello del
  //   // primo operando della seconda istruzione (per costruzione
  //   dell'esempio) assert(&Inst1st == Inst2nd.getOperand(0));

  // 1° Assignment

  bool isModified = false;
  for (Instruction &inst : B) {
    if (algebraicIdentity(inst))
      isModified = true;
    if (strengthReduction(inst))
      isModified = true;
    if (multiInstructionOptimization(inst))
      isModified = true;
  }
  return isModified;
}

bool runOnFunction(Function &F) {
  bool Transformed = false;

  for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
    if (runOnBasicBlock(*Iter)) {
      Transformed = true;
    }
  }

  return Transformed;
}

PreservedAnalyses LocalOpts::run(Module &M, ModuleAnalysisManager &AM) {
  for (auto Fiter = M.begin(); Fiter != M.end(); ++Fiter)
    if (runOnFunction(*Fiter))
      return PreservedAnalyses::none();

  return PreservedAnalyses::all();
}
