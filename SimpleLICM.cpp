//
// Created by myounghoshin on 4/14/2025.
//

#include "SimpleLICM.h"

#define DEBUG_TYPE "simplelicm"

using namespace llvm;

PreservedAnalyses SimpleLICMPass::run(Loop &L, LoopAnalysisManager &AM,
                                      LoopStandardAnalysisResults &AR,
                                      LPMUpdater &) {
    // Get the dominator tree from analysis results
    auto &DT = AR.DT;

    // Ensure the loop has a preheader since we need to move the invariant to preheader
    BasicBlock *preHeader = L.getLoopPreheader();
    if (!preHeader) {
        errs() << "[SimpleLICM] No preheader found, skipping. \n";
        return PreservedAnalyses::all();
    }

    // Perform the hoisting work
    bool changed = hoistInvariantInstructions(&L, DT);
    errs() << "[SimpleLICM] Finished. Changed: " << changed << "\n";
    // if it has been changed then we need to invalidate.
    // if no changes, then return all preserved analyses.
    return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

bool SimpleLICMPass::isLoopInvariant(llvm::Instruction &I, llvm::Loop *L) {
    // If the loop is not the basic block to contain its instruction then it can be moved
    if (!L->contains(I.getParent()))
        return true;

    // if it is load instruction then handle it separately.
    if (LoadInst *LI = dyn_cast<LoadInst>(&I)) {
        return isInvariantLoad(LI, L);
    }

    // Check if all operands are loop-invariant
    for (Value *op : I.operands()) {
        // if it is constant or arguments, it can be moved
        if (isa<Constant>(op) || isa<Argument>(op))
            continue;

        // For instructions, check if they're loop-invariant
        if (Instruction *opInst = dyn_cast<Instruction>(op)) {
            // If the operand is in the loop, ensure it's invariant
            if (L->contains(opInst->getParent())) {
                // Avoid infinite recursion by returning false for PHI nodes
                // that might reference the result of the instruction being analyzed
                if (isa<PHINode>(opInst) && opInst->getParent() == I.getParent())
                    return false;

                if (!isLoopInvariant(*opInst, L))
                    return false;
            }
        } else {
            // false for other types
            return false;
        }
    }

    return true;
}

bool SimpleLICMPass::isInvariantLoad(llvm::LoadInst *LI, llvm::Loop *L) {
    // Get the value Pointer
    Value *ptr = LI->getPointerOperand();

    // Check if this pointer is ever written to in the loop
    for (BasicBlock *BB : L->getBlocks()) {
        for (Instruction &I : *BB) {
            if (StoreInst *SI = dyn_cast<StoreInst>(&I)) {
                if (SI->getPointerOperand() == ptr) {
                    // This pointer is stored to in the loop
                    return false;
                }
            }
        }
    }

    return true;
}

bool SimpleLICMPass::hoistInvariantInstructions(llvm::Loop *L, llvm::DominatorTree &DT) {
    BasicBlock* preHeader = L->getLoopPreheader();
    if (!preHeader) return false;

    bool changed = false;

    // Identify all loop-invariant instructions
    SmallVector<Instruction*, 32> invariantInsts;

    for (BasicBlock* bb : L->getBlocks()) {
        for (auto& I : *bb) {
            if (I.isTerminator()) continue;
                        
            if (isLoopInvariant(I, L)) {
                invariantInsts.push_back(&I);
            }
        }
    }

    // Hoist instructions in correct order
    // Process instructions multiple times until no more hoisting can be done
    for (Instruction* I : invariantInsts) {
        // Skip instructions that are no longer in the loop
        if (!L->contains(I->getParent()))
            continue;

        // Check if all operands are available in the preheader
        bool allOperandsAvailable = true;
        for (Value* op : I->operands()) {
            if (Instruction* opInst = dyn_cast<Instruction>(op)) {
                // If operand is in the loop and hasn't been hoisted yet, we can't hoist this instruction yet
                if (L->contains(opInst->getParent())) {
                    allOperandsAvailable = false;
                    break;
                }
            }
        }

        if (allOperandsAvailable && !I->mayHaveSideEffects() && llvm::isSafeToSpeculativelyExecute(I)) {
            // Double-check that preheader dominates all uses
            bool safeToHoist = true;
            for (User* U : I->users()) {
                if (Instruction* userInst = dyn_cast<Instruction>(U)) {
                    BasicBlock* userBB = userInst->getParent();
                    if (!DT.dominates(preHeader, userBB)) {
                        safeToHoist = false;
                        errs() << "[SimpleLICM] Not safe to hoist: " << *I << " - preheader doesn't dominate use in: " << *userInst << "\n";
                        break;
                    }
                }
            }

            if (safeToHoist) {
                errs() << "[SimpleLICM] Hoisting: " << *I << "\n";
                I->moveBefore(preHeader->getTerminator());
                changed = true;
            }
        }
    }

    return changed;
}

// Match LLVM’s expected internal declaration
PassPluginLibraryInfo getSimpleLICMPluginInfo() {
    return {
            LLVM_PLUGIN_API_VERSION, "SimpleLICMPass", LLVM_VERSION_STRING,
            [](PassBuilder& PB) {
                PB.registerPipelineParsingCallback(
                        [](StringRef Name, LoopPassManager& LPM,
                           ArrayRef<PassBuilder::PipelineElement>) {
                            if (Name == "simple-licm") {
                                LPM.addPass(SimpleLICMPass());
                                return true;
                            }
                            return false;
                        });
            }
    };
}

#if defined(_WIN32)
#pragma comment(linker, "/EXPORT:llvmGetPassPluginInfo")
#endif
#ifndef LLVM_SIMPLELICM_LINK_INTO_TOOLS
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    llvm::errs() << "[SimpleLICM] Plugin loaded\n";
    return getSimpleLICMPluginInfo();
}
#endif
