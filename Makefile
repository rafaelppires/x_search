CC             := gcc
CXX            := g++-5
SHELL          := /bin/bash
SGX            := 0
TARGET         := xsearch
SRCDIR         := src
ENCLAVENAME=enclave_$(TARGET)
LIBRESSLDIR=/home/rafaelpp/dev/libressl-2.4.1
LIBRARIES=$(addprefix ${LIBRESSLDIR}/build/, tls crypto ssl)
#LDFLAGS=$(addprefix -L,$(LIBRARIES)) -ltls -lcrypto -lssl -lpthread
LDFLAGS= -lpthread
SGX_COMMONDIR        := ../../sgx-systems/sgx_common
INCLUDEDIRS := $(SRCDIR) ${LIBRESSLDIR}/incilude/ ../broker_xsearch/src \
               $(SGX_COMMONDIR)
SERVERCPPOBJS= $(TARGET).o connection_loop.o server_protocol.o query_manager.o enclave_ocalls.o crypto_stream.o

colon:= :
empty:=
space:= $(empty) $(empty)

CFLAGS := -m64 
ifeq ($(DEBUG),1)
CFLAGS += -g -O0
else
CFLAGS += -O2
endif

ifeq ($(SGX),1)
BINDIR := sgx_bin
OBJDIR := sgx_obj
TRUSTEDOBJS          := query_manager.o crypto_stream.o $(ENCLAVENAME)_t.o
SGX_SDK              := /opt/intel/sgxsdk
SGX_LIBRARY_PATH     := $(SGX_SDK)/lib64
SGX_ENCLAVE_SIGNER   := $(SGX_SDK)/bin/x64/sgx_sign
SGX_EDGER8R          := $(SGX_SDK)/bin/x64/sgx_edger8r
Trts_Library_Name    := sgx_trts
Service_Library_Name := sgx_tservice
App_Link_Flags       := -m64 -L$(SGX_LIBRARY_PATH) -lsgx_uae_service -lsgx_urts
CFLAGS               += -DENABLE_SGX
INCLUDEDIRS          += $(SGX_SDK)/include  
SERVERCPPOBJS := $(filter-out $(TRUSTEDOBJS), $(SERVERCPPOBJS)) \
                 $(ENCLAVENAME)_u.o 
SGXCOMOBJS := $(addprefix $(OBJDIR)/, sgx_initenclave.o sgx_errlist.o)
Trusted_Link_Flags := -m64 -Wl,--no-undefined -nostdlib \
    -nodefaultlibs -nostartfiles -L$(SGX_LIBRARY_PATH)  \
    -Wl,--whole-archive -l$(Trts_Library_Name) -Wl,--no-whole-archive \
    -Wl,--start-group -lsgx_tstdc -lsgx_tstdcxx -lsgx_tcrypto \
    -l$(Service_Library_Name) -Wl,--end-group \
    -Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined \
    -Wl,-pie,-eenclave_entry -Wl,--export-dynamic  \
    -Wl,--defsym,__ImageBase=0 \
    -Wl,--version-script=$(SRCDIR)/$(ENCLAVENAME).lds
Common_C_Cpp_Flags := $(CFLAGS) -nostdinc -fvisibility=hidden \
                      -fpie -fstack-protector -fno-builtin-printf
Enclave_C_Flags   := -Wno-implicit-function-declaration -std=c11 \
                     $(Common_C_Cpp_Flags)
Enclave_Cpp_Flags := $(Common_C_Cpp_Flags) -I$(SGX_SDK)/include/stlport \
                     -std=c++11 -nostdinc++
Enclave_IncDirs   := $(SRCDIR) $(SGX_SDK)/include $(SGX_SDK)/include/tlibc \
                     $(SGX_COMMONDIR)
Enclave_Include   := $(addprefix -I, $(Enclave_IncDirs))
App_Cpp_Objs      := $(SGXCOMOBJS)
Trusted_Objects   := $(addprefix $(OBJDIR)/,$(TRUSTEDOBJS))
all: $(BINDIR)/$(TARGET) $(BINDIR)/$(TARGET).signed.so
$(addprefix $(OBJDIR)/,server_protocol.o $(TARGET).o): $(SRCDIR)/$(ENCLAVENAME)_u.c
$(OBJDIR)/query_manager.o : $(SRCDIR)/$(ENCLAVENAME)_t.c
else
BINDIR := ntv_bin
OBJDIR := ntv_obj
App_Link_Flags += -lcryptopp
all: $(BINDIR)/$(TARGET)
endif

App_Include  := $(addprefix -I,$(INCLUDEDIRS))
App_Cpp_Objs += $(addprefix $(OBJDIR)/, $(SERVERCPPOBJS))

$(BINDIR)/$(TARGET): ${App_Cpp_Objs} | $(BINDIR)
	@${CXX} $^ -o $@ ${App_Link_Flags} ${LDFLAGS}
	@echo -e "LINK (App)\t=>\t$@"

$(OBJDIR)/%.o : $(SRCDIR)/%.c | $(OBJDIR)
	@${CC} ${CFLAGS} ${App_Include} -c $< -o $@
	@echo -e "CC (App)\t<=\t$<"

$(OBJDIR)/%.o : $(SRCDIR)/%.cpp | $(OBJDIR)
	@${CXX} -std=c++11 ${CFLAGS} ${App_Include} -c $< -o $@
	@echo -e "CXX (App)\t<=\t$<"

$(SGXCOMOBJS) : $(OBJDIR)/%.o : $(SGX_COMMONDIR)/%.cpp | $(OBJDIR)
	@${CXX} -std=c++11 ${CFLAGS} ${App_Include} -c $< -o $@
	@echo -e "CXX (App)\t<=\t$<"

$(SRCDIR)/$(ENCLAVENAME)_u.c : $(SRCDIR)/$(ENCLAVENAME).edl $(SGX_EDGER8R)
	@cd $(dir $@) && $(SGX_EDGER8R) --untrusted ../$< --search-path ../$(dir $<) --search-path $(SGX_SDK)/include
	@echo -e "EDGER (App)\t=>\t$@"

############## TRUSTED #########################################################
$(BINDIR)/$(TARGET).signed.so : %.signed.so : %.so 
	@$(SGX_ENCLAVE_SIGNER) sign -enclave $< -config $(SRCDIR)/$(ENCLAVENAME).config.xml -out $@ -key $(SRCDIR)/enclave-key.pem
	@echo -e "SIGN (Enclave)\t=>\t$@"

$(BINDIR)/$(TARGET).so : $(Trusted_Objects)
	@$(CXX) $^ -o $@ $(Trusted_Link_Flags)
	@echo -e "LINK (Enclave)\t=>\t$@"

$(SRCDIR)/$(ENCLAVENAME)_t.c : $(SRCDIR)/$(ENCLAVENAME).edl $(SGX_EDGER8R)
	@cd $(dir $@) && $(SGX_EDGER8R) --trusted ../$< --search-path ../$(dir $<) --search-path $(SGX_SDK)/include
	@echo -e "EDGER (Enclave)\t=>\t$@"

$(OBJDIR)/$(ENCLAVENAME)_t.o : $(SRCDIR)/$(ENCLAVENAME)_t.c
	@${CC} -c $< -o $@ ${Enclave_C_Flags} ${Enclave_Include} 
	@echo -e "CC (Enclave)\t<=\t$<"

$(filter-out %_t.o, $(Trusted_Objects)) : $(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	@${CXX} -c $< -o $@ ${Enclave_Include} ${Enclave_Cpp_Flags} 
	@echo -e "CXX (Enclave)\t<=\t$<"
################################################################################

$(BINDIR):
	@mkdir $(BINDIR)

$(OBJDIR):
	@mkdir $(OBJDIR)

.PHONY: run clean

run:
	LD_LIBRARY_PATH=$(subst $(space),$(colon),$(LIBRARIES)) ./$(BINDIR)/$(TARGET)

clean:
	rm -rf $(BINDIR) $(OBJDIR) $(SRCDIR)/*_{t,u}.{c,h} 

