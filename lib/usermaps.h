/**
 * -==== usermaps.h
 *
 *                LLGenMu LLVM Mutation Tool
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Define the class UserMaps which is used to map operation name (as string) with the corresponding opcode (expElemKeys).
 *                it also map each opcode to the corresponding mutation op's C++ class object
 */
 
#ifndef __KLEE_SEMU_GENMU_usermaps__
#define __KLEE_SEMU_GENMU_usermaps__

//#include "typesops.h"
#include <sstream>
#include <vector>
#include <map>

#include <llvm/IR/Module.h>
#include "llvm/IR/Value.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/DenseMap.h"

#include "llvm/IR/LLVMContext.h"

#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/DataLayout.h"

//enum replacementModes {rmNUMVAL, rmPTR, rmDEREF}  //TODO rermove this

enum codeParts {cpEXPR=0, cpCONSTNUM, cpVAR, cpADDRESS, cpPOINTER};

//enum typeOP {Arithetical, Relational, Logical, Bitwise, Assignement, Misc, Call, DelStmt};
//enum modeOP {Unary, Binary, None};	//None for DelStmt
enum ExpElemKeys {mALLSTMT=0, mALLFEXPR, mALLIEXPR, mANYFVAR, mANYIVAR, mANYFCONST, mANYICONST, mDELSTMT, mKEEP_ONE_OPRD, mCONST_VALUE_OF, //special, Delete stmt
            mIASSIGN, mFASSIGN, mADD, mFADD, mSUB, mFSUB, mMUL, mFMUL, mSDIV, mUDIV, mFDIV, mSMOD, mUMOD, mFMOD,  //Arithmetic Binary
            mNEG, mFNEG, mLEFTINC, mFLEFTINC, mRIGHTINC, mFRIGHTINC, mLEFTDEC, mFLEFTDEC, mRIGHTDEC, mFRIGHTDEC, mABS, mFABS,    //Arithmetic Unary
            mPADD, mPSUB, mPLEFTINC, mPRIGHTINC, mPLEFTDEC, mPRIGHTDEC,     //Pointer
            mBITAND, mBITOR, mBITXOR, mBITSHIFTLEFT, mABITSHIFTRIGHT, mLBITSHIFTRIGHT,    //Bitwise Binary
            mBITNOT,                                             //Bitwise Unary
            mFCMP_FALSE, mFCMP_OEQ, mFCMP_OGT, mFCMP_OGE, mFCMP_OLT, mFCMP_OLE, mFCMP_ONE, mFCMP_ORD, mFCMP_UNO, mFCMP_UEQ, mFCMP_UGT, mFCMP_UGE, mFCMP_ULT, mFCMP_ULE, mFCMP_UNE, mFCMP_TRUE, //Relational FP
            mICMP_EQ, mICMP_NE, mICMP_UGT, mICMP_UGE, mICMP_ULT, mICMP_ULE, mICMP_SGT, mICMP_SGE, mICMP_SLT, mICMP_SLE,   //Relational Int 
            mP_EQ, mP_NE, mP_GT, mP_GE, mP_LT, mP_LE,     //Relational ptr
            mAND, mOR,                              //Logical Binary
            mNOT,                                    //Logical Unary    TODO TODO: Add this to replace only
            
            mCALL, mNEWCALLEE,              // called function
            mRETURN_BREAK_CONTINUE,         // delete them by replacing unconditional 'br' target. for the final return with argument, set it to 0
            mPADD_DEREF, mPSUB_DEREF, mPDEREF_ADD, mPDEREF_SUB,     // Pointer/val operation combined with deref
            mPLEFTINC_DEREF, mPRIGHTINC_DEREF, mPLEFTDEC_DEREF, mPRIGHTDEC_DEREF, mPDEREF_LEFTINC, mPDEREF_RIGHTINC, mPDEREF_LEFTDEC, mPDEREF_RIGHTDEC,   // Pointer/val operation combined with deref
            /**** ADD HERE ****/
            
            
            /******************/
            mFORBIDEN_TYPE,   //Says that we cannot use an op for the type (float or int) - Do not Modify
            //enumExpElemKeysSIZE     //Number of elements in this enum  - Do not Modify 
          };

static bool isExpElemKeys_ForbidenType (enum ExpElemKeys me)
{
    return (me == mFORBIDEN_TYPE);
}

class GenericMuOpBase;

class UserMaps
{
public:   
    //using MatcherFuncType = void (*)(std::vector<llvm::Value *>&, llvmMutationOp &, std::vector<std::vector<llvm::Value *>>&);
private:
    llvm::DenseMap<unsigned/*enum ExpElemKeys*/, GenericMuOpBase *> mapOperationMatcher;
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    std::map<std::string, std::vector<enum ExpElemKeys>> mapConfnameOperations;
#else
    llvm::StringMap<std::vector<enum ExpElemKeys>> mapConfnameOperations;
#endif
    
    void addOpMatchObjectPair (enum ExpElemKeys op, GenericMuOpBase *muOpObj);
    
    void addConfNameOpPair (llvm::StringRef cname, std::vector<enum ExpElemKeys> ops);
    
public:
    inline bool isConstValOPRD(llvm::StringRef oprd);
    
    void validateNonConstValOPRD(llvm::StringRef oprd, unsigned lno);
    
    static enum codeParts getCodePartType (llvm::StringRef oprd);
    
    bool isDeleteStmtConfName(llvm::StringRef s); 
    
    GenericMuOpBase * getMatcherObject(enum ExpElemKeys opKey);
    
    std::vector<enum ExpElemKeys> * getExpElemKeys(llvm::StringRef operation, std::string &confexp, unsigned confline);
    
    void clearAll ();
    
    ~UserMaps();
    
    UserMaps();
    
};

#endif  //__KLEE_SEMU_GENMU_usermaps__