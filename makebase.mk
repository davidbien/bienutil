# makebase.mk
# dbien: evolving - today is 09MAY2019.
# Generic base makefile for compiling a variety of possibilities.

ifndef MOD_ARCH
# Then we are building for local machine architecture for sure.
MOD_ARCH = $(shell uname -m)
MACHINE_ARCH = $(MOD_ARCH)
else
# Then we are building for some architecture - not necessarily the current machine.
MACHINE_ARCH = $(shell uname -m)
endif 

ifeq (1,$(NDEBUG))
$(info ***RELEASE BUILD***)
BUILD_DIR = Release_$(MOD_ARCH)
else
BUILD_DIR = Debug_$(MOD_ARCH)
endif
$(info BUILD_DIR: $(BUILD_DIR))

ifndef CC
$(error Must define CC to be the compiler of your choice.)
endif

# Allow using newer versions of clang-tidy.
ifndef CLANGTIDY
CLANGTIDY := clang-tidy
endif

ifndef MAKEBASE
$(error Must define MAKEBASE to be the location of this file so that depending on it works correctly.)
endif

CXXFLAGS_DEFINES =  $(MOD_DEFINES) -D_REENTRANT -D_STLP_MEMBER_TEMPLATES -D_STLP_USE_NAMESPACES -D_STLP_CLASS_PARTIAL_SPECIALIZATION -D_STLP_FUNCTION_TMPL_PARTIAL_ORDER
CXXFLAGS_INCLUDES = -I $(HOME)/dv/bienutil -I $(HOME)/dv
CXXFLAGS_BASE = $(CXXFLAGS_DEFINES) $(CXXFLAGS_INCLUDES)
ifeq (1,$(NDEBUG))
# Release flags:
CXXANDLINKFLAGS = -O3 -DNDEBUG
else
# Debug flags:
CXXANDLINKFLAGS = -g
endif

ifeq (1,$(TIDY))
$(info ***TIDY BUILD***)
ifndef TIDYFLAGS
TIDYFLAGS := -checks=* -header-filter=.*
endif
CC := $(CLANGTIDY)
CXX := $(CLANGTIDY)
# setup CCU to be nvcc for gcc.
CCU := $(CLANGTIDY)
CXXANDLINKFLAGS += -std=gnu++17 -pthread
# Remove any existing *.tidy files from the build dir. We will always build tidy files because the user is requesting them specially.
$(shell rm -f $(BUILD_DIR)/*.tidy >/dev/null 2>/dev/null)
else
ifneq (,$(findstring nvcc,$(CC)))
CXX := /usr/local/cuda/bin/nvcc
# We allow CCU (cuda compiler) to be potentially clang since apparently it supports cuda natively (interestingly enough).
CCU := /usr/local/cuda/bin/nvcc 
CXXANDLINKFLAGS += -std=c++14 -Xcompiler="-pthread"
else
ifeq ($(CC),gcc)
CXX := g++
# setup CCU to be nvcc for gcc.
CCU := /usr/local/cuda/bin/nvcc
CXXANDLINKFLAGS += -std=gnu++17 -pthread
else
CXX := clang 
# setup CCU to be clang for clang - just in case it supports it the same as nvcc, etc (i.e. we don't know anything yet, just putting this here).
CCU := clang
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
endif

CXXFLAGS = $(CXXFLAGS_BASE) $(CXXFLAGS_COMPILER_SPECIAL) $(CXXANDLINKFLAGS)

LINKFLAGS = $(CXXANDLINKFLAGS) -lstdc++ 

DEPDIR := .d/$(BUILD_DIR)
$(shell mkdir -p $(BUILD_DIR) >/dev/null)
$(shell mkdir -p $(DEPDIR) >/dev/null)
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

ifeq (,$(findstring nvcc,$(CC)))
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
else
DEPFLAGS = -MT $@ -Xcompiler="-MMD" -Xcompiler="-MP" -Xcompiler="-MF $(DEPDIR)/$*.Td"
endif

#$(info CPPFLAGS: $(CPPFLAGS))
#$(info TARGET_ARCH: $(TARGET_ARCH))

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
COMPILE.cc = $(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
COMPILE.cu = $(CCU) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c

%.o : %.c
$(BUILD_DIR)/%.o : %.c $(DEPDIR)/%.d makefile $(MAKEBASE)
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

%.o : %.cc
$(BUILD_DIR)/%.o : %.cc $(DEPDIR)/%.d makefile $(MAKEBASE)
	$(COMPILE.cc) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

%.o : %.cxx
$(BUILD_DIR)/%.o : %.cxx $(DEPDIR)/%.d makefile $(MAKEBASE)
	$(COMPILE.cc) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

%.o : %.cpp
$(BUILD_DIR)/%.o : %.cpp $(DEPDIR)/%.d makefile $(MAKEBASE)
	$(COMPILE.cc) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

# Create *.tidy file from clang-tidy output. Display it to the screen if (1 != $(NOTIDYOUTPUT)).
%.tidy : %.c
$(BUILD_DIR)/%.tidy : %.c
ifeq (1,$(NOTIDYOUTPUT))
	$(CC) $(TIDYFLAGS) $< -- $(CFLAGS) > $(BUILD_DIR)/$*.tidy
else
	$(CC) $(TIDYFLAGS) $< -- $(CFLAGS) | tee $(BUILD_DIR)/$*.tidy
endif

%.tidy : %.cc
$(BUILD_DIR)/%.tidy : %.cc
ifeq (1,$(NOTIDYOUTPUT))
	$(CXX) $(TIDYFLAGS) $< -- $(CXXFLAGS) > $(BUILD_DIR)/$*.tidy
else
	$(CXX) $(TIDYFLAGS) $< -- $(CXXFLAGS) | tee $(BUILD_DIR)/$*.tidy
endif

%.tidy : %.cxx
$(BUILD_DIR)/%.tidy : %.cxx
ifeq (1,$(NOTIDYOUTPUT))
	$(CXX) $(TIDYFLAGS) $< -- $(CXXFLAGS) > $(BUILD_DIR)/$*.tidy
else
	$(CXX) $(TIDYFLAGS) $< -- $(CXXFLAGS) | tee $(BUILD_DIR)/$*.tidy
endif

%.tidy : %.cpp
$(BUILD_DIR)/%.tidy : %.cpp
ifeq (1,$(NOTIDYOUTPUT))
	$(CXX) $(TIDYFLAGS) $< -- $(CXXFLAGS) > $(BUILD_DIR)/$*.tidy
else
	$(CXX) $(TIDYFLAGS) $< -- $(CXXFLAGS) | tee $(BUILD_DIR)/$*.tidy
endif

.PHONY: all clean tidy

# Create dependency on all that must be overridden by the eventual makefile. This must be the first rule seen by make to make it by default.
all:

# The above predeclare of "all" allows us to use a generic clean that merely removes the build directory entirely if present.
clean:
	rm -rf $(BUILD_DIR) >/dev/null 2>/dev/null

# Declare the below as a utility in case it is not declared by the containing makefile.
tidy:
