#include <stdio.h>
#include <stdlib.h>
#include "macros.h"
#include "tpl.h"
#include "readresult.h"
#include <fcntl.h>



typedef struct sdmprofile__list_item__heap
{
    u_int64_t instruction_number  ;
    u_int64_t r ;
    u_int64_t w ;
    u_int32_t size ;
    double createtime;
    double freedtime;
    u_int32_t Findex;
    
    u_int64_t key ;
    int deleted ;
    u_int64_t upperbound ;
    struct sdmprofile__list_item__heap * next ;
}sdmprofile__list_item__heap;


typedef struct sdmprofile__GV_allocation_info
{
    u_int32_t index;
    u_int64_t r ;
    u_int64_t w ;
    u_int64_t address ;
    u_int32_t size ;
    char * VarName;
    char * TypeString;
    
    u_int64_t upperbound ;
}sdmprofile__GV_allocation_info;

typedef struct sdmprofile__stack_allocations_info
{
    u_int64_t instruction_number  ;
    u_int64_t r;
    u_int64_t w ;
    u_int32_t size;
    u_int32_t function_number ;
    
    u_int64_t address ;
    u_int64_t upperbound ;
    struct sdmprofile__stack_allocations_info * next ;
    
}sdmprofile__stack_allocations_info;

typedef struct  stack_totals
{
    u_int64_t R , W , inst_number, count ; u_int32_t size ; u_int32_t Findex;
}stack_totals;

typedef struct heap_totals
{
    u_int64_t R , W ,inst_number , count; u_int32_t size ; u_int32_t Findex ;
} heap_totals;


typedef struct function_info
{
    u_int32_t index;
    char * Fname;
}function_info ;


int head;
double sdmprofile__AppStartTime;
double sdmprofile__AppEndTime;
long long sdmprofile__Total_AccessCount_R,sdmprofile__Total_AccessCount_W ;
int profileType ;
struct sdmprofile__stack_allocations_info stack;
struct sdmprofile__list_item__heap  heap;
struct sdmprofile__GV_allocation_info  GV;
u_int64_t Findex ; char *Fname ;
u_int64_t functionsCount, heapallocationCount,stackallocationCount,GVCount = 0;

function_info * firstFinfo ;
struct stack_totals * first_st;
struct heap_totals * first_hp;
struct sdmprofile__GV_allocation_info * first_gv ;
u_int32_t FindexNull ;
char * FnameEmpty = "-" ;

int decodepf   ( int file  )
{
    
    int dataexist  =  0;
    {
        tpl_node *tnhead ;
        tnhead  = tpl_map("A(i)", &head);
        tpl_load(tnhead, TPL_FD , file );
        tpl_unpack(tnhead ,1);
        {}
        tpl_free(tnhead );
    }
    //printf ("head:%d\n",head);
    
    switch (head)
    {
        case 1 :
        {
            tpl_node *tn;
            tn = tpl_map("A(fuUUUU)", &sdmprofile__AppStartTime , & profileType, &functionsCount, &heapallocationCount,&stackallocationCount,&GVCount);
            tpl_load(tn, TPL_FD , file );
            tpl_unpack(tn,1);
            {
                dataexist = 1 ;
                
                //printf("\nhp count:%lu, st count:%lu, GV count:%lu\n",heapallocationCount , stackallocationCount,GVCount);
                
                // allocate memory for function information
                FindexNull =  functionsCount + 100;
                firstFinfo = (struct function_info *)realloc(firstFinfo , sizeof(struct  function_info ) * (functionsCount + 5));
                if (firstFinfo == NULL)
                {
                    printf("resize failed"); return  0;
                }
                for (int ix = 0; ix < functionsCount;ix ++)
                {
                    firstFinfo[ix].Fname = FnameEmpty;
                    firstFinfo[ix].index = FindexNull;
                    
                }
                
                // stack totals
                if (stackallocationCount > 0)
                {
                    first_st = (struct stack_totals *) realloc (first_st,  stackallocationCount * sizeof(struct stack_totals ));
                    if (first_st == NULL)
                    {
                        printf(" resize failed"); return 0;
                    }
                    
                    for (int ix =0 ; ix < stackallocationCount ; ix++)
                    {
                        first_st[ix].R =0;
                        first_st[ix].W=0 ;
                        first_st[ix].count = 0 ;
                        first_st[ix].Findex = FindexNull ;
                    }
                }
                
                // heap totals
                if (heapallocationCount > 0)
                {
                    first_hp = (struct heap_totals   *) realloc (first_hp, heapallocationCount   * sizeof(struct  heap_totals ));
                    if (first_hp == NULL)
                    {
                        printf(" resize failed"); return 0;
                    }
                    
                    for (int ix =0 ; ix < heapallocationCount ; ix++)
                    {
                        first_hp[ix].R =0;
                        first_hp[ix].W=0 ;
                        first_hp[ix].count = 0 ;
                        first_hp[ix].Findex = FindexNull ;
                    }
                }
                
                // gv
                if (GVCount > 0)
                {
                    first_gv = (struct sdmprofile__GV_allocation_info   *) realloc (first_gv, GVCount   * sizeof(struct  sdmprofile__GV_allocation_info ));
                    if (first_gv == NULL)
                    {
                        printf(" resize failed"); return 0;
                    }
                    
                    for (int ix =0 ; ix < GVCount ; ix++)
                    {
                        first_gv[ix].r=0;
                        first_gv[ix].w=0 ;
                    }
                }
                
                
            }
            tpl_free(tn);
        }
            break ;
        case 2:
        {
            tpl_node *tn;
            tn = tpl_map("A(fUU)", &sdmprofile__AppEndTime , &sdmprofile__Total_AccessCount_R ,&sdmprofile__Total_AccessCount_W    );
            tpl_load(tn, TPL_FD , file );
            tpl_unpack(tn,1);
            {
                dataexist = 0 ;
            }
            tpl_free(tn);
        }
            break ;
        case 10:
        {
            tpl_node *tn;
            tn = tpl_map("A(us)", & Findex , &Fname );
            tpl_load(tn, TPL_FD , file );
            tpl_unpack(tn,1);
            {
                dataexist = 1 ;
                firstFinfo[Findex].index = Findex;
                firstFinfo[Findex].Fname = Fname;
                //printf("F : %lu , %s",Findex,Fname);
            }
            tpl_free(tn);
        }
            break;
        case 3:
        {
            tpl_node *tn;
            tn = tpl_map("A(S(iUUUiss))", &GV);
            tpl_load(tn, TPL_FD , file );
            tpl_unpack(tn,1);
            {
                dataexist = 1 ;
                //printf("gv -->%u\n",GV.index  );
                first_gv[GV.index].r = GV.r ;
                first_gv[GV.index].w = GV.r ;
                first_gv[GV.index].VarName = GV.VarName ;
                first_gv[GV.index]. TypeString = GV.TypeString ;
                first_gv[GV.index].size = GV.size ;
            }
            tpl_free(tn);
            
        }
            break;
        case 4:
        {
            tpl_node *tn;
            tn = tpl_map("A(S(UUUuffu))", &heap);
            tpl_load(tn, TPL_FD , file );
            tpl_unpack(tn,1);
            {
                dataexist = 1 ;
                //printf("H -->%lu\n",heap.instruction_number);
                first_hp[heap.instruction_number].count ++;
                first_hp[heap.instruction_number].Findex = heap.Findex;
                first_hp[heap.instruction_number].size = heap.size  ;
                first_hp[heap.instruction_number].R += heap.r ;
                first_hp[heap.instruction_number].W += heap.w ;
            }
            tpl_free(tn);
        }
            break ;
        case 5:
        {
            tpl_node *tn;
            tn = tpl_map("A(S(UUUuu))", &stack);
            tpl_load(tn, TPL_FD , file );
            tpl_unpack(tn,1);
            {
                dataexist = 1 ;
                first_st[stack.instruction_number].count ++;
                first_st[stack.instruction_number].Findex = stack.function_number;
                first_st[stack.instruction_number].size = stack.size  ;
                first_st[stack.instruction_number].R += stack.r ;
                first_st[stack.instruction_number].W += stack.w ;
            }
            tpl_free(tn);
        }
            break ;
        default:
            break;
    }
    return dataexist;
    
}

int make_csv_file(const char * filename)
{
    printf("\nWriting result to a csv(result.csv) file\n\n");
    
    int fdr = open ( filename, O_RDONLY);
    while (decodepf(fdr) ){}
    
    //printf("\n finish reading scope=%d\n", profileType);
    
    FILE *f;
    f = fopen("result.csv", "w+");
    
    if ((profileType == STACK_ || profileType == ALL_))
    {
        //printf("writing s\n");
        fprintf(f, "Stack Memory Region Profile Result\n\nInstruction Index,Read Count,Write Count,Memory Size,Hits,Function");
        
        for (int ix =0 ; ix < stackallocationCount ; ix++)
        {
            if (first_st[ix].Findex != FindexNull)
            {
                fprintf(f,"\n%d,%lu,%lu,%u,%lu,%s , \n",ix,first_st[ix].R, first_st[ix].W ,first_st[ix].size ,first_st[ix].count ,firstFinfo[first_st[ix].Findex].Fname) ;
            }
            
        }
        fprintf(f, "\n\n\n");
    }
    
    if (profileType == HEAP_ || profileType == ALL_)
    {
        //printf("writing h\n");
        fprintf(f, "Heap Memory Region Profile Result\n\nInstruction Index,Read Count,Write Count,Memory Size,Hits,Function");
        
        for (int ix =0 ; ix < heapallocationCount ; ix++)
        {
            if (first_hp[ix].Findex != FindexNull)
            {
                fprintf(f,"\n%d,%lu,%lu,%u,%lu,%s , \n",ix,first_hp [ix].R, first_hp[ix].W ,first_hp[ix].size  , first_hp[ix].count,firstFinfo[first_hp[ix].Findex].Fname) ;
            }
            
        }
        fprintf(f, "\n\n\n");
    }
    
    if (profileType == GV_ || profileType == ALL_)
    {
        //printf("writing gv\n");
        fprintf(f, "Global Variable Memory Region Profile Result\n\nIndex,Read Count,Write Count,Memory Size,Name, Type  ");
        
        for (int ix =0 ; ix < GVCount ; ix++)
        {
            fprintf(f,"\n%d,%lu,%lu,%u,%s,%s , \n",ix,first_gv [ix].r, first_gv[ix].w ,first_gv[ix].size  , first_gv[ix].VarName,first_gv[ix].TypeString ) ;
        }
        fprintf(f, "\n\n\n");
    }
    
    fclose(f);
    return  1;
}