#ifndef __KLEE_SEMU_GENMU_operatorClasses__FPMinus__
#define __KLEE_SEMU_GENMU_operatorClasses__FPMinus__

/**
 * -==== FPMinus.h
 *
 *                LLGenMu LLVM Mutation Tool
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class for all non-pointer FP minus negation mutation operator.
 * \details   This abstract class define is extended from the Generic base class. 
 */
 
#include "../NumericArithNegation_Base.h"

class FPMinus: public NumericArithNegation_Base
{
  protected:
    /**
     * \brief Implements from @see NumericArithNegation_Base
     */
    inline int matchAndGetOprdID(llvm::BinaryOperator * propNeg)
    {
        int oprdId = -1;
        if (llvm::ConstantFP *theConst = llvm::dyn_cast<llvm::ConstantFP>(propNeg->getOperand(0)))
            if (propNeg->getOpcode() == llvm::Instruction::FSub)
                if (theConst->isExactlyValue(0.0))
                    oprdId = 1;
                
        return oprdId;
    }
  
  public:
    llvm::Value * createReplacement (llvm::Value * oprd1_addrOprd, llvm::Value * oprd2_intValOprd, std::vector<llvm::Value *> &replacement, ModuleUserInfos const &MI)
    {
        llvm::IRBuilder<> builder(MI.getContext()); 
        llvm::Value *fneg = builder.CreateFSub(llvm::ConstantFP::get(oprd1_addrOprd->getType(), 0.0), oprd1_addrOprd);
        if (!llvm::dyn_cast<llvm::Constant>(fneg))
            replacement.push_back(fneg);
        return fneg;
    } 
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__FPMinus__