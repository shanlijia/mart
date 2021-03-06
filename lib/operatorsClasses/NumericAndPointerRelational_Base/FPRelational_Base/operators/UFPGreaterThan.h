#ifndef __MART_GENMU_operatorClasses__UFPGreaterThan__
#define __MART_GENMU_operatorClasses__UFPGreaterThan__

/**
 * -==== UFPGreaterThan.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all UFP GT relational mutation
 * operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../FPRelational_Base.h"

namespace mart {

class UFPGreaterThan : public FPRelational_Base {
protected:
  /**
   * \brief Implements from NumericAndPointerRelational_Base
   */
  inline llvm::CmpInst::Predicate getMyPredicate() {
    return llvm::CmpInst::FCMP_UGT;
  }

  /**
   * \brief Implements from FPRelational_Base
   */
  inline bool isUnordedRel() { return true; }
}; // class UFPGreaterThan

} // namespace mart

#endif //__MART_GENMU_operatorClasses__UFPGreaterThan__
