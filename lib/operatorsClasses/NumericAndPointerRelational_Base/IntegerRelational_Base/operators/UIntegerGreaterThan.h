#ifndef __KLEE_SEMU_GENMU_operatorClasses__UIntegerGreaterThan__
#define __KLEE_SEMU_GENMU_operatorClasses__UIntegerGreaterThan__

/**
 * -==== UIntegerGreaterThan.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class for all UInteger GT relational mutation operator.
 * \details   This abstract class define is extended from the Generic base class. 
 */
 
#include "../IntegerRelational_Base.h"

class UIntegerGreaterThan: public IntegerRelational_Base
{
  protected:
    /**
     * \brief Implements from NumericAndPointerRelational_Base
     */
    inline llvm::CmpInst::Predicate getMyPredicate ()
    {
        return llvm::CmpInst::ICMP_UGT;
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__UIntegerGreaterThan__
