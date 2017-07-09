#ifndef __MART_SEMU_GENMU_operatorClasses__FPAdd__
#define __MART_SEMU_GENMU_operatorClasses__FPAdd__

/**
 * -==== FPAdd.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Matching and replacement mutation operator for Integer ADD.
 * \details   This abstract class define is extended from the @see
 * NumericExpression_Base class.
 */

#include "../NumericArithBinop_Base.h"

namespace mart {

class FPAdd : public NumericArithBinop_Base {
protected:
  /**
   * \brief Implement from @see NumericArithBinop_Base
   */
  inline unsigned getMyInstructionIROpCode() { return llvm::Instruction::FAdd; }

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());
    llvm::Value *fadd = builder.CreateFAdd(oprd1_addrOprd, oprd2_intValOprd);
    if (!llvm::dyn_cast<llvm::Constant>(fadd))
      replacement.push_back(fadd);
    return fadd;
  }
}; // class FPAdd

} // namespace mart

#endif //__MART_SEMU_GENMU_operatorClasses__FPAdd__
