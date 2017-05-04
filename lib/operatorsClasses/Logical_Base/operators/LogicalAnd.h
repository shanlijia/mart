#ifndef __KLEE_SEMU_GENMU_operatorClasses__LogicalAnd__
#define __KLEE_SEMU_GENMU_operatorClasses__LogicalAnd__

/**
 * -==== LogicalAnd.h
 *
 *                LLGenMu LLVM Mutation Tool
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief    Class for matching and replacing logical And
 */
 
#include "../Logical_Base.h"

class LogicalAnd: public Logical_Base
{
    /**
     * \bref Inplements virtual from @see GenericMuOpBase
     */
    llvm::Value * createReplacement (llvm::Value * oprd1_addrOprd, llvm::Value * oprd2_intValOprd, std::vector<llvm::Value *> &replacement, ModuleUserInfos const &MI)
    {
        llvm::errs() << "Not yet implemented here, still in Logical_Base \n";
        assert (false);
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__LogicalAnd__