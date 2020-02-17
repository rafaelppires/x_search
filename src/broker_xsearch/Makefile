CXX      := g++-5
SRCDIR   := src
BINDIR   := bin
OBJDIR   := obj
TARGET   := proxy_broker
OBJS     := $(addprefix $(OBJDIR)/, $(TARGET).o connection_handler.o)
XSOBJS   := $(addprefix $(OBJDIR)/, crypto_stream.o)
COMMOBJS := $(addprefix $(OBJDIR)/, crypto.o)
XSDIR    := ../x_search/src
COMMDIR  := ../../sgx-systems/sgx_common
INCDIRS  := $(SRCDIR) $(COMMDIR) $(XSDIR)
Include  := $(addprefix -I, $(INCDIRS))
C_Flags  := -m64 -std=c++11
L_Flags  := -lpthread -lcryptopp

ifeq ($(DEBUG),1)
C_Flags  += -g
endif

all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET) : $(OBJS) $(COMMOBJS) $(XSOBJS)| $(BINDIR)
	@${CXX} -o $@ $^ ${C_Flags} ${L_Flags}
	@echo "LINK\t=>\t$@"

$(OBJDIR)/%.o : $(XSDIR)/%.cpp | $(OBJDIR) 
	@${CXX} -c -o $@ $< ${C_Flags} ${Include}
	@echo "CXX\t<=\t$<"

$(OBJDIR)/%.o : $(COMMDIR)/%.cpp | $(OBJDIR)
	@${CXX} -c -o $@ $< ${C_Flags} ${Include}
	@echo "CXX\t<=\t$<"

$(OBJDIR)/%.o : $(SRCDIR)/%.cpp | $(OBJDIR)
	@${CXX} -c -o $@ $< ${C_Flags} ${Include}
	@echo "CXX\t<=\t$<"

clean:
	rm -rf $(BINDIR) $(OBJDIR)

$(BINDIR):
	@mkdir $(BINDIR)

$(OBJDIR):
	@mkdir $(OBJDIR)

.PHONY: all clean

