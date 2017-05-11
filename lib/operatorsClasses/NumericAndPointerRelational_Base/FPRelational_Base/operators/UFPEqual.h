#ifndef __KLEE_SEMU_GENMU_operatorClasses__UFPEqual__
#define __KLEE_SEMU_GENMU_operatorClasses__UFPEqual__

/**
 * -==== UFPEqual.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class for all UFP EQ relational mutation operator.
 * \details   This abstract class define is extended from the Generic base class. 
 */
 
#include "../FPRelational_Base.h"

class UFPEqual: public FPRelational_Base
{
  protected:
    /**
     * \brief Implements from NumericAndPointerRelational_Base
     */
    inline llvm::CmpInst::Predicate getMyPredicate ()
    {
        return llvm::CmpInst::FCMP_UEQ;
    }
    
    /**
     * \brief Implements from FPRelational_Base
     */
    inline bool isUnordedRel() 
    {
        return true;
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__UFPEqual__
