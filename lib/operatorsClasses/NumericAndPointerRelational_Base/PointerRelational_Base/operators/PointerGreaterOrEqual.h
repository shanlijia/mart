#ifndef __KLEE_SEMU_GENMU_operatorClasses__PointerGreaterOrEqual__
#define __KLEE_SEMU_GENMU_operatorClasses__PointerGreaterOrEqual__

/**
 * -==== PointerGreaterOrEqual.h
 *
 *                LLGenMu LLVM Mutation Tool
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class for all Pointer GE relational mutation operator.
 * \details   This abstract class define is extended from the Generic base class. 
 */
 
#include "../PointerRelational_Base.h"

class PointerGreaterOrEqual: public PointerRelational_Base
{
  protected:
    /**
     * \brief Implements from NumericAndPointerRelational_Base
     */
    inline llvm::CmpInst::Predicate getMyPredicate ()
    {
        return llvm::CmpInst::ICMP_UGE;
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__PointerGreaterOrEqual__