#ifndef __KLEE_SEMU_GENMU_operatorClasses__PointerLessOrEqual__
#define __KLEE_SEMU_GENMU_operatorClasses__PointerLessOrEqual__

/**
 * -==== PointerLessOrEqual.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class for all Pointer LE relational mutation operator.
 * \details   This abstract class define is extended from the Generic base class. 
 */
 
#include "../PointerRelational_Base.h"

class PointerLessOrEqual: public PointerRelational_Base
{
  protected:
    /**
     * \brief Implements from NumericAndPointerRelational_Base
     */
    inline llvm::CmpInst::Predicate getMyPredicate ()
    {
        return llvm::CmpInst::ICMP_ULE;
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__PointerLessOrEqual__
