

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "profile_simple.hpp"
#include "readresult.h"



using namespace sdmllvm ;

namespace profile_simple
{
    
    void  Save_Output   (sdmllvm::module *M , std::string InputFilename,std::string OutputFilename,bool Convert_object_file)
    {
        std::string O_bc  ;std::string O_o;
        if ( OutputFilename == "")
        {
            O_bc = InputFilename + std::string ("_profile.bc");
            O_o  = InputFilename + std::string ("_profile.o");
        }
        else
        {
            if (Convert_object_file)
            {
                O_bc  = OutputFilename + ".bc";
                O_o = OutputFilename ;
            }else
            {
                O_bc  = OutputFilename ;
                
            }
        }
        
        
        {
            std::string ErrorInfo;
            
            tool_output_file Out( O_bc .c_str(), ErrorInfo, sys::fs::F_None);
            if (!ErrorInfo.empty())
            {
                errs() << ErrorInfo << '\n';
            }
            
            WriteBitcodeToFile(M->Mod.get(), Out.os());
            Out.keep();
            
        }
        
        
        if (Convert_object_file)
        {
            std::string call ( std::string ("llc -filetype=obj -o ") + O_o + std::string (" ")  + O_bc);
            int call_rtn = std::system(call.c_str());
            
            message  <<  "Output Object File = " <<  O_o   << go ;
        }else
        {
            message  <<  "Output Bitcode File = " <<  O_bc  << go ;
        }
        
        
    }
    
    llvm::Instruction * PlaceInitEnd(sdmllvm::module * M, u_int32_t profileType, u_int64_t heapallocationCount, u_int64_t stackallocationCount ,u_int64_t GVCount )
    {
        debug << "Place pf init and end functions call in main function" << go ;
        function * mainf  = M->functionmap[M->Mod->getFunction ("main" )] ;
        sdmllvm::function * finit  = M->pf_functions["sdmprofile__Initialize_proling"];
        sdmllvm::function * fsave  = M->pf_functions["sdmprofile__End_profiling"];
        llvm::CallInst * InitcallInst = sdmllvm::WrapAfunction_head(M,mainf,finit , ARGV {ARG(profileType),ARG(M->functionmap.size(),ARGI64),ARG(heapallocationCount,ARGI64),ARG(stackallocationCount,ARGI64), ARG(GVCount,ARGI64),ARG(mainf->index) }  );
        std::vector< llvm::CallInst * >  * EndcallInstlist = sdmllvm::WrapAfunction_tail(M,mainf,fsave , ARGV {ARG( profileType)});
        
        llvm::Instruction  * LastPoint = InitcallInst ;
        sdmllvm::function * regf  = M->pf_functions["sdmprofile__Store_FunctionNames"];
        int cc = 0;
        for (sdmllvm::module::functionmapI I = M->functionmap.begin() ; I != M->functionmap.end();++I)
        {
            sdmllvm::function * f  = I->second ;
            std::stringstream  variablename ; variablename << "sdmprofile__F_Name_" << f->index;
            ARGV args ;
            args.push_back (ARG(f->index));
            args.push_back (ARG(f->Name(),variablename.str() ,ARGSL ));
            llvm::CallInst * fcall = sdmllvm::CreateCallInst(M, LastPoint,true, regf , args );
            LastPoint = fcall ;
        }
        return LastPoint;
    }
    
    int  GV_profiling_only(std::string InputFilename, std::string OutputFilename,int job,bool Convert_object_file)
    {
        int profileType = 3;
        message << go << "\tSDM Profile Tool " << go << "\tOptions :" << go <<  "\t Scope\t\t: Global Variables " << go <<
         "\t Input File \t: " << InputFilename << go << "\t Output File \t: " << OutputFilename << go ;
        
        sdmllvm::module *M = new sdmllvm::module();
        M->LoadApp(InputFilename, "profile_functions.bc"   );
        
        
        llvm::Instruction  * LastPoint = PlaceInitEnd(M , profileType,0,0,M->gvmap.size());
        
        sdmllvm::function * mainf  = M->functionmap[M->Mod->getFunction ("main" )] ;
        sdmllvm::function * regf  = M->pf_functions["sdmprofile__Register_GV"];
        int cc = 0;
        for (sdmllvm::module::gvmapI I = M->gvmap.begin() ; I != M->gvmap.end();++I)
        {
            sdmllvm::globalvariable *GV = I->second ;
            std::string  name_string ("sdmprofile__GV_Name_" + GV->Name());
            std::string  type_string ("sdmprofile__GV_Type_" + GV->Name());
            ARGV args ;
            args.push_back (ARG( ++cc));
            args.push_back (ARG(GV->base,ARGpti ));
            args.push_back (ARG(GV->MemorySize()));
            args.push_back (ARG(GV->Name(), name_string,ARGSL));
            args.push_back (ARG(GV->TypeString(), type_string,ARGSL) );
            llvm::CallInst * fcall = sdmllvm::CreateCallInst(M, LastPoint,true, regf , args );
            LastPoint = fcall ;
        }
        sdmllvm::function *  logfunction  = M->pf_functions["sdmprofile__profile_access3"];
        sdmllvm::Insert_log_default(   M ,   logfunction );
        
        Save_Output(M,InputFilename , OutputFilename , Convert_object_file );
        return  1;
    }
    
    int heap_profiling_only (std::string InputFilename,std::string OutputFilename, int job,bool Convert_object_file)
    {
        int profileType = 1;
        message << go << "\tSDM Profile Tool " << go << "\tOptions :" << go <<  "\t Scope\t\t: Heap Variables " << go <<
        "\t Input File \t: " << InputFilename << go << "\t Output File \t: " << OutputFilename << go ;
        
        sdmllvm::module *M = new sdmllvm::module();
        M->LoadApp(InputFilename, "profile_functions.bc"   );
        
        // collect all malloc and free function call instructions
        llvm::Function * fmalloc = M->Mod->getFunction ("malloc");
        llvm::Function * ffree = M->Mod->getFunction("free") ;
        
        std::vector<llvm::CallInst *> * malloclist = Collect_FcallInst(M,fmalloc);
        std::vector<llvm::CallInst *> * freelist = Collect_FcallInst(M,ffree);
        
        // replace all malloc and free function call instruction with profile malloc/free function
        long Heap_fcall_index = 0 ;
        function * pf_malloc = M->pf_functions["sdmprofile__malloc"];
        function * pf_free = M->pf_functions["sdmprofile__free"];
        globalvariable * Inst_no = M->pf_globalvariables["sdmprofile__Instruction_number"];
        std::vector<llvm::CallInst *>::iterator Ifcall ;
        for (Ifcall = malloclist->begin() ; Ifcall != malloclist->end();++Ifcall)
        {
            llvm::CallInst * fcall = *Ifcall ;
            sdmllvm::function * F = M->functionmap[fcall->getParent()->getParent()];
            ARGV args;
            args.push_back(ARG(fcall->getArgOperand(0)));
            args.push_back(ARG(++Heap_fcall_index  ,ARGI64 ));
            args.push_back(ARG(F->index));
            llvm::CallInst * cc = CreateCallInst(M,fcall,true,  pf_malloc,args) ;
            fcall->replaceAllUsesWith(cc);
            fcall->eraseFromParent();
        }
        for (Ifcall = freelist->begin() ; Ifcall != freelist->end();++Ifcall)
        {
            llvm::CallInst * fcall = *Ifcall ;
            fcall->setCalledFunction(pf_free->base);
        }
        
        llvm::Instruction  * LastPoint = PlaceInitEnd(M , profileType,malloclist->size(),0,0);
        
        sdmllvm::function *  logfunction  = M->pf_functions["sdmprofile__profile_access1"];
        sdmllvm::Insert_log_default(M , logfunction );
        
        Save_Output(M,InputFilename , OutputFilename , Convert_object_file );
        return 1;
    }
    
    int stack_profiling_only(std::string InputFilename, std::string OutputFilename,int job ,bool Convert_object_file )
    {
        int profileType = 2;
        message << go << "\tSDM Profile Tool " << go << "\tOptions :" << go <<  "\t Scope\t\t: Heap Variables " << go <<
        "\t Input File \t: " << InputFilename << go << "\t Output File \t: " << OutputFilename << go ;
        
        sdmllvm::module *M = new sdmllvm::module();
        M->LoadApp(InputFilename, "profile_functions.bc"   );
        
        // collect all stack allocation instructions
        std::vector<llvm::Instruction *> * AllocaList = Collect_Inst(M,sdmllvm::InstType::StackAllocationInst) ;
        
        
        function * Add_stack_allocation  = M->pf_functions["sdmprofile__Add_stack_allocation"];
        function * Reg_Current_executing_f = M->pf_functions["sdmprofile__Register_Current_executing_function"];
        function * Exit_Current_executing_f = M->pf_functions["sdmprofile__Exit_Current_executing_function"];
        function * mainf  = M->functionmap[M->Mod->getFunction ("main" )] ;
        globalvariable * mainF_info = M->pf_globalvariables["sdmprofile__MainFunction"];
        
        std::map <sdmllvm::function * , llvm::Value *> function_info_map ;
        function_info_map[mainf] = mainF_info->base ;
        long Alloc_inst_index = 0;
        
        std::vector<llvm::Instruction *>::iterator II ;
        for (II = AllocaList->begin() ; II != AllocaList->end();++II)
        {
            llvm::Instruction * Inst = *II ;
            llvm::Value * function_info ;
            sdmllvm::function * F = M->functionmap[ Inst->getParent()->getParent()] ;
            
            if (!(function_info_map[F]))
            {
                llvm::CallInst * InitcallInst = sdmllvm::WrapAfunction_head(M,F,Reg_Current_executing_f, ARGV {ARG (F->index)}  );
                sdmllvm::WrapAfunction_tail(M,F,Exit_Current_executing_f , ARGV {ARG(InitcallInst)});
                                function_info_map[F] = InitcallInst ;
            }
            
            function_info = function_info_map[F];
            int MemorySize = M->TD->getTypeAllocSize( ((llvm::AllocaInst *)(Inst))-> getAllocatedType());
            
            ARGV args;
            args.push_back(ARG(Inst,ARGpti));
            args.push_back(ARG(function_info )) ;
            args.push_back(ARG(MemorySize,ARGI64));
            args.push_back(ARG(++Alloc_inst_index,ARGI64 ));
            llvm::CallInst * cc = CreateCallInst(M,Inst,true, Add_stack_allocation,args) ;
        }
        
        llvm::Instruction  * LastPoint = PlaceInitEnd(M , profileType,0,AllocaList->size(),0);
        
        
        
        sdmllvm::function *  logfunction  = M->pf_functions["sdmprofile__profile_access2"];
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
                    sdmllvm::function * F = M->functionmap[ mem->base->getParent()->getParent()] ;
                    llvm::Value * function_info ;
                    function_info = function_info_map[F];
                    ARGV args {ARG( mem->getPointerOperand(),ARGpti),ARG(mem->AccessType,ARGI8 ),ARG(function_info ) } ;
                    llvm::CallInst * fcall = CreateCallInst(M,mem->base , false ,logfunction ,args ) ;
                    cc++;
                }
                
            }
        }
        
        Save_Output(M,InputFilename , OutputFilename , Convert_object_file );
        
        return 1;
    }
    
    int All_profiling(std::string InputFilename, std::string OutputFilename,int job ,bool Convert_object_file )
    {
        int profileType = 4;
        message << go << "\tSDM Profile Tool " << go << "\tOptions :" << go <<  "\t Scope\t\t: All  " << go <<
        "\t Input File \t: " << InputFilename << go << "\t Output File \t: " << OutputFilename << go ;
        
        sdmllvm::module *M = new sdmllvm::module();
        M->LoadApp(InputFilename, "profile_functions.bc"   );
        
        function * mainf  = M->functionmap[M->Mod->getFunction ("main" )] ;
        
        
        // collect all malloc and free function call instructions
        llvm::Function * fmalloc = M->Mod->getFunction ("malloc");
        llvm::Function * ffree = M->Mod->getFunction("free") ;
        std::vector<llvm::CallInst *> * malloclist = Collect_FcallInst(M,fmalloc);
        std::vector<llvm::CallInst *> * freelist = Collect_FcallInst(M,ffree);
        
        // collect all stack allocation instructions
        std::vector<llvm::Instruction *> * AllocaList = Collect_Inst(M,sdmllvm::InstType::StackAllocationInst) ;
        
        
        // replace all malloc and free function call instruction with profile malloc/free function
        long Heap_fcall_index = 0 ;
        function * pf_malloc = M->pf_functions["sdmprofile__malloc"];
        function * pf_free = M->pf_functions["sdmprofile__free"];
        globalvariable * Inst_no = M->pf_globalvariables["sdmprofile__Instruction_number"];
        std::vector<llvm::CallInst *>::iterator Ifcall ;
        for (Ifcall = malloclist->begin() ; Ifcall != malloclist->end();++Ifcall)
        {
            llvm::CallInst * fcall = *Ifcall ;
            sdmllvm::function * F = M->functionmap[fcall->getParent()->getParent()];
            ARGV args;
            args.push_back(ARG(fcall->getArgOperand(0)));
            args.push_back(ARG(++Heap_fcall_index  ,ARGI64 ));
            args.push_back(ARG(F->index));
            llvm::CallInst * cc = CreateCallInst(M,fcall,true,  pf_malloc,args) ;
            fcall->replaceAllUsesWith(cc);
            fcall->eraseFromParent();
        }
        for (Ifcall = freelist->begin() ; Ifcall != freelist->end();++Ifcall)
        {
            llvm::CallInst * fcall = *Ifcall ;
            fcall->setCalledFunction(pf_free->base);
        }
        
        
        function * Add_stack_allocation  = M->pf_functions["sdmprofile__Add_stack_allocation"];
        function * Reg_Current_executing_f = M->pf_functions["sdmprofile__Register_Current_executing_function"];
        function * Exit_Current_executing_f = M->pf_functions["sdmprofile__Exit_Current_executing_function"];
        globalvariable * mainF_info = M->pf_globalvariables["sdmprofile__MainFunction"];
        
        std::map <sdmllvm::function * , llvm::Value *> function_info_map ;
        function_info_map[mainf] = mainF_info->base ;
        long Alloc_inst_index = 0;
        
        std::vector<llvm::Instruction *>::iterator II ;
        for (II = AllocaList->begin() ; II != AllocaList->end();++II)
        {
            llvm::Instruction * Inst = *II ;
            llvm::Value * function_info ;
            sdmllvm::function * F = M->functionmap[ Inst->getParent()->getParent()] ;
            
            if (!(function_info_map[F]))
            {
                llvm::CallInst * InitcallInst = sdmllvm::WrapAfunction_head(M,F,Reg_Current_executing_f, ARGV {ARG (F->index)}  );
                sdmllvm::WrapAfunction_tail(M,F,Exit_Current_executing_f , ARGV {ARG(InitcallInst)});
                function_info_map[F] = InitcallInst ;
            }
            
            function_info = function_info_map[F];
            int MemorySize = M->TD->getTypeAllocSize( ((llvm::AllocaInst *)(Inst))-> getAllocatedType());
            
            ARGV args;
            args.push_back(ARG(Inst,ARGpti));
            args.push_back(ARG(function_info )) ;
            args.push_back(ARG(MemorySize,ARGI64));
            args.push_back(ARG(++Alloc_inst_index,ARGI64 ));
            llvm::CallInst * cc = CreateCallInst(M,Inst,true, Add_stack_allocation,args) ;
        }
        
        
        llvm::Instruction  * LastPoint = PlaceInitEnd(M , profileType,malloclist->size(),AllocaList->size(),M->gvmap.size());
        
        sdmllvm::function * regf  = M->pf_functions["sdmprofile__Register_GV"];
        int cc = 0;
        for (sdmllvm::module::gvmapI I = M->gvmap.begin() ; I != M->gvmap.end();++I)
        {
            sdmllvm::globalvariable *GV = I->second ;
            std::string  name_string ("sdmprofile__GV_Name_" + GV->Name());
            std::string  type_string ("sdmprofile__GV_Type_" + GV->Name());
            ARGV args ;
            args.push_back (ARG( ++cc));
            args.push_back (ARG(GV->base,ARGpti ));
            args.push_back (ARG(GV->MemorySize()));
            args.push_back (ARG(GV->Name(), name_string,ARGSL));
            args.push_back (ARG(GV->TypeString(), type_string,ARGSL) );
            llvm::CallInst * fcall = sdmllvm::CreateCallInst(M, LastPoint,true, regf , args );
            LastPoint = fcall ;
        }
        
        
        sdmllvm::function *  logfunction  = M->pf_functions["sdmprofile__profile_access4"];
          cc = 0;
        for (sdmllvm::module::functionmapI  FI  = M->functionmap.begin(), FE = M->functionmap.end(); FI != FE; ++FI)
        {
            sdmllvm::function * FF = FI->second ;
            for (sdmllvm::function::bbmapI BI = FF->bbmap.begin(), BE = FF->bbmap.end();BI != BE; ++BI)
            {
                sdmllvm::basicblock * BB = BI->second ;
                for (sdmllvm::basicblock::meminstmapI II = BB->meminstmap.begin(), IE = BB->meminstmap.end(); II != IE; ++II)
                {
                    sdmllvm::MemoryAccessInst * mem = II->second ;
                    sdmllvm::function * F = M->functionmap[ mem->base->getParent()->getParent()] ;
                    llvm::Value * function_info ;
                    function_info = function_info_map[F];
                    ARGV args {ARG( mem->getPointerOperand(),ARGpti),ARG(mem->AccessType,ARGI8 ),ARG(function_info ) } ;
                    llvm::CallInst * fcall = CreateCallInst(M,mem->base , false ,logfunction ,args ) ;
                    cc++;
                }
                
            }
        }

        Save_Output(M,InputFilename , OutputFilename , Convert_object_file );
        
        return 1 ;
    }
    
    std::string getfullpath(std::string path__)
    {
        std::string path (path__) ;
        char actualpath [PATH_MAX];
        char *ptr;
        ptr = realpath( path.c_str(), actualpath);
        return  std::string(actualpath);
    }
    
    int static_profiling(std::string InputFilename, std::string OutputFilename,int job ,bool Convert_object_file)
    {
        
        int profileType = 2;
        message << go << "\tSDM Profile Tool " << go << "\tOptions :" << go <<  "\t Scope\t\t: Static Profiling " << go <<
        "\t Input File \t: " << InputFilename << go << "\t Output File \t: " << OutputFilename << go ;
        
        std::string appfilename = getfullpath(InputFilename);
        std::string libraryfile = getfullpath("staticprofile.so") ;
        
        std::stringstream command ;
        command << "opt -load " << libraryfile << " -sdm_static_profile1 -sdm_static_profile2  -sdm_static_profile3 " <<  appfilename << " > /dev/null";
        message << command.str() << go ;
        int call_rtn = std::system(command.str().c_str());
        return 1;
    }
}


namespace profile_simple // result processing functions
{
    int  printResultFile(std::string InputFilename,   int job)
    {
        make_csv_file( InputFilename.c_str()) ;
        return 1;
    }
     
}


using namespace llvm ;

static llvm::cl::opt<std::string> OutputFilename("o", llvm::cl::desc("output filename"), llvm::cl::value_desc("filename"));
static llvm::cl::opt<std::string> InputFilename ("i", llvm::cl::desc("input filename"), llvm::cl::value_desc("filename"));
static llvm::cl::opt<int> job ("a", llvm::cl::desc("action \n 1  Heap variable profiling only (heap variables allocated with malloc) \n 2  Stack variable profiling only \n 3  Global variable only \n 4  All variable (Heap,Stack,Global) \n 6 generate .csv file profile results "), llvm::cl::value_desc("number"));
static llvm::cl::opt<bool> Convert_object_file("obj", llvm::cl::desc("convert output bitcode to object file"));
static cl::extrahelp MoreHelp( help___  );

int main (int argc, char ** argv)
{
    //llvm_shutdown_obj Y;
    message.show = true;
    debug.show = true ; // set to false to disable logging
    int action;
    std::string outf ;
    std::string inf ;
    bool obj ;
    
    if (argc == 2)
    {
        inf =  std::string(argv[1]) ;
        if (inf == "-h" || inf == "--help")
        {
            // show help and exit
            message  << help___ << go  ;
            return 1;
        }
        
        outf = inf;
        boost::replace_last(outf , ".bc",".o");
        action = 4;
        obj = true ;
    }else
    {
        cl::ParseCommandLineOptions(argc, argv, "");
        action = job ;
        inf = InputFilename ;
        outf =  OutputFilename ;
        obj = Convert_object_file ;
    }
    
    switch (action)
    {
        case HEAP_:
            profile_simple::heap_profiling_only(inf, outf, action, obj);
            break;
        case STACK_:
            profile_simple::stack_profiling_only(inf, outf, action, obj);
            break ;
        case GV_:
            profile_simple::GV_profiling_only(inf, outf, action, obj);
            break ;
        case ALL_:
            profile_simple::All_profiling(inf, outf, action, obj);
            break ;
        case 5:
            profile_simple::static_profiling(inf, outf, action, obj);
            break;
        case 6:
            profile_simple::printResultFile (inf, action);
            break ;
        default:
            break;
    }
    return 0 ;
}





