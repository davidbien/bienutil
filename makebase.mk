
#          Copyright David Lawrence Bien 1997 - 2021.
# Distributed under the Boost Software License, Version 1.0.
#    (See accompanying file LICENSE_1_0.txt or copy at
#          https://www.boost.org/LICENSE_1_0.txt).

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

MACHINE_OS = $(shell uname)

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

ifndef MOD_CPPVER
MOD_CPPVER := -std=c++20
endif
ifndef MOD_CPPVER_NVCC
MOD_CPPVER_NVCC := -std=c++14
endif
$(info MOD_CPPVER: $(MOD_CPPVER))

# Allow using newer versions of clang-tidy.
ifndef CLANGTIDY
CLANGTIDY := clang-tidy
endif

ifndef MAKEBASE
$(error Must define MAKEBASE to be the location of this file so that depending on it works correctly.)
endif

CXXFLAGS_DEFINES =  $(MOD_DEFINES) -D_REENTRANT
CXXFLAGS_INCLUDES = $(MOD_INCLUDES) 
#-I"$(HOME)/dv/bienutil" -I"$(HOME)/dv"
CXXFLAGS_BASE = $(CXXFLAGS_DEFINES) $(CXXFLAGS_INCLUDES)

ifeq (1,$(NDEBUG))
# Release flags:
CXXANDLINKFLAGS = -DNDEBUG -flto
LINKFLAGS_BASE = -O3 -fwhole-program
CXXFLAGS_BASE += -O3
else
# Debug flags:
ifneq (Darwin,$(MACHINE_OS))
CXXANDLINKFLAGS = -ggdb
else
CXXANDLINKFLAGS = -glldb
endif
endif

#! The include of the TCMALLOC lib must be first so that it can bind to malloc before any other shared lib initializes.
ifeq (1,$(MOD_TCMALLOC))
ifneq (1,$(NDEBUG))
MK_LIBS := -ltcmalloc_debug
else
MK_LIBS := -ltcmalloc
endif
endif

ifneq (Darwin,$(MACHINE_OS))
ifeq (clang,$(CC))
LINKFLAGS_BASE += -fuse-ld=lld 
endif
endif

ifeq (1,$(TIDY))
$(info ***TIDY BUILD***)
ifndef TIDYFLAGS
ifndef TIDYCHECKFLAGS
TIDYCHECKFLAGS := -checks=*,-fuchsia-*,-cppcoreguidelines-pro-bounds-array-to-pointer-decay,-hicpp-no-array-decay,-cppcoreguidelines-macro-usage,-modernize-use-trailing-return-type,-readability-named-parameter,-misc-non-private-member-variables-in-classes,-google-runtime-int
ifdef MODTIDYCHECKFLAGS
TIDYCHECKFLAGS := $(TIDYCHECKFLAGS),$(MODTIDYCHECKFLAGS)
else
ifneq (1,$(TIDYSTRICT))
MODTIDYCHECKFLAGS = -modernize-use-using,-performance-unnecessary-copy-initialization,-google-build-using-namespace,-llvm-include-order,-llvm-header-guard,-readability-implicit-bool-conversion
TIDYCHECKFLAGS := $(TIDYCHECKFLAGS),$(MODTIDYCHECKFLAGS)
endif
endif
endif
TIDYFLAGS := $(TIDYCHECKFLAGS) -header-filter=.*
endif
CC := $(CLANGTIDY)
CXX := $(CLANGTIDY)
CCU := $(CLANGTIDY)
CXXANDLINKFLAGS += $(MOD_CPPVER) -pthread
# Remove any existing *.tidy files from the build dir. We will always build tidy files because the user is requesting them specially.
$(shell rm -f $(BUILD_DIR)/*.tidy >/dev/null 2>/dev/null)
else
ifneq (,$(findstring nvcc,$(CC)))
CXX := /usr/local/cuda/bin/nvcc
# We allow CCU (cuda compiler) to be potentially clang since apparently it supports cuda natively (interestingly enough).
CCU := /usr/local/cuda/bin/nvcc 
CXXANDLINKFLAGS += $(MOD_CPPVER_NVCC) -Xcompiler="-pthread"
ifneq (1,$(NDEBUG))
# Addition debug flags for nvcc:
CXXANDLINKFLAGS += -G -O0
endif
# Generate the various compute capacities according to the project's needs:
CUDAGENCODEOPTIONS := $(foreach gc, $(CUDAGENCODES), -gencode arch=compute_$(gc),code=sm_$(gc))
else
ifeq ($(CC),gcc)
CXX := g++
# setup CCU to be nvcc for gcc.
CCU := /usr/local/cuda/bin/nvcc
CXXANDLINKFLAGS += $(MOD_CPPVER) -pthread
else
ifneq (,$(findstring clang,$(CC)))
# Allow the use of any version of clang by copying CC.
CXX := $(CC)
CXXANDLINKFLAGS += $(MOD_CPPVER) -pthread --cuda-path=/usr/local/cuda -I"/usr/local/cuda/targets/$(MOD_ARCH)-linux/include"
CLANG_INSTALL_DIR := $(dir $(shell readlink -f $(shell which clang)))..
MK_LIBS += -lm -Wl,-rpath,$(CLANG_INSTALL_DIR)/lib
ifeq (1,$(MOD_FORCE_CLANG_LIBCPP))
CXXANDLINKFLAGS += -stdlib=libc++
MK_LIBS += -lc++abi
endif #(1,$(FORCE_CLANG_LIBCPP))
ifeq (1,$(MOD_DEBUG_LIBCPP))
CXXFLAGS_BASE += -D_LIBCPP_DEBUG -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS
endif #(1,$(MOD_DEBUG_LIBCPP))
# setup CCU to be clang for clang - separate out the cuda specific compile and link flags.
CCU := $(CC)
CUDAGENCODEOPTIONS := $(foreach gc, $(CUDAGENCODES), --cuda-gpu-arch=sm_$(gc))
CCUANDLINKFLAGS += $(CUDAGENCODEOPTIONS)
ifneq (1,$(NDEBUG))
CXXANDLINKFLAGS += -O0
CCUANDLINKFLAGS += --cuda-noopt-device-debug
# --no-cuda-version-check -fcuda-short-ptr -fcuda-rdc -fcuda-approx-transcendentals -nocudainc -nocudalib --cuda-path-ignore-env --cuda-path=<arg> --ptxas-path=<arg>
endif
# Generate the various compute capacities according to the project's needs:
# clang doesn't allow separating the codegen from the arch.
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
else
$(error Unrecognized compiler $(CC) - cannot continue.)
endif
endif
endif
endif

CXXFLAGS = $(CXXFLAGS_BASE) $(CXXFLAGS_COMPILER_SPECIAL) $(CXXANDLINKFLAGS)

LINKFLAGS = $(LINKFLAGS_BASE) $(CXXANDLINKFLAGS) $(MK_LIBS) $(MOD_LIBS) -lstdc++

DEPDIR := .d/$(BUILD_DIR)
$(shell mkdir -p $(BUILD_DIR) >/dev/null)
$(shell mkdir -p $(DEPDIR) >/dev/null)
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

#$(info CPPFLAGS: $(CPPFLAGS))
#$(info TARGET_ARCH: $(TARGET_ARCH))

ifeq (,$(findstring nvcc,$(CC)))
# Normal, non-NVCC build dependency generation to a "temp .d" file:
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
COMPILE.cc = $(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
COMPILE.cu = $(CCU) $(DEPFLAGS) $(CXXFLAGS) $(CCUANDLINKFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
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

%.o : %.cu
$(BUILD_DIR)/%.o : %.cu $(DEPDIR)/%.d makefile $(MAKEBASE)
	$(COMPILE.cu) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)
else
# For NVCC we compile things a bit differently.
# For C and CPP files we just pass through the dependency parameters to the compiler and let it do the work in a single pass.
DEPFLAGS_C = -MT $@ -Xcompiler="-MMD" -Xcompiler="-MP" -Xcompiler="-MF $(DEPDIR)/$*.Td"
COMPILE.c = $(CC) $(DEPFLAGS_C) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) --compile -x c
COMPILE.cc = $(CXX) $(DEPFLAGS_C) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) --compile -x c++
# For *.cu files there are two passes. The first produces the dependencies for the makefile, the second compiles the code.
DEPFLAGS_CU = -M -o $(DEPDIR)/$*.Td
COMPILE.cu_dep = $(CCU) $(DEPFLAGS_CU) $(CXXFLAGS) $(CUDAGENCODEOPTIONS) $(CPPFLAGS) $(TARGET_ARCH)
COMPILE.cu = $(CCU) $(CXXFLAGS) $(CUDAGENCODEOPTIONS) $(CPPFLAGS) $(TARGET_ARCH) --compile -x cu

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

%.o : %.cu
$(BUILD_DIR)/%.o : %.cu $(DEPDIR)/%.d makefile $(MAKEBASE)
	$(COMPILE.cu_dep) $<
	$(COMPILE.cu) $(OUTPUT_OPTION) $<
	$(POSTCOMPILE)
endif

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
