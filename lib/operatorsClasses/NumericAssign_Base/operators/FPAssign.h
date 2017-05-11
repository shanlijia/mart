#ifndef __KLEE_SEMU_GENMU_operatorClasses__FPAssign__
#define __KLEE_SEMU_GENMU_operatorClasses__FPAssign__

/**
 * -==== FPAssign.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Matching and replacement mutation operator for Integer assignment.
 * \details   This abstract class define is extended from the @see NumericExpression_Base class. 
 */
 
#include "../NumericAssign_Base.h"

class FPAssign: public NumericAssign_Base
{
  protected:
    /**
     * \brief Implement from @see NumericAssign_Base
     */
    inline bool valTypeMatched(llvm::Type * type)
    {  
        return type->isFloatingPointTy();
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__FPAssign__
