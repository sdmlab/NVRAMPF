EXEC = profiletool
STATICLIB = staticprofile.so
PF = profile_functions.bc
SRC_DIR = src/
CXX = clang++
CC = clang
LLVMLINKER = llvm-link
CXXFLAGS = -I/usr/local/include  -D_DEBUG -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -O3 -fomit-frame-pointer -std=c++11 -fvisibility-inlines-hidden -fno-exceptions -fno-rtti -fPIC -Woverloaded-virtual -ffunction-sections -fdata-sections -Wcast-qual

LDFLAGS	=  -L/usr/local/lib
LIBS = -lLLVMBitWriter -lLLVMBitReader -lLLVMCore -lLLVMSupport -ldl -pthread

OBJS = $(SRC_DIR)sdmllvm.o $(SRC_DIR)profile_simple.o $(SRC_DIR)tpl.o $(SRC_DIR)readresult.o
OBJSLIB = $(SRC_DIR)static_profile.o
OBJPF = $(SRC_DIR)pf.bc $(SRC_DIR)tpl.bc $(SRC_DIR)r.bc

all: $(EXEC) $(PF)
#$(STATICLIB)

$(EXEC): $(OBJS)    ; @echo " LD ==> '$@'" ; $(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o $(EXEC)

.cpp.o:             ; @echo "  ==> '$@'" ; $(CXX) $(CXXFLAGS) $(DEF) -c $*.cpp -o $*.o

.c.o :              ; @echo "  ==> '$@'" ; $(CC) -c $*.c -o $*.o


$(PF) : $(OBJPF)    ; @echo " LINK ==> '$@'" ; $(LLVMLINKER) $(OBJPF) -o $@

$(SRC_DIR)tpl.bc :  ; @echo "  ==> '$@'" ; $(CC) $(SRC_DIR)tpl.c -c -flto -o $@

$(SRC_DIR)pf.bc :   ; @echo "  ==> '$@'" ; $(CC) $(SRC_DIR)profile_functions.c -c -flto -o $@

$(SRC_DIR)r.bc :    ; @echo "  ==> '$@'" ; $(CC) $(SRC_DIR)readresult.c -c -flto -o $@



$(STATICLIB):$(OBJSLIB) ; @echo " LD ==> '$@'" ; $(CXX) $(LDFLAGS) -rdynamic -shared $(OBJSLIB) -o $(STATICLIB)

clean: ;rm -f $(OBJS) $(OBJSLIB) $(OBJPF) $(EXEC) $(PF)






