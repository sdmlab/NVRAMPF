
#include "sdmllvm.hpp"
#include <fstream>
#include <iostream>
using namespace llvm ;

osFlush go;
osMessage message; osMessage debug("logfiles") ;  osMessage debug2;
osMessage::osMessage() {prefix = "" ; show = true ; usefile = false;}
osMessage::osMessage(std::string logfile)
{
    usefile = true; logF = logfile; prefix = "" ; show = true ;
    std::ofstream myfile (logF,  std::ios::trunc  );
    myfile << "" << "\n";
    myfile.close();
}
std::ostream& operator <<(std::ostream &os,  const    osFlush &go_ )
{
    osMessage& mos = static_cast<osMessage& >(os  );
    if (  mos.show)
    {
        if(mos.usefile )
        {
            std::ofstream myfile (mos.logF, std::ios::out | std::ios::app | std::ios::ate );
            myfile << mos.prefix << mos.str() << "\n";
            myfile.close();
        }
        else
        {
            printf( "%s%s\n" , mos.prefix.c_str() , mos.str().c_str() ) ;
        }
    }
    mos.str("") ;
    return os ;
};


namespace sdmllvm
{
    std::stringstream oss;
    std::string OSSs;
    llvm::raw_string_ostream OSS( OSSs);
    #define print__(v) {OSSs.clear() ; v->print( OSS) ; return OSS.str() ;} ;
    std::string printllvmvalue(llvm::Value * v  ){  print__(v); }
    std::string printllvmvalue(llvm::Type   * t  ){  print__(t); }
    
    
    void Save_Output ( llvm::Module * Mod  , std::string filename  )
    {
        std::string ErrorInfo;
        tool_output_file Out( filename .c_str(), ErrorInfo, sys::fs::F_None);
        if (!ErrorInfo.empty())
        {
            errs() << ErrorInfo << '\n';
        }
        WriteBitcodeToFile(Mod, Out.os());
        Out.keep();
    }
    
    
    // module class definition
    module::module()
    {
        inst_added_count = 0;
    }
    
    const char * module::getnewinstname()
    {
        oss.str("") ;
        oss << "__sdm__" << (++inst_added_count) ;
        return std::string( oss.str()).c_str();
    }
    const char * module::getnewinstname(const char * prefix)
    {
        oss.str("") ;
        oss << prefix << (++inst_added_count) ;
        return   oss.str().c_str();
    }
    
    std::string module::print(){OSSs.clear() ; base->print( OSS,0) ; return OSS.str() ; }

    int module::LoadApp(std::string InputFilename ,std::string profile_function     )
    {
        std::string ErrorMessage;
        bool listMemoryAccessInst = true ;
        Module::iterator FI ,FE;
        
        // get profile function file info
        std::unique_ptr<llvm::Module> pMod ;
        DataStreamer *pf_streamer = llvm::getDataFileStreamer( profile_function , &ErrorMessage);
        if (pf_streamer)
        {
        
            std::string DisplayFilename;
            pMod.reset(getStreamedBitcodeModule(DisplayFilename, pf_streamer, llvm::getGlobalContext(), &ErrorMessage));
            if(pMod.get())
            {
                if (std::error_code EC = pMod->materializeAllPermanently())
                {
                    ErrorMessage = EC.message();
                    pMod.reset();
                }
            }
        }
        if (!pMod.get())
        {
            if (ErrorMessage.size())
            {
                debug  << ErrorMessage << "\n";
            }
            else
            {
                debug  << "profile functions didn't read correctly.\n";
            }
            return 0;
        }
        
        // get list of global variable in profile function file
        for (llvm::Module::global_iterator  I = pMod.get()->global_begin(), E = pMod->global_end();I != E; ++I)
        {
            // ensure all global variables have 'sdmprofile' prefix
            if (I ->getName().find ("sdmprofile") == std::string::npos  )
            {
                if(I->getName() != "stderr")
                {
                I->setName((std::string (  "sdmprofile__")  + I->getName().str()   ) );
                }else { debug << "stderr:::" <<  (new sdmllvm::globalvariable (I))->print() << go ;  }
                
            }
            sdmllvm::globalvariable * GV = new sdmllvm::globalvariable (I);
            
            pf_globalvariables[GV->Name() ] = GV ;
        }
        
        // get list of functions in profile function file
        for (FI = pMod.get()->begin(), FE = pMod.get()->end(); FI != FE; ++FI)
        {
            sdmllvm::function * FF = new sdmllvm::function(FI,this);
            pf_functions [FF->Name() ] = FF;
        }
        
        // save altered profile function file to temporary file
        std::string pftempfile = std::string ( tmpnam(NULL)) + "pf.bc" ;
        Save_Output( pMod.get() ,pftempfile );

        // Link application with profile functions
        debug  << "Link application with profile functions : " << profile_function << go ;
        std::string temp_app = std::string ( tmpnam(NULL)) + ".bc"    ;
        debug << "Temporary Application file " << temp_app << go ;
        std::stringstream linkcommand ;
        linkcommand << "llvm-link " << pftempfile << " " << InputFilename << " -o " << temp_app;
        debug  << "Link command : " <<  linkcommand.str() << go ;
        int call_rtn = std::system(linkcommand.str().c_str());
        
        DataStreamer *streamer = llvm::getDataFileStreamer(temp_app, &ErrorMessage);
        if (streamer)
        {
            std::string DisplayFilename;
            Mod.reset(getStreamedBitcodeModule(DisplayFilename, streamer, Context, &ErrorMessage));
            
            if(Mod.get())
            {
                if (std::error_code EC = Mod->materializeAllPermanently())
                {
                    ErrorMessage = EC.message();
                    Mod.reset();
                }
            }
        }
        if (!Mod.get())
        {
            if (ErrorMessage.size())
            {
                debug  << ErrorMessage << "\n";
            }
            else
            {
                debug  << "bitcode didn't read correctly.\n";
            }
            return 0;
        }
        TD = new llvm::DataLayout( Mod.get() ) ;
        debug << "App Target Triple : " << Mod.get()->getTargetTriple () <<  go ;
        debug << "App Data Layout : "  << TD->getStringRepresentation () << go ;
        
        // get all Global variables
        long indexGV = 0;
        for (llvm::Module::global_iterator  I = Mod.get()->global_begin(), E = Mod->global_end();I != E; ++I)
        {
            sdmllvm::globalvariable * GV = new sdmllvm::globalvariable (I);
            
            if (I->getName() == "llvm.global_ctors"){ continue ; }
            
            // exclude if this is a profile function global variable
            if (!(pf_globalvariables[GV->Name()] ))
            {
                gvmap[I] = GV ;
                GV->index = ++indexGV;
                debug << "App global variable : " << GV->Name() << go ;
            }
            else
            {
                pf_globalvariables[GV->Name()] = GV ;
            }
        }
        
        // get all App functions
        long mccc = 0 ;
        u_int32_t fcc = 0 ;
        for (FI = Mod.get()->begin(), FE = Mod.get()->end(); FI != FE; ++FI)
        {
            sdmllvm::function * FF = new sdmllvm::function(FI,this);
            
            // exclude if this is a profile function file function
            if(!(pf_functions[FF->Name()]))
            {
                functionmap[FI] = FF ;
                FF->index = ++fcc;
                debug << "App function : " << FF->index  << "\t" << FF->Name() << go;
            }
            else
            {
                pf_functions[FF->Name()] = FF ;
            }
        
            for (Function::iterator BI = FF->base->begin(), BE = FF->base->end();      BI != BE; ++BI)
            {
                sdmllvm::basicblock * BB = new sdmllvm::basicblock (BI );
                BB->parentfunction = FF ;
                FF->bbmap[BI] = BB ;
                if(!listMemoryAccessInst ) { continue ;}
                for (BasicBlock::iterator II = BB->base->begin(), IE = BB->base->end(); II != IE; ++II)
                {
                    if (sdmllvm::MemoryAccessInst * meminst = MemoryAccessInst::IsMemAccess(II) )
                    {
                        BB->meminstmap[II] = meminst ;
                        ++mccc;
                    }
                }
                
            }
            
        }
        debug << "memory access instructions count : " << mccc << go;
        
        return 1 ;
    }
    
    
    // globalvariable class definition
    globalvariable::globalvariable( llvm::GlobalVariable * gv)
    {
        base = gv;
        typestring = sdmllvm::printllvmvalue(  (base->getType()->getElementType())) ;
    }
    std::string  globalvariable::print() {print__ (base); };
    
    // function class definition
    std::string function::print() {print__ (base); };
    function ::function (llvm::Function* F , sdmllvm::module * Mod_)
    {
        base = F ;
        Mod = Mod_ ;
    }
    
    // basicblock class definition
    static long bbindexcount = 0;
    std::string basicblock ::print() {print__ (base); };
    basicblock::basicblock(llvm::BasicBlock * B)
    {
        base = B ;
        index = ++bbindexcount ;
    }
    
    // instruction class definition
    std::string instruction::print() {print__ (base); };
 
    // MemoryAccessInst class definition
    MemoryAccessInst * MemoryAccessInst:: IsMemAccess( llvm::Instruction * Inst)
    {
        if (  llvm::StoreInst *I = llvm::dyn_cast<llvm::StoreInst>( Inst))
        {
            return new MemoryAccessInst(Inst , StoreINST  ,  I , 0);
        }
        
        if (  llvm::LoadInst *I = llvm::dyn_cast<llvm::LoadInst >( Inst))
        {
            return new MemoryAccessInst(Inst , LoadINST ,0 ,I  );
        }
        return  0 ;
    }
    
    MemoryAccessInst:: MemoryAccessInst(llvm::Instruction * Inst , int AccessType_ ,llvm::StoreInst * s_ ,llvm::LoadInst * l_ )
    {
        base = Inst ;
        AccessType = AccessType_;
        s = s_ ;
        l = l_;
    }
    
    llvm::Value* MemoryAccessInst::getPointerOperand()
    {
        
        if (AccessType == StoreINST)
        {
            return s->getPointerOperand();
        }
        else if ( AccessType == LoadINST)
        {
            return l->getPointerOperand();
        }
        return 0;
    }
    
}

namespace sdmllvm
{
    // Other functions
    llvm::CallInst * CreateCallInst(sdmllvm::module * M,llvm::Instruction * Insertpoint,bool insertafter , sdmllvm::function * functiontocall, ARGV arg_)
    {
        llvm::Instruction * insertP = Insertpoint ;
        SmallVector<Value*, 20> arguments  ;
        llvm::IRBuilder<> B_builder(Insertpoint->getParent());
        B_builder.SetInsertPoint(insertP );
        bool noarg = false ;
        
        ARGI it;
        int cccc = 0;
        for (it=arg_.begin(); it < arg_.end(); it++)
        {
            ARG a = *it ;
            switch (a.type)
            {
                case -1:
                {
                    noarg = true;
                }
                    break ;
                case 1:
                {
                    llvm::Value * v = B_builder.Insert(  ((llvm::Constant *)ConstantInt::get( Int32_t, a.dataInt)), M->getnewinstname());
                    if(insertafter)
                    {
                       if (llvm::Instruction *instv = llvm::dyn_cast<llvm::Instruction >( v))
                       {
                           insertP->moveBefore( instv );
                           insertP= instv ;
                           B_builder.SetInsertPoint(insertP );
                       }
                    }
                    arguments.push_back(v);
                }
                    break;
                case 2:
                {
                    arguments.push_back(a.dataValue);
                }
                    break;
                case 3:
                {
                    arguments.push_back(a.dataInst);
                }
                    break ;

                case 4:
                {
                    arguments.push_back(a.datainst->base );
                }
                    break ;

                case ARGpti :
                {
                    llvm::Value * v = B_builder.CreatePtrToInt(a.dataValue, Int64_t, M->getnewinstname ("ptr_") );
                    if(insertafter)
                    {
                        if (llvm::Instruction * instv = llvm::dyn_cast<llvm::Instruction >(v))
                        {
                            insertP->moveBefore( instv );
                            insertP = instv ;
                            B_builder.SetInsertPoint(insertP );
                        }
                    }
                    arguments.push_back(v);
                }
                    break ;

                case 6:
                {
                    Value * v =  B_builder.CreateGlobalStringPtr ( a.SL  ,a.SLname   );
                    arguments.push_back(v);
                }
                    break ;

                case ARGI8:
                {
                    llvm::Value * v = B_builder.Insert(  ((llvm::Constant *)  ConstantInt::get( Int8_t, a.dataInt)) , M->getnewinstname());
                    if(insertafter)
                    {
                        if (llvm::Instruction *instv = llvm::dyn_cast<llvm::Instruction >( v))
                        {
                            insertP->moveBefore( instv );
                            insertP = instv ;
                            B_builder.SetInsertPoint(insertP );
                        }
                    }
                    arguments.push_back(v);
                }
                    break ;
                case ARGI64:
                {
                    llvm::Value * v = B_builder.Insert(  ((llvm::Constant *)  ConstantInt::get( Int64_t, a.dataInt)) , M->getnewinstname());
                    if(insertafter)
                    {
                        if (llvm::Instruction *instv = llvm::dyn_cast<llvm::Instruction >( v))
                        {
                            insertP->moveBefore( instv );
                            insertP = instv  ;
                            B_builder.SetInsertPoint(insertP );
                        }
                    }
                    arguments.push_back(v);
                }
                    break;
                default:
                    break;
            }
        }
        
        llvm::ArrayRef<llvm::Value *>  argsRef(arguments );
        llvm::CallInst * fcall ;
        if(noarg)
        {
            fcall = B_builder.CreateCall(functiontocall->base  );
        }
        else
        {
            fcall = B_builder.CreateCall(  functiontocall->base  , argsRef  );
        }
        if(insertafter){ insertP ->moveBefore( fcall );}
        
        return fcall;
    }
    
    llvm::CallInst * WrapAfunction_head(sdmllvm::module * M,sdmllvm::function * FF , sdmllvm::function * head, ARGV arg_ )
    {
        // get first instructtion in function
        llvm::BasicBlock *  EntryB = const_cast<BasicBlock *>(&FF->base->getEntryBlock());
        llvm::Instruction * firstInst = EntryB->getFirstNonPHIOrDbg();
        llvm::CallInst * fcall = CreateCallInst(M,firstInst,false  , head , arg_ );
        return fcall;
    }
    std::vector< llvm::CallInst * > * WrapAfunction_tail  (sdmllvm::module * M,sdmllvm::function * FF , sdmllvm::function * tail, ARGV arg_ )
    {
        std::vector< llvm::CallInst * > * rtnlist = new std::vector< llvm::CallInst * > () ;
        // collect all return statement in the function
        std::vector<llvm::ReturnInst * > returns_insts  ;
        for (Function::iterator  BI = FF->base->begin(), BE = FF->base->end();      BI != BE; ++BI)
        {
            for (BasicBlock::iterator II = BI->begin(), IE = BI->end();      II != IE; ++II)
            {
                if (llvm::ReturnInst *Irtn = dyn_cast<llvm::ReturnInst>(II)) {returns_insts.push_back(Irtn);}
            }
        }
    
        // place a function call instruction to the 'tail' function
        for ( std::vector<llvm::ReturnInst *>::iterator Irtn = returns_insts.begin(); Irtn != returns_insts.end() ;++ Irtn )
        {
            llvm::CallInst * Inst_rtn = CreateCallInst(M, *Irtn, false, tail , arg_ );
            rtnlist->push_back(Inst_rtn);
        }
        return rtnlist;
    }

    int Insert_log_default( sdmllvm::module * M , sdmllvm::function * logf)
    {
        int cc = 0;
        for (sdmllvm::module::functionmapI  FI  = M->functionmap.begin(), FE = M->functionmap.end(); FI != FE; ++FI)
        {
            sdmllvm::function * FF = FI->second ;
            for (sdmllvm::function::bbmapI BI = FF->bbmap.begin(), BE = FF->bbmap.end();BI != BE; ++BI)
            {
                sdmllvm::basicblock * BB = BI->second ;
                for (sdmllvm::basicblock::meminstmapI II = BB->meminstmap.begin(), IE = BB->meminstmap.end(); II != IE; ++II)
                {
                    sdmllvm::MemoryAccessInst * mem = II->second ;
                    ARGV args {ARG( mem->getPointerOperand(),ARGpti),ARG(mem->AccessType,ARGI8 ) } ;
                    CreateCallInst(M,mem->base , false ,logf ,args ) ;
                    cc++;
                }
            }
        }
        return cc;
    }
    
    std::vector<llvm::CallInst *> * Collect_FcallInst(sdmllvm::module * M,llvm::Function *F)
    {
        std::vector<llvm::CallInst *> * list = new std::vector< llvm::CallInst *> ();
        for (sdmllvm::module::functionmapI  FI  = M->functionmap.begin(), FE = M->functionmap.end(); FI != FE; ++FI)
        {
            sdmllvm::function * FF = FI->second ;
            for (sdmllvm::function::bbmapI BI = FF->bbmap.begin(), BE = FF->bbmap.end();BI != BE; ++BI)
            {
                sdmllvm::basicblock * BB = BI->second ;
                for (BasicBlock::iterator II = BB->base->begin(), IE = BB->base->end(); II != IE; ++II)
                {
                    if (  llvm::CallInst *fcall = llvm::dyn_cast<llvm::CallInst >( II))
                    {
                        if (F == fcall->getCalledFunction	() )
                        {
                            list->push_back(fcall);
                        }
                    }
                }
            }
        }
        
        return list ;
        
        
    }
    
    std::vector<llvm::Instruction *> * Collect_Inst(sdmllvm::module * M, InstType Ty)
    {
        std::vector<llvm::Instruction *> * list = new std::vector< llvm::Instruction *> ();
        for (sdmllvm::module::functionmapI  FI  = M->functionmap.begin(), FE = M->functionmap.end(); FI != FE; ++FI)
        {
            sdmllvm::function * FF = FI->second ;
            for (sdmllvm::function::bbmapI BI = FF->bbmap.begin(), BE = FF->bbmap.end();BI != BE; ++BI)
            {
                sdmllvm::basicblock * BB = BI->second ;
                for (BasicBlock::iterator II = BB->base->begin(), IE = BB->base->end(); II != IE; ++II)
                {
                    switch (Ty)
                    {
                        case InstType::StackAllocationInst :
                        {
                            if (llvm::AllocaInst *inst = llvm::dyn_cast<llvm::AllocaInst >( II))
                            {
                                list->push_back(inst);
                            }
                        }
                            break;
                        default:
                            break;
                    }
                   
                }
            }
        }
        
        return list ;
    }
    

    
}





