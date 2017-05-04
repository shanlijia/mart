#ifndef __KLEE_SEMU_GENMU_operatorClasses__MatchOnly_Base__
#define __KLEE_SEMU_GENMU_operatorClasses__MatchOnly_Base__

/**
 * -==== MatchOnly_Base.h
 *
 *                LLGenMu LLVM Mutation Tool
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class for all match only mutation operator.
 * \details   This abstract class is extended from the Generic base class. 
 */
 
#include "../GenericMuOpBase.h"

class MatchOnly_Base: public GenericMuOpBase
{
    llvm::Value * createReplacement (llvm::Value * oprd1_addrOprd, llvm::Value * oprd2_intValOprd, std::vector<llvm::Value *> &replacement, ModuleUserInfos const &MI)
    {
        llvm::errs() << "Error: Calling 'createReplacement' method from a MatchOnly objects of the class: " /*<< typeid(*this).name()*/ << "\n";
        assert (false);
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__MatchOnly_Base__