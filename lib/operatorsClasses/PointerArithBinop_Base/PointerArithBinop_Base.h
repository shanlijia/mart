#ifndef __KLEE_SEMU_GENMU_operatorClasses__PointerArithBinop_Base__
#define __KLEE_SEMU_GENMU_operatorClasses__PointerArithBinop_Base__

/**
 * -==== PointerArithBinop_Base.h
 *
 *                LLGenMu LLVM Mutation Tool
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class for all mutation operator that match and replace binary arithmetic operation.
 * \details   This abstract class define is extended from the Generic base class. 
 * \todo      Frontend should make sure that the left had operand is the pointer and the right hand the integer
 */
 
#include "../GenericMuOpBase.h"

class PointerArithBinop_Base: public GenericMuOpBase
{
  protected:
    /**
     * \brief This method is to be implemented by each numeric pointer arithmetic binary operation matching-mutating operator
     * @return true if the parameter matches, else false.
     */
    inline virtual bool checkIntPartConst(llvm::ConstantInt * constIndxVal) = 0;
    inline virtual bool checkIntPartExp(llvm::Instruction *tmpI) = 0;
    
  public:
    bool matchIRs (std::vector<llvm::Value *> const &toMatch, llvmMutationOp const &mutationOp, unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) 
    {
        llvm::Value *val = toMatch.at(pos);
        if (auto *gep = llvm::dyn_cast<llvm::GetElementPtrInst>(val))
        {
            // check the pointer displacement index (0)'s value
            int indx;
            llvm::Value *indxVal = checkIsPointerIndexingAndGet(gep, indx);
            if (! indxVal)
                return false;
            if (llvm::ConstantInt * constIndxVal = llvm::dyn_cast<llvm::ConstantInt>(indxVal))
            {
                if (!checkIntPartConst(constIndxVal))
                    return false;
            }
            else    //it is a non constant
            {   
                llvm::Value *tmpI = indxVal;
                while (llvm::isa<llvm::CastInst>(tmpI))
                    tmpI = llvm::dyn_cast<llvm::User>(tmpI)->getOperand(0);
                if (! checkIntPartExp(llvm::dyn_cast<llvm::Instruction>(tmpI)))
                    return false;
            }
            
            // Make sure the pointer is the right type.  XXX: assumed that the front end made sure that the pointer is the left hand oprd (for case where , PADD(c,p), PADD(@,p))
            if (! checkCPTypeInIR (mutationOp.getCPType(0), gep->getPointerOperand()) || ! checkCPTypeInIR (mutationOp.getCPType(1), indxVal))
                return false;
            
            MatchUseful *ptr_mu = MU.getNew();
            ptr_mu->appendHLOprdsSource(pos, 0);            //Pointer oprd
            ptr_mu->appendHLOprdsSource(pos, indx + 1);     //Int oprd.   we add 1 because here we pass the user oprd while indx had the Gep access index
            ptr_mu->appendRelevantIRPos(pos);
            ptr_mu->setHLReturningIRPos(pos);
        }
        return (MU.first() != MU.end());
    }
    
    void prepareCloneIRs (std::vector<llvm::Value *> const &toMatch, unsigned pos,  MatchUseful const &MU, llvmMutationOp::MutantReplacors const &repl, DoReplaceUseful &DRU, ModuleUserInfos const &MI)
    {
        cloneStmtIR (toMatch, DRU.toMatchClone);
        llvm::Value *indxVal = MU.getHLOperandSource(1, DRU.toMatchClone);      //Integer oprd (secondly appended in matchIs)
        int oldPos = MU.getRelevantIRPosOf(0);
        int newPos = oldPos;
        int indx = MU.getHLOperandSourceIndexInIR(1) - 1;      //index according to Gep:   Get the in IR index of the Integer HLOprd (1) then remove 1
        if (indx > 0)
            newPos++;
            
        std::vector<llvm::Value *> extraIdx;
        
        llvm::GetElementPtrInst * preGep=nullptr, * postGep=nullptr;
        llvm::IRBuilder<> builder(MI.getContext());
        if (indx > 0)
        {
            llvm::GetElementPtrInst * curGI = llvm::dyn_cast<llvm::GetElementPtrInst>(DRU.toMatchClone[oldPos]);
            extraIdx.clear();
            for (auto i=0; i<indx;i++)
                extraIdx.push_back(*(curGI->idx_begin() + i));
            llvm::GetElementPtrInst * preGep = llvm::dyn_cast<llvm::GetElementPtrInst>(builder.CreateInBoundsGEP(curGI->getPointerOperand(), extraIdx));
        }
        if (indx < llvm::dyn_cast<llvm::GetElementPtrInst>(DRU.toMatchClone[oldPos])->getNumIndices()-1)
        {
            llvm::GetElementPtrInst * curGI = llvm::dyn_cast<llvm::GetElementPtrInst>(DRU.toMatchClone[oldPos]);
            extraIdx.clear();
            for (auto i=indx+1; i < llvm::dyn_cast<llvm::GetElementPtrInst>(DRU.toMatchClone[oldPos])->getNumIndices();i++)
                extraIdx.push_back(*(curGI->idx_begin() + i));
            llvm::GetElementPtrInst * postGep = llvm::dyn_cast<llvm::GetElementPtrInst>(builder.CreateInBoundsGEP(curGI, extraIdx));
            std::vector<std::pair<llvm::User *, unsigned>> affectedUnO;
            //set uses of the matched IR to corresponding OPRD
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            for (llvm::Value::use_iterator ui=curGI->use_begin(), ue=curGI->use_end(); ui!=ue; ++ui)
            {
                auto &U = ui.getUse();
                llvm::User *user = U.getUser();
                affectedUnO.push_back(std::pair<llvm::User *, unsigned>(user,ui.getOperandNo()));
#else
            for (auto &U: llvm::dyn_cast<llvm::Instruction>(curGI)->uses())
            {
                llvm::User *user = U.getUser();
                affectedUnO.push_back(std::pair<llvm::User *, unsigned>(user,U.getOperandNo()));
#endif
            }
            for(auto &affected: affectedUnO)
                if (affected.first != postGep)// && std::find(replacement.begin(), replacement.end(), affected.first) == replacement.end())     //avoid 'use loop': a->b->a
                    affected.first->setOperand(affected.second, postGep);
        }
        if (postGep)
            DRU.toMatchClone.insert(DRU.toMatchClone.begin() + oldPos + 1, postGep);
        if (preGep)
            DRU.toMatchClone.insert(DRU.toMatchClone.begin() + oldPos, preGep);
        llvm::Value * ptroprd = nullptr, *valoprd = nullptr;
        if (repl.getOprdIndexList().size() == 2)
        {
            if (preGep)
                ptroprd = preGep;
            else
                ptroprd = llvm::dyn_cast<llvm::GetElementPtrInst >(DRU.toMatchClone[newPos])->getPointerOperand();
            if (! (valoprd = createIfConst (indxVal->getType(), repl.getOprdIndexList()[1])))
                valoprd = indxVal;
        }
        else    //size is 1
        {
            if (ptroprd = createIfConst (indxVal->getType(), repl.getOprdIndexList()[0]))    
            {   //The replacor should be CONST_VALUE_OF
                ptroprd = builder.CreateIntToPtr(ptroprd, preGep? preGep->getType(): llvm::dyn_cast<llvm::GetElementPtrInst >(DRU.toMatchClone[newPos])->getPointerOperand()->getType());
                if (! llvm::isa<llvm::Constant>(ptroprd))
                    DRU.toMatchClone.insert(DRU.toMatchClone.begin() + newPos + 1, ptroprd);    //insert right after the instruction to remove
            }
            else if (repl.getOprdIndexList()[0] == 1)   //non pointer (integer)
            {   //The replacor should be either KEEP_ONE_OPRD
                ptroprd = builder.CreateIntToPtr(indxVal, preGep? preGep->getType(): llvm::dyn_cast<llvm::GetElementPtrInst >(DRU.toMatchClone[newPos])->getPointerOperand()->getType());
                if (! llvm::isa<llvm::Constant>(ptroprd))
                    DRU.toMatchClone.insert(DRU.toMatchClone.begin() + newPos + 1, ptroprd);    //insert right after the instruction to remove
            }
            else // pointer
            {
                if (preGep)
                    ptroprd = preGep;
                else
                    ptroprd = llvm::dyn_cast<llvm::GetElementPtrInst >(DRU.toMatchClone[newPos])->getPointerOperand();
            }
        }

        DRU.posOfIRtoRemove.push_back(newPos);
        DRU.appendHLOprds(ptroprd);
        DRU.appendHLOprds(valoprd);
        DRU.setOrigRelevantIRPos(MU.getRelevantIRPos());
        DRU.setHLReturningIRPos(MU.getHLReturningIRPos());
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__PointerArithBinop_Base__