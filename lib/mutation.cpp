
#include <vector>
#include <set>
#include <queue>
#include <sstream>
#include <regex>
#include <fstream>

#include "mutation.h"
#include "usermaps.h"
#include "typesops.h"
#include "tce.h"    //Trivial Compiler Equivalence
#include "operatorsClasses/GenericMuOpBase.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/GlobalVariable.h"
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
#include "llvm/PassManager.h"    //This is needed for 3rd param of SplitBlock
#include "llvm/Analysis/RegionInfo.h"   //This is needed for 3rd param of SplitBlock
#include "llvm/Analysis/Verifier.h"
#include "llvm/Linker.h" //for Linker
#else
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h" //for Linker
#endif
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Transforms/Utils/Cloning.h"  //for CloneModule

Mutation::Mutation(llvm::Module &module, std::string mutConfFile, DumpMutFunc_t writeMutsF, std::string scopeJsonFile): forKLEESEMu(true), writeMutantsCallback(writeMutsF), moduleInfo (&module, &usermaps)
{
    //set module
    currentInputModule = &module;
    currentMetaMutantModule = currentInputModule;       //for now the input is transformed (mutated to become mutant)
    
    //get mutation config (operators)
    assert(getConfiguration(mutConfFile) && "@Mutation(): getConfiguration(mutconfFile) Failed!");
    
    //Get scope info
    mutationScope.Initialize(module, scopeJsonFile);
    
    //initialize mutantIDSelectorName
    getanothermutantIDSelectorName();
    currentFunction = nullptr;
    currentBasicBlock = nullptr;
    curMutantID = 0;
}


bool Mutation::getConfiguration(std::string &mutConfFile)
{
    //TODO
    std::vector<unsigned> reploprd;
    std::vector<llvmMutationOp> mutationOperations;
    std::vector<std::string> matchoprd;
    std::vector<enum ExpElemKeys> *correspKeysMatch;
    std::vector<enum ExpElemKeys> *correspKeysMutant;
    
    std::ifstream infile(mutConfFile);
    if (infile)
    {
        std::string linei;
        std::vector<std::string> matchop_oprd;
        unsigned confLineNum = 0;
        while (infile)
        {
            confLineNum++;
            std::getline(infile, linei); 
            
            //remove comment (Everything after the '#' character)
            size_t commentInd = linei.find_first_of('#');
            if (commentInd != std::string::npos)
                linei.erase(commentInd, std::string::npos);
            
            //skip white(empty) line
            if (std::regex_replace(linei, std::regex("^\\s+|\\s+$"), std::string("$1")).length() == 0)
            {
                //llvm::errs() << "#"<<linei <<"#\n";
                continue;
            }
            
            std::regex rgx("\\s+\-\->\\s+");        // Matcher --> Replacors
            std::sregex_token_iterator iter(linei.begin(),linei.end(), rgx, -1);
            std::sregex_token_iterator end;
            unsigned short matchRepl = 0;
            for ( ; iter != end; ++iter)
            {
                if (matchRepl == 0)
                {
                    std::string matchstr2(*iter);
                    std::regex rgx2("\\s*,\\s*|\\s*\\(\\s*|\\s*\\)\\s*");        // operation (operand1,operand2,...) 
                    std::sregex_token_iterator iter2(matchstr2.begin(), matchstr2.end(), rgx2, -1);
                    std::sregex_token_iterator end2;
                    
                    matchoprd.clear();
                    for ( ; iter2 != end2; ++iter2)
                    {
                        //Position 0 for operation, rest for operands
                        matchoprd.push_back(std::regex_replace(std::string(*iter2), std::regex("^\\s+|\\s+$"), std::string("$1")));
                    }
                    
                    //when we match @ or C or V or A or P, they are their own params
                    if (matchoprd.size() == 1 && usermaps.isConstValOPRD(matchoprd.back()))
                    {
                        //I am my own parameter
                        matchoprd.push_back(matchoprd.back());
                    }
                    
                    mutationOperations.clear();
                    
                    correspKeysMatch = usermaps.getExpElemKeys (matchoprd.front(), matchstr2, confLineNum);   //floats then ints. EX: OF, UF, SI, UI
                    for (unsigned i=0; i < correspKeysMatch->size(); i++)
                    {
                        mutationOperations.push_back(llvmMutationOp());
                        mutationOperations.back().setMatchOp(correspKeysMatch->at(i), matchoprd, 1);
                    }
                }
                else if (matchRepl == 1)
                {
                    std::string matchstr3(*iter);
                    std::regex rgx3("\\s*;\\s*");        // Matcher --> Replacors
                    std::sregex_token_iterator iter3(matchstr3.begin(),matchstr3.end(), rgx3, -1);
                    std::sregex_token_iterator end3;
                    for ( ; iter3 != end3; ++iter3)     //For each replacor
                    {
                        std::string tmprepl(std::regex_replace(std::string(*iter3), std::regex("^\\s+|\\s+$"), std::string("$1")));
                        std::regex rgx4("\\s*,\\s*|\\s*\\(\\s*|\\s*\\)\\s*");        // MutName, operation(operand1,operand2,...)
                        std::sregex_token_iterator iter4(tmprepl.begin(),tmprepl.end(), rgx4, -1);
                        std::sregex_token_iterator end4;
                        std::string mutName(*iter4);
                        ++iter4; assert (iter4 != end4 && "only Mutant name, no info!");
                        std::string mutoperation(*iter4);
                        
                        //If replace with constant number, add it here
                        auto contvaloprd = llvmMutationOp::insertConstValue (mutoperation, true);   //numeric=true
                        if (llvmMutationOp::isSpecifiedConstIndex(contvaloprd))
                        {
                            reploprd.clear();
                            reploprd.push_back(contvaloprd); 
                            for (auto & mutOper:mutationOperations)
                                mutOper.addReplacor(mCONST_VALUE_OF, reploprd, mutName);
                            ++iter4; assert (iter4 == end4 && "Expected no mutant operand for const value replacement!");
                            continue;
                        }
                           
                        ++iter4; assert (iter4 != end4 && "no mutant operands");
                        
                        reploprd.clear();
                        
                        for ( ; iter4 != end4; ++iter4)     //for each operand
                        {
                            if ((*iter4).length() == 0)
                            {
                                if (! usermaps.isDeleteStmtConfName(mutoperation))
                                { 
                                    llvm::errs() << "missing operand at line " << confLineNum << "\n";
                                    assert (false && "");
                                }
                                continue;
                            }
                                
                            bool found = false;
                            unsigned pos4 = 0;
                            
                            //If constant number, directly add it
                            if (llvmMutationOp::isSpecifiedConstIndex(pos4 = llvmMutationOp::insertConstValue (std::string(*iter4), true)))     //numeric=true
                            {
                                reploprd.push_back(pos4); 
                                continue;
                            }
                            
                            // If CALLED Function replacement
                            if (mutationOperations.back().getMatchOp() == mCALL)
                            {
                                for (auto &obj: mutationOperations)
                                    assert (obj.getMatchOp() == mCALL && "All 4 types should be mCALL here");
                                pos4 = llvmMutationOp::insertConstValue (std::string(*iter4), false);
                                assert (llvmMutationOp::isSpecifiedConstIndex(pos4) && "Insertion should always work here");
                                reploprd.push_back(pos4); 
                                continue;
                            }
                            
                            //Make sure that the parameter is either anything or any variable or any pointer or any constant
                            usermaps.validateNonConstValOPRD(llvm::StringRef(*iter4), confLineNum);
                            
                            //search the operand position
                            for (std::vector<std::string>::iterator it41 = matchoprd.begin()+1; it41 != matchoprd.end(); ++it41)
                            {
                                if (llvm::StringRef(*it41).equals_lower(llvm::StringRef(*iter4)))
                                {
                                    found = true;
                                    break;
                                }
                                pos4++;
                            }
                            
                            if (found)
                            {
                                reploprd.push_back(pos4);    //map to Matcher operand
                            }
                            else
                            {
                                llvm::errs() << "Error in the replacor parameters (do not match any of the Matcher's): '"<<tmprepl<<"', confLine("<<confLineNum<<")!\n";
                                return false;
                            }
                        }
                        
                        correspKeysMutant = usermaps.getExpElemKeys (mutoperation, tmprepl, confLineNum);   //floats then ints. EX: FDiv, SDiv, UDiv
                        
                        assert (correspKeysMutant->size() == mutationOperations.size() && "Incompatible types Mutation oprerator");
                        
                        unsigned iM=0;
                        while (iM < correspKeysMutant->size() && (isExpElemKeys_ForbidenType(mutationOperations[iM].getMatchOp()) || isExpElemKeys_ForbidenType(correspKeysMutant->at(iM))))
                        {
                            iM++;
                        }
                        if (iM >= correspKeysMutant->size())
                            continue;
                            
                        mutationOperations[iM].addReplacor(correspKeysMutant->at(iM), reploprd, mutName);
                        enum ExpElemKeys prevMu = correspKeysMutant->at(iM);
                        enum ExpElemKeys prevMa = mutationOperations[iM].getMatchOp();
                        for (iM=iM+1; iM < correspKeysMutant->size(); iM++)
                        {
                            if (! isExpElemKeys_ForbidenType(mutationOperations[iM].getMatchOp()) && ! isExpElemKeys_ForbidenType(correspKeysMutant->at(iM)))
                            {
                                if (correspKeysMutant->at(iM) != prevMu || mutationOperations[iM].getMatchOp() != prevMa)
                                {    
                                    mutationOperations[iM].addReplacor(correspKeysMutant->at(iM), reploprd, mutName);
                                    prevMu = correspKeysMutant->at(iM);
                                    prevMa = mutationOperations[iM].getMatchOp();
                                }
                            }
                        }
                    }
                    
                    //Remove the matcher without replacors
                    for (unsigned itlM=0; itlM < mutationOperations.size();)
                    {
                        if (mutationOperations.at(itlM).getNumReplacor() < 1)
                            mutationOperations.erase(mutationOperations.begin() + itlM);
                        else
                            ++itlM;
                    }
                    
                    //Finished extraction for a line, Append to cofiguration
                    for (auto &opsofmatch: mutationOperations)
                    {
                        configuration.mutators.push_back(opsofmatch);
                    }
                }
                else
                {   //should not be reached
                    llvm::errs() << "Invalid Line: '" << linei << "'\n";
                    return false; //assert (false && "Invalid Line!!!")
                }
                
                matchRepl++;
            }
        }
    }
    else
    {
        llvm::errs() << "Error while opening (or empty) mutant configuration '" << mutConfFile << "'\n";
        return false;
    }
    
    // @ Make sure that the redundant deletions are removed (delete any stmt -> 
    // @    -   remove deletion for others with straight deletion
    // @    -   add those with not stright deletion if absent 'like delete return, break and continue'.) 
    //TODO: above       TODO TODO TODO
    
    /*//DEBUG
    for (llvmMutationOp oops: configuration.mutators)   //DEBUG
        llvm::errs() << oops.toString();    //DEBUG
    return false;   //DEBUG*/
    
    return true; 
}

void Mutation::getanothermutantIDSelectorName()
{
     static unsigned tempglob = 0;
     mutantIDSelectorName.assign("klee_semu_GenMu_Mutant_ID_Selector");
     if (tempglob > 0)
        mutantIDSelectorName.append(std::to_string(tempglob));
     tempglob++;
     
     mutantIDSelectorName_Func.assign(mutantIDSelectorName + "_Func");
}

/* Assumes that the IRs instructions in stmtIR have same order as the initial original IR code*/
//@@TODO Remove this Function : UNSUSED
llvm::BasicBlock * Mutation::getOriginalStmtBB (std::vector<llvm::Value *> &stmtIR, unsigned stmtcount)
{
    llvm::SmallDenseMap<llvm::Value *, llvm::Value *> pointerMap;
    llvm::BasicBlock* block = llvm::BasicBlock::Create(moduleInfo.getContext(), std::string("KS.original_Mut0.")+std::to_string(stmtcount), currentFunction, currentBasicBlock);
    for (llvm::Value * I: stmtIR)
    { 
        //clone instruction
        llvm::Value * newI = llvm::dyn_cast<llvm::Instruction>(I)->clone();
        
        //set name
        if (I->hasName())
            newI->setName((I->getName()).str()+"_Mut0");
        
        if (! pointerMap.insert(std::pair<llvm::Value *, llvm::Value *>(I, newI)).second)
        {
            llvm::errs() << "Error (Mutation::getOriginalStmtBB): inserting an element already in the map\n";
            return nullptr;
        }
    };
    for (llvm::Value * I: stmtIR)
    {
        for(unsigned opos = 0; opos < llvm::dyn_cast<llvm::User>(I)->getNumOperands(); opos++)
        {
            auto oprd = llvm::dyn_cast<llvm::User>(I)->getOperand(opos);
            if (llvm::isa<llvm::Instruction>(oprd))
            {
                if (auto newoprd = pointerMap.lookup(oprd))    //TODO:Double check the use of lookup for this map
                {
                    llvm::dyn_cast<llvm::User>(pointerMap.lookup(I))->setOperand(opos, newoprd);  //TODO:Double check the use of lookup for this map
                }
                else
                {
                    bool fail = false;
                    switch (opos)
                    {
                        case 0:
                            {
                                if (llvm::isa<llvm::StoreInst>(I))
                                    fail = true;
                                break;
                            }
                        case 1:
                            {
                                if (llvm::isa<llvm::LoadInst>(I))
                                    fail = true;
                                break;
                            }
                        default:
                            fail = true;
                    }
                    
                    if (fail)
                    {
                        llvm::errs() << "Error (Mutation::getOriginalStmtBB): lookup an element not in the map -- "; 
                        llvm::dyn_cast<llvm::Instruction>(I)->dump();
                        return nullptr;
                    }
                }
            }
        }
        block->getInstList().push_back(llvm::dyn_cast<llvm::Instruction>(pointerMap.lookup(I)));   //TODO:Double check the use of lookup for this map
    }
    
    if (!block)
    {
         llvm::errs() << "Error (Mutation::getOriginalStmtBB): original Basic block is NULL";
    }
    
    return block;
}

// @Name: Mutation::getMutantsOfStmt
// This function takes a statement as a list of IR instruction, using the 
// mutation model specified for this class, generate a list of all possible mutants
// of the statement
void Mutation::getMutantsOfStmt (std::vector<llvm::Value *> const &stmtIR, MutantsOfStmt &ret_mutants, ModuleUserInfos const &moduleInfo)
{
    assert ((ret_mutants.getNumMuts() == 0) && "Error (Mutation::getMutantsOfStmt): mutant list result vector is not empty!\n");
    
    bool isDeleted = false;
    for (llvmMutationOp mutator: configuration.mutators)
    {
        /*switch (mutator.getMatchOp())
        {
            case mALLSTMT:
            {
                assert ((mutator.getNumReplacor() == 1 && mutator.getReplacor(0).first == mDELSTMT) && "only Delete Stmt affect whole statement and match anything");
                if (!isDeleted && ! llvm::dyn_cast<llvm::Instruction>(stmtIR.back())->isTerminator())     //This to avoid deleting if condition or 'ret'
                {
                    ret_mutants.push_back(std::vector<llvm::Value *>());
                    isDeleted = true;
                }
                break;
            }
            default:    //Anything beside math anything and delete whole stmt
            {*/
        usermaps.getMatcherObject(mutator.getMatchOp())->matchAndReplace (stmtIR, mutator, ret_mutants, isDeleted, moduleInfo);
        
        // Verify that no constant is considered as instruction in the mutant (inserted in replacement vector)
        /*# llvm::errs() << "\n@orig\n";   //DBG
        for (auto *dd: stmtIR)          //DBG
            dd->dump();                 //DBG*/
        for (auto ind = 0; ind < ret_mutants.getNumMuts(); ind++)
        {    
            auto &mutInsVec = ret_mutants.getMutantStmtIR(ind);
            /*# llvm::errs() << "\n@Muts\n";    //DBG*/
            for (auto *mutIns: mutInsVec)
            {
                /*# mutIns->dump();     //DBG*/
                if(llvm::dyn_cast<llvm::Constant>(mutIns))
                {
                    llvm::errs() << "\nError: A constant is considered as Instruction (inserted in 'replacement') for mutator (enum ExpElemKeys): " << mutator.getMatchOp() << "\n\n";
                    mutIns->dump();
                    assert (false);
                }
            }
        }
            //}
        //}
    }
}//~Mutation::getMutantsOfStmt

llvm::Function * Mutation::createGlobalMutIDSelector_Func(llvm::Module &module, bool bodyOnly)
{
    llvm::Function *funcForKS = nullptr;
    if (!bodyOnly)
    {
        llvm::Constant* c = module.getOrInsertFunction(mutantIDSelectorName_Func,
                                         llvm::Type::getVoidTy(moduleInfo.getContext()),
                                         llvm::Type::getInt32Ty(moduleInfo.getContext()),
                                         llvm::Type::getInt32Ty(moduleInfo.getContext()),
                                         NULL);
        funcForKS = llvm::cast<llvm::Function>(c);
        assert (funcForKS && "Failed to create function 'GlobalMutIDSelector_Func'");
    }
    else
    {
        funcForKS = module.getFunction(mutantIDSelectorName_Func);
        assert (funcForKS && "function 'GlobalMutIDSelector_Func' is not existing");
    }
    llvm::BasicBlock* block = llvm::BasicBlock::Create(moduleInfo.getContext(), "entry", funcForKS);
    llvm::IRBuilder<> builder(block);
    llvm::Function::arg_iterator args = funcForKS->arg_begin();
    llvm::Value* x = llvm::dyn_cast<llvm::Value>(args++);
    x->setName("mutIdFrom");
    llvm::Value* y = llvm::dyn_cast<llvm::Value>(args++);
    y->setName("mutIdTo");
    //builder.CreateBinOp(llvm::Instruction::Mul, x, y, "tmp");*/
    builder.CreateRetVoid();
    
    return funcForKS;
}

// @Name: doMutate
// This is the main method of the class Mutation, Call this to mutate a module
bool Mutation::doMutate()
{
    llvm::Module &module = *currentMetaMutantModule;
    
    //Instert Mutant ID Selector Global Variable
    while (module.getNamedGlobal(mutantIDSelectorName))
    {
        llvm::errs() << "The gobal variable '" << mutantIDSelectorName << "' already present in code!\n";
        assert (false && "ERROR: Module already mutated!"); //getanothermutantIDSelectorName();
    }
    module.getOrInsertGlobal(mutantIDSelectorName, llvm::Type::getInt32Ty(moduleInfo.getContext()));
    llvm::GlobalVariable *mutantIDSelectorGlobal = module.getNamedGlobal(mutantIDSelectorName);
    //mutantIDSelectorGlobal->setLinkage(llvm::GlobalValue::CommonLinkage);             //commonlinkage require 0 as initial value
    mutantIDSelectorGlobal->setAlignment(4);
    mutantIDSelectorGlobal->setInitializer(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, 0, false)));
    
    //TODO: Insert definition of the function whose call argument will tell KLEE-SEMU which mutants to fork
    if (forKLEESEMu)
    {
        funcForKLEESEMu = createGlobalMutIDSelector_Func(module);
    }
    
    //mutate
    unsigned mod_mutstmtcount = 0;
    for (auto &Func: module)
    {
        
        //Skip Function with only Declaration (External function -- no definition)
        if (Func.isDeclaration())
            continue;
            
        if (forKLEESEMu && funcForKLEESEMu == &Func)
            continue;
        
        currentFunction = &Func;
        
        unsigned instructionPosInFunc = 0;
        
        for (auto itBBlock = Func.begin(), F_end = Func.end(); itBBlock != F_end; ++itBBlock)
        { 
            currentBasicBlock = &*itBBlock;
            
            std::vector<std::vector<llvm::Value *>> sourceStmts;
            std::vector<std::vector<unsigned>> srcStmtsPos;         //Alway go together with sourceStmts (same modifications - push, pop,...)
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            std::set<llvm::Value *> visited;
#else
            llvm::SmallPtrSet<llvm::Value *, 5> visited;
#endif
            std::queue<llvm::Value *> curUses;
            
            //Used to maintain same order of instruction as originally when extracting source statement
            int stmtIRcount = 0;
            
            for (auto &Instr: *itBBlock)
            {
                instructionPosInFunc++;
                
                //Skip llvm debugging functions void @llvm.dbg.declare and void @llvm.dbg.value
                if (auto * callinst = llvm::dyn_cast<llvm::CallInst>(&Instr))
                {
                    if (llvm::Function *fun = callinst->getCalledFunction())    //TODO: handle function alias
                    {
                        if(fun->getName().startswith("llvm.dbg.") && fun->getReturnType()->isVoidTy())
                        {
                            if((/*callinst->getNumArgOperands()==2 && */fun->getName().equals("llvm.dbg.declare")) ||
                                (/*callinst->getNumArgOperands()==3 && */fun->getName().equals("llvm.dbg.value")))
                            {
                                if (visited.count(&Instr)) 
                                {
                                    assert (false && "The debug statement should not have been in visited (cause no dependency on others stmts...)");
                                    sourceStmts.pop_back();
                                    srcStmtsPos.pop_back();
                                }
                                continue;
                            }
                        }
                        if (forKLEESEMu && fun->getName().equals("klee_make_symbolic") && callinst->getNumArgOperands() == 3 && fun->getReturnType()->isVoidTy())
                        {
                            if (visited.count(&Instr)) 
                            {
                                sourceStmts.pop_back();     //do not mutate klee_make_symbolic
                                srcStmtsPos.pop_back();
                                stmtIRcount = 0;
                            }
                            continue;
                        }
                    }
                }
               
                if (visited.count(&Instr))  //is it visited?
                {
                    if (stmtIRcount <= 0)
                    {
                        if (stmtIRcount < 0)
                            llvm::errs() << "Error (Mutation::doMutate): 'stmtIRcount' should never be less than 0; Possibly bug in the porgram.\n";
                        else
                            llvm::errs() << "Error (Mutation::doMutate): Instruction appearing multiple times.\n";
                        return false;
                    }
                    sourceStmts.back().push_back(&Instr);
                    srcStmtsPos.back().push_back(instructionPosInFunc - 1);
                    stmtIRcount--;
                    
                    continue;
                }
                
                //Check that Statements are atomic (all IR of stmt1 before any IR of stmt2, except Alloca - actually all allocas are located at the beginning of the function)
                if (stmtIRcount > 0)
                {
                    llvm::errs() << "Error (Mutation::doMutate): Problem with IR - statements are not atomic (" << stmtIRcount << ").\n";
                    for(auto * rr:sourceStmts.back()) //DEBUG
                        llvm::dyn_cast<llvm::Instruction>(rr)->dump(); //DEBUG
                    return false;
                }
                
                //Do not mind Alloca
                if (llvm::isa<llvm::AllocaInst>(&Instr))
                    continue;
                
                /* //Commented because the mutating function do no delete stmt with terminator instr (to avoid misformed while), but only delete for return break and continue in this case
                //make the final unconditional branch part of this statement (to avoid multihop empty branching)
                if (llvm::isa<llvm::BranchInst>(&Instr))
                {
                    if (llvm::dyn_cast<llvm::BranchInst>(&Instr)->isUnconditional() && !visited.empty())
                    {
                        sourceStmts.back().push_back(&Instr);
                        srcStmtsPos.back().push_back(instructionPosInFunc - 1);
                        continue;
                    }
                }*/
                
                visited.clear();
                sourceStmts.push_back(std::vector<llvm::Value *>());
                srcStmtsPos.push_back(std::vector<unsigned>());
                sourceStmts.back().push_back(&Instr); 
                srcStmtsPos.back().push_back(instructionPosInFunc - 1);
                visited.insert(&Instr);
                curUses.push(&Instr);
                while (! curUses.empty())
                {
                    llvm::Value *popInstr = curUses.front();
                    curUses.pop();
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                    for (llvm::Value::use_iterator ui=popInstr->use_begin(), ue=popInstr->use_end(); ui!=ue; ++ui)
                    {
                        auto &U = ui.getUse();
#else
                    for (auto &U: popInstr->uses())
                    {
#endif
                        if (visited.insert(U.getUser()).second)
                        {
                            curUses.push(U.getUser());
                            stmtIRcount++;
                        }
                    }
                    //consider only operands when more than 1 (popInstr is a user or operand or Load or Alloca)
                    //if (llvm::dyn_cast<llvm::User>(popInstr)->getNumOperands() > 1)
                    if (!(llvm::isa<llvm::AllocaInst>(popInstr)))
                    {
                        for(unsigned opos = 0; opos < llvm::dyn_cast<llvm::User>(popInstr)->getNumOperands(); opos++)
                        {
                            auto oprd = llvm::dyn_cast<llvm::User>(popInstr)->getOperand(opos);
                            
                            //@ Check that oprd is not Alloca (done already above 'if')..
                            
                            if (!oprd || llvm::isa<llvm::AllocaInst>(oprd)) // || llvm::isa<llvm::LoadInst>(oprd))
                                continue;
                                
                            if (llvm::dyn_cast<llvm::Instruction>(oprd) && visited.insert(oprd).second)
                            {
                                curUses.push(oprd);
                                stmtIRcount++;
                            }
                        }
                    }
                }
                //curUses is empty here
            }
            
            // \brief Actual mutation
            //ModuleUserInfos moduleInfo (&module, &usermaps);
            llvm::BasicBlock * sstmtCurBB = &*itBBlock;
            for (auto sstmts_ind=0; sstmts_ind < sourceStmts.size(); sstmts_ind++)
            {
                auto &sstmt = sourceStmts[sstmts_ind];
                auto &sstmtpos = srcStmtsPos[sstmts_ind];
                /*llvm::errs() << "\n";   
                for (auto ins: sstmt) 
                {
                    llvm::errs() << ">> ";ins->dump();//U.getUser()->dump();
                }*/
                
                //llvm::BasicBlock * original = getOriginalStmtBB(sstmt, mod_mutstmtcount++);
                //if (! original)
                //    return false;
                
                // Find all mutants and put into 'mutantStmt_list'
                MutantsOfStmt mutantStmt_list;
                getMutantsOfStmt (sstmt, mutantStmt_list, moduleInfo);
                unsigned nMuts = mutantStmt_list.getNumMuts();
                
                //Mutate only when mutable: at least one mutant (nMuts > 0)
                if (nMuts > 0)
                {
                    llvm::Instruction * firstInst = llvm::dyn_cast<llvm::Instruction>(sstmt.front());

#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                    llvm::PassManager PM;
                    llvm::RegionInfo * tmp_pass = new llvm::RegionInfo();
                    PM.add(tmp_pass);   //tmp_pass must be created with 'new'
                    llvm::BasicBlock * original = llvm::SplitBlock(sstmtCurBB, firstInst, tmp_pass);
#else                
                    llvm::BasicBlock * original = llvm::SplitBlock(sstmtCurBB, firstInst);
#endif
                    original->setName(std::string("KS.original_Mut0.Stmt")+std::to_string(mod_mutstmtcount));
                
                    llvm::Instruction * linkterminator = sstmtCurBB->getTerminator();    //this cannot be nullptr because the block just got splitted
                    llvm::IRBuilder<> sbuilder(linkterminator);
                    
                    //XXX: Insert definition of the function whose call argument will tell KLEE-SEMU which mutants to fork (done elsewhere)
                    if (forKLEESEMu)
                    {
                        std::vector<llvm::Value*> argsv;
                        argsv.push_back(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)(curMutantID+1), false)));
                        argsv.push_back(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)(curMutantID+nMuts), false)));
                        sbuilder.CreateCall(funcForKLEESEMu, argsv);
                    }
                    
                    llvm::SwitchInst * sstmtMutants = sbuilder.CreateSwitch (sbuilder.CreateAlignedLoad(mutantIDSelectorGlobal, 4), original, nMuts);
                    
                    //Remove old terminator link
                    linkterminator->eraseFromParent();
                    
                    //Separate Mutants(including original) BB from rest of instr
                    if (! llvm::dyn_cast<llvm::Instruction>(sstmt.back())->isTerminator())    //if we have another stmt after this in this BB
                    {
                        firstInst = llvm::dyn_cast<llvm::Instruction>(sstmt.back())->getNextNode();
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                        llvm::BasicBlock * nextBB = llvm::SplitBlock(original, firstInst, tmp_pass);
#else                         
                        llvm::BasicBlock * nextBB = llvm::SplitBlock(original, firstInst);
#endif
                        nextBB->setName(std::string("KS.BBafter.Stmt")+std::to_string(mod_mutstmtcount));
                        
                        sstmtCurBB = nextBB;
                    }
                    else
                    {
                        //llvm::errs() << "Error (Mutation::doMutate): Basic Block '" << original->getName() << "' has no terminator!\n";
                        //return false;
                        sstmtCurBB = original;
                    }
                    
                    //XXX: Insert mutant blocks here
                    //@# MUTANTS (see ELSE bellow)
                    for (auto ms_ind = 0; ms_ind < mutantStmt_list.getNumMuts(); ms_ind++)
                    {
                        auto &mut_vect = mutantStmt_list.getMutantStmtIR(ms_ind);
                        std::string mutIDstr(std::to_string(++curMutantID));
                        
                        // Store mutant info
                        mutantsInfos.add(curMutantID, sstmt, mutantStmt_list.getTypeName(ms_ind), mutantStmt_list.getIRRelevantPos(ms_ind), &Func, sstmtpos);
                        
                        //construct Basic Block and insert before original
                        llvm::BasicBlock* mutBlock = llvm::BasicBlock::Create(moduleInfo.getContext(), std::string("KS.Mutant_Mut")+mutIDstr, &Func, original);
                        
                        //Add to mutant selection switch
                        sstmtMutants->addCase(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)curMutantID, false)), mutBlock);
                        
                        for (auto mutIRIns: mut_vect)
                        {
                            mutBlock->getInstList().push_back(llvm::dyn_cast<llvm::Instruction>(mutIRIns));
                        }
                        
                        if (! llvm::dyn_cast<llvm::Instruction>(sstmt.back())->isTerminator())    //if we have another stmt after this in this BB
                        {
                            //clone original terminator
                            llvm::Instruction * mutTerm = original->getTerminator()->clone();
                
                            //set name
                            if (original->getTerminator()->hasName())
                                mutTerm->setName((original->getTerminator()->getName()).str()+"_Mut"+mutIDstr);
                            
                            //set as mutant terminator
                            mutBlock->getInstList().push_back(mutTerm);
                        }
                    }
                    
                    /*//delete previous instructions
                    auto rit = sstmt.rbegin();
                    for (; rit!= sstmt.rend(); ++rit)
                    {
                        llvm::dyn_cast<llvm::Instruction>(*rit)->eraseFromParent();
                    }*/
                    
                    //Help name the labels for mutants
                    mod_mutstmtcount++;
                }//~ if(nMuts > 0)
            }
            
            //Get to the right block
            while (&*itBBlock != sstmtCurBB)
            {
                itBBlock ++;
            }
            
        }   //for each BB in Function
        

#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
        if (llvm::verifyFunction (Func, llvm::AbortProcessAction))
#else
        if (llvm::verifyFunction (Func, &llvm::errs()))
#endif
        {
            llvm::errs() << "ERROR: Misformed Function('" << Func.getName() << "') After mutation!\n";//module.dump();
            assert(false); //return false;
        }
         
    }   //for each Function in Module
    
    //@ Set the Initial Value of mutantIDSelectorGlobal to '<Highest Mutant ID> + 1' (which is equivalent to selecting the original program)
    mutantIDSelectorGlobal->setInitializer(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)1+curMutantID, false)));
    
    //module.dump();
    
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    if (llvm::verifyModule (module, llvm::AbortProcessAction))
#else
    if (llvm::verifyModule (module, &llvm::errs()))
#endif
    {
        llvm::errs() << "ERROR: Misformed Module after mutation!\n"; 
        assert(false); //return false;
    }
    
    return true;
}//~Mutation::doMutate

/**
 * \brief obtain the Weak Mutant kill condition and insert it before the instruction @param insertBeforeInst and return the result of the comparison 'original' != 'mutant'
 */
llvm::Value * Mutation::getWMCondition (llvm::BasicBlock *orig, llvm::BasicBlock *mut, llvm::Instruction * insertBeforeInst)
{
    // Look for difference
    
    //TODO: Put the real condition here (original != Mutant)
    //return llvm::ConstantInt::getTrue(moduleInfo.getContext());
    return llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(8, 1, true)); 
}

/**
 * \brief transform non optimized meta-mutant module into weak mutation module. 
 * @param cmodule is the meta mutant module. @note: it will be transformed into WM module, so clone module before this call
 */
void Mutation::computeWeakMutation(std::unique_ptr<llvm::Module> &cmodule, std::unique_ptr<llvm::Module> &modWMLog)
{
    /// Link cmodule with the corresponding driver module (actually only need c module)
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    llvm::Linker linker(cmodule.get());
    std::string ErrorMsg;
    if (linker.linkInModule(modWMLog.get(), &ErrorMsg))
    {
        llvm::errs() << "Failed to link weak mutation module with log function module: " << ErrorMsg << "\n";
        assert (false);
    }
    modWMLog.reset(nullptr);
#else
    llvm::Linker linker(*cmodule);
    if (linker.linkInModule (std::move(modWMLog)))
    {
        assert (false && "Failed to link weak mutation module with log function module");
    }
#endif
    
    llvm::Function *funcWMLog = cmodule->getFunction (wmLogFuncName);
    assert (funcWMLog && "Weak Mutation Log Function absent in WM Module. Was it liked properly?");
    for (auto &Func: *cmodule)
    {
        if (&Func == funcWMLog)
            continue;
        for (auto &BB: Func)
        {
            std::vector<llvm::BasicBlock *> toBeRemovedBB;
            for (llvm::BasicBlock::iterator Iit = BB.begin(), Iie = BB.end(); Iit != Iie;)
            {
                llvm::Instruction &Inst = *Iit++;   //we increment here so that 'eraseFromParent' bellow do not cause crash
                if (auto *callI = llvm::dyn_cast<llvm::CallInst>(&Inst))
                {
                    if (forKLEESEMu && callI->getCalledFunction() == cmodule->getFunction(mutantIDSelectorName_Func))
                    {   //Weak mutation is not seen by KLEE, no need to keep KLEE's function...
                        callI->eraseFromParent();
                    }
                }
                if (auto *sw = llvm::dyn_cast<llvm::SwitchInst>(&Inst))
                {
                    if (auto *ld = llvm::dyn_cast<llvm::LoadInst>(sw->getCondition()))
                    {
                        if (ld->getOperand(0) == cmodule->getNamedGlobal(mutantIDSelectorName))
                        {
                            std::vector<llvm::ConstantInt *> cases;
                            auto *defaultBB = sw->getDefaultDest ();    //original
                            for (llvm::SwitchInst::CaseIt i = sw->case_begin(), e = sw->case_end(); i != e; ++i) 
                            {
                                auto *mutIDConstInt = i.getCaseValue();
                                cases.push_back(mutIDConstInt);     //to be removed later
                                
                                /// Now create the call to weak mutation log func.
                                auto *caseiBB = i.getCaseSuccessor();   //mutant
                                
                                toBeRemovedBB.push_back(caseiBB);
                                
                                llvm::Value *condVal = getWMCondition (defaultBB, caseiBB, sw);
                                llvm::IRBuilder<> sbuilder(sw);
                                std::vector<llvm::Value*> argsv;
                                argsv.push_back(mutIDConstInt);     //mutant ID
                                argsv.push_back(condVal);           //weak kill condition
                                sbuilder.CreateCall(funcWMLog, argsv);  //call WM log func
                            }
                            for (auto *caseval: cases)
                            {
                                llvm::SwitchInst::CaseIt cit = sw->findCaseValue(caseval);
                                sw->removeCase (cit);
                            }
                        }
                    }
                }
            }
            for (auto *bbrm: toBeRemovedBB)
                bbrm->eraseFromParent();
        }
    }
    if (forKLEESEMu)
    {
        llvm::Function *funcForKS = cmodule->getFunction(mutantIDSelectorName_Func);
        funcForKS->eraseFromParent();
    }
    llvm::GlobalVariable *mutantIDSelGlob = cmodule->getNamedGlobal(mutantIDSelectorName);
    //mutantIDSelGlob->setInitializer(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)0, false)));    //Not needed because there is no case anyway, only default
    mutantIDSelGlob->setConstant(true); // only original...
    
    //verify WM module
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    if (llvm::verifyModule (*cmodule, llvm::AbortProcessAction))
#else
    if (llvm::verifyModule (*cmodule, &llvm::errs()))
#endif
    {
        llvm::errs() << "ERROR: Misformed WM Module!\n"; 
        assert(false); 
    }
}//~Mutation::computeWeakMutation

void Mutation::doTCE (std::unique_ptr<llvm::Module> &modWMLog, bool writeMuts)
{
    assert (currentMetaMutantModule && "Running TCE before mutation");
    llvm::Module &module = *currentMetaMutantModule;    
    
    llvm::GlobalVariable *mutantIDSelGlob = module.getNamedGlobal(mutantIDSelectorName);
    assert (mutantIDSelGlob && "Unmutated module passed to TCE");
    
    unsigned highestMutID = getHighestMutantID (module);
    
    TCE tce;
    
    std::map<unsigned, std::vector<unsigned>> duplicateMap;
    std::vector<llvm::Module *> mutModules;
    for (unsigned id=0; id <= highestMutID; id++)       //id==0 is the original
    {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
        llvm::Module *clonedM = llvm::CloneModule(&module);
#else
        llvm::Module *clonedM = llvm::CloneModule(&module).release();
#endif
        mutModules.push_back(clonedM);  
        mutantIDSelGlob = clonedM->getNamedGlobal(mutantIDSelectorName);
        mutantIDSelGlob->setInitializer(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)id, false)));
        mutantIDSelGlob->setConstant(true);
        tce.optimize(*clonedM);
        bool hasEq = false;
        for (auto &M: duplicateMap)
        {
            if (! tce.moduleDiff(mutModules.at(M.first), clonedM))
            {
                hasEq = true;
                duplicateMap.at(M.first).push_back(id);
                break;
            }
        }
        if (! hasEq)
            duplicateMap[id];   //insert id into the map
    }
    
    // Store some statistics about the mutants
    preTCENumMuts = highestMutID;
    postTCENumMuts = duplicateMap.size() - 1;       // -1 because the original is also in
    //
    
    /*for (auto &p :duplicateMap)
    {
        llvm::errs() << p.first << " <--> {"; 
        for (auto eq: p.second)
            llvm::errs() << eq << " ";
        llvm::errs() << "}\n";
    }*/
    
    //re-assign the ids of mutants
    duplicateMap.erase(0);      //Remove original
    unsigned newmutIDs = 1;
    
    //The keys of duplicateMap are (must be) sorted in increasing order: helpful when enabled 'forKLEESEMu'
    for (auto &mm :duplicateMap)    
    {
        mm.second.clear();
        mm.second.push_back(newmutIDs++);
    }
    
    // update mutants infos
    mutantsInfos.postTCEUpdate(duplicateMap);
    
    for (auto &Func: module)
    {
        for (auto &BB: Func)
        {
            std::vector<llvm::BasicBlock *> toBeRemovedBB;
            for (llvm::BasicBlock::iterator Iit = BB.begin(), Iie = BB.end(); Iit != Iie;)
            {
                llvm::Instruction &Inst = *Iit++;   //we increment here so that 'eraseFromParent' bellow do not cause crash
                if (auto *callI = llvm::dyn_cast<llvm::CallInst>(&Inst))
                {
                    if (forKLEESEMu && callI->getCalledFunction() == module.getFunction(mutantIDSelectorName_Func))
                    {
                        uint64_t fromMID = llvm::dyn_cast<llvm::ConstantInt>(callI->getArgOperand(0))->getZExtValue();
                        uint64_t toMID = llvm::dyn_cast<llvm::ConstantInt>(callI->getArgOperand(1))->getZExtValue();
                        unsigned newFromi = 0, newToi = 0;
                        for (auto i=fromMID; i <= toMID; i++)
                        {
                            if (duplicateMap.count(i) != 0)
                            {
                                newToi = i;
                                if (newFromi == 0)
                                    newFromi = i;
                            }
                        }
                        if (newToi == 0)
                        {
                            callI->eraseFromParent();
                        }
                        else
                        {   
                            callI->setArgOperand (0, llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)(duplicateMap.at(newFromi).front()), false)));
                            callI->setArgOperand (1, llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)(duplicateMap.at(newToi).front()), false)));
                        }
                    }
                }
                if (auto *sw = llvm::dyn_cast<llvm::SwitchInst>(&Inst))
                {
                    if (auto *ld = llvm::dyn_cast<llvm::LoadInst>(sw->getCondition()))
                    {
                        if (ld->getOperand(0) == module.getNamedGlobal(mutantIDSelectorName))
                        {
                            uint64_t fromMID=highestMutID;
                            uint64_t toMID=0; 
                            for (llvm::SwitchInst::CaseIt i = sw->case_begin(), e = sw->case_end(); i != e; ++i) 
                            {
                                uint64_t curcase = i.getCaseValue()->getZExtValue();
                                if (curcase > toMID)
                                    toMID = curcase;
                                if (curcase < fromMID)
                                    fromMID = curcase;
                            }
                            for (unsigned i = fromMID; i <= toMID; i++)
                            {
                                llvm::SwitchInst::CaseIt cit = sw->findCaseValue(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)(i), false)));
                                if (duplicateMap.count(i) == 0)
                                {
                                    toBeRemovedBB.push_back(cit.getCaseSuccessor());
                                    sw->removeCase (cit);
                                }
                                else
                                {
                                    cit.setValue(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)(duplicateMap.at(i).front()), false)));
                                }
                            }
                        }
                    }
                }
            }
            for (auto *bbrm: toBeRemovedBB)
                bbrm->eraseFromParent();
        }
    }
    
    //@ Set the Initial Value of mutantIDSelectorGlobal to '<Highest Mutant ID> + 1' (which is equivalent to original)
    highestMutID = duplicateMap.size();     //here 'duplicateMap' contain only remaining mutant (excluding original)
    mutantIDSelGlob = module.getNamedGlobal(mutantIDSelectorName);
    
    if (highestMutID == 0)
    {       //No mutants
        mutantIDSelGlob->eraseFromParent();
    }
    else
    {
        mutantIDSelGlob->setInitializer(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)1+highestMutID, false)));
    }
    
    // Write mutants files and weak mutation file
    if (writeMuts || modWMLog)
    {
        std::unique_ptr<llvm::Module> wmModule(nullptr);
        if (modWMLog)
        {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            wmModule.reset (llvm::CloneModule(&module));
#else
            wmModule = llvm::CloneModule(&module);
#endif
            computeWeakMutation(wmModule, modWMLog);
        }
        assert (writeMutantsCallback((writeMuts ? &duplicateMap : nullptr), (writeMuts ? &mutModules : nullptr), wmModule.get()) && "Failed to dump mutants IRs");
    }
    
    //create the final version of the meta-mutant file
    if (forKLEESEMu)
    {
        llvm::Function *funcForKS = module.getFunction(mutantIDSelectorName_Func);
        funcForKS->deleteBody();
        //tce.optimize(module);
        
        if (highestMutID > 0)   //if There are mutants
            createGlobalMutIDSelector_Func(module, true);
        
        ///reduce consecutive range (of selector func) into one //TODO
    }
    else
    {
        //tce.optimize(module);
    }
    
    //verify post TCE Meta-module
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    if (llvm::verifyModule (module, llvm::AbortProcessAction))
#else
    if (llvm::verifyModule (module, &llvm::errs()))
#endif
    {
        llvm::errs() << "ERROR: Misformed post-TCE Meta-Module!\n"; 
        assert(false); //return false;
    }
    
    // free mutModules' cloned modules.
    for (auto *mm: mutModules)
        delete mm;
}

bool Mutation::getMutant (llvm::Module &module, unsigned mutantID)
{
    unsigned highestMutID = getHighestMutantID (module);
    if (mutantID > highestMutID)
        return false;
    
    llvm::GlobalVariable *mutantIDSelGlob = module.getNamedGlobal(mutantIDSelectorName);    
    TCE tce;
    
    mutantIDSelGlob->setInitializer(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)mutantID, false)));
    mutantIDSelGlob->setConstant(true);
    tce.optimize(module);
    
    return true;
}
unsigned Mutation::getHighestMutantID (llvm::Module &module)
{
    llvm::GlobalVariable *mutantIDSelectorGlobal = module.getNamedGlobal(mutantIDSelectorName);
    assert (mutantIDSelectorGlobal && mutantIDSelectorGlobal->getInitializer()->getType()->isIntegerTy() && "Unmutated module passed to TCE");
    return llvm::dyn_cast<llvm::ConstantInt>(mutantIDSelectorGlobal->getInitializer())->getZExtValue() - 1;
}

void Mutation::loadMutantInfos (std::string filename)
{
    mutantsInfos.loadFromJsonFile(filename);
}

void Mutation::dumpMutantInfos (std::string filename)
{
    mutantsInfos.printToJsonFile(filename);
}

Mutation::~Mutation ()
{
    mutantsInfos.printToStdout();
    
    llvm::errs() << "\nNumber of Mutants:   PreTCE: " << preTCENumMuts << ", PostTCE: " << postTCENumMuts << "\n";
    
    //Clear the constant map to avoid double free
    llvmMutationOp::destroyPosConstValueMap();
}


