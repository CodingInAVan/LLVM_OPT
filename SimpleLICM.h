//
// Created by myounghoshin on 4/14/2025.
//

#ifndef LLVM_OPTIMIZER_SIMPLELICM_H
#define LLVM_OPTIMIZER_SIMPLELICM_H

#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Function.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Transforms/Scalar/LoopPassManager.h>
#include <llvm/Analysis/DominanceFrontier.h>
#include "llvm/ADT/Statistic.h"
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/ValueTracking.h>
#include <iostream>

namespace {
    class SimpleLICMPass : public llvm::PassInfoMixin<SimpleLICMPass> {
    public:
        llvm::PreservedAnalyses run(llvm::Loop &L, llvm::LoopAnalysisManager &AM,
                                    llvm::LoopStandardAnalysisResults &AR,
                                    llvm::LPMUpdater &);
        static llvm::StringRef name() { return "SimpleLICM"; }

    private:
        bool isLoopInvariant(llvm::Instruction &I, llvm::Loop *L);
        bool isInvariantLoad(llvm::LoadInst *LI, llvm::Loop *L);
        bool hoistInvariantInstructions(llvm::Loop *L, llvm::DominatorTree &DT);
    };

}

#endif //LLVM_OPTIMIZER_SIMPLELICM_H
