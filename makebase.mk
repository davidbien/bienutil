
CXXFLAGS_DEFINES =  $(CXXPROJ_DEFINES) -D_REENTRANT -D_STLP_MEMBER_TEMPLATES -D_STLP_USE_NAMESPACES -D_STLP_CLASS_PARTIAL_SPECIALIZATION -D_STLP_FUNCTION_TMPL_PARTIAL_ORDER
CXXFLAGS_INCLUDES = -I $(HOME)/dv/bienutil -I $(HOME)/dv
CXXFLAGS_BASE = $(CXXFLAGS_DEFINES) $(CXXFLAGS_INCLUDES)
CXXANDLINKFLAGS = -g

ifneq (,$(findstring nvcc,$(CC)))
CXX := /usr/local/cuda/bin/nvcc
CXXANDLINKFLAGS += -std=c++14 -Xcompiler="-pthread"
else 
ifeq ($(CC),gcc)
CXX := g++
CXXANDLINKFLAGS += -std=gnu++17 -pthread
else
CXX := clang 
CXXANDLINKFLAGS += -std=gnu++17 -pthread
# ASAN_OPTIONS=check_initialization_order=1
# ASAN_OPTIONS=detect_leaks=1
# ASAN_OPTIONS=detect_stack_use_after_return=1
#CLANG_ADDR_SANITIZE = -fsanitize=address -fsanitize-address-use-after-scope

# MSAN_OPTIONS=poison_in_dtor=1
# ASAN_OPTIONS=detect_leaks=1
# ASAN_OPTIONS=detect_stack_use_after_return=1
#CLANG_MEM_SANITIZE = -fsanitize=memory -fsanitize-memory-track-origins -fsanitize-memory-use-after-dtor
#CLANGSANITIZE = $(CLANG_ADDR_SANITIZE) $(CLANG_MEM_SANITIZE) -fno-omit-frame-pointer
#-fsanitize-blacklist=blacklist.txt 
CXXANDLINKFLAGS += $(CLANGSANITIZE)
CXXFLAGS_COMPILER_SPECIAL = -fdelayed-template-parsing
endif
endif

CXXFLAGS = $(CXXFLAGS_BASE) $(CXXFLAGS_COMPILER_SPECIAL) $(CXXANDLINKFLAGS)

LINKFLAGS = $(CXXANDLINKFLAGS) -lstdc++ 

ifeq (,$(findstring nvcc,$(CC)))
DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@
else
DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -Xcompiler="-MMD" -Xcompiler="-MP" -Xcompiler="-MF $(DEPDIR)/$*.Td"
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@
endif

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
COMPILE.cc = $(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c

%.o : %.c
%.o : %.c $(DEPDIR)/%.d makefile $(MAKEBASE)
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

%.o : %.cc
%.o : %.cc $(DEPDIR)/%.d makefile $(MAKEBASE)
	$(COMPILE.cc) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

%.o : %.cxx
%.o : %.cxx $(DEPDIR)/%.d makefile $(MAKEBASE)
	$(COMPILE.cc) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

%.o : %.cpp
%.o : %.cpp $(DEPDIR)/%.d makefile $(MAKEBASE)
	$(COMPILE.cc) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d
