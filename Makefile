# Makefile for POSIX
# ----------------------------------------------------------------------
# Written by Raph.K.
#

GCC = gcc
GPP = g++
AR  = ar
WRC = windres

# Base PATH
BASE_PATH = .
LLIB_PATH = $(BASE_PATH)/lib
SRC_PATH  = $(BASE_PATH)/src

# TARGET settings
TARGET_PKG = autoupgrader
TARGET_DIR = ./bin
TARGET_OBJ = ./obj

# FLTK config
FLTKCFG_CXX = $(shell fltk-config --use-images --cxxflags)
FLTKCFG_LDF = $(shell fltk-config --use-images --ldflags)

# DEFINITIONS
DEFS += -DDEBUG -g3

# Compiler optiops 
COPTS += -std=c++11

# Linker options
LOPTS +=

# Automatic detecting architecture.
KRNL := $(shell uname -s)
KVER := $(shell uname -r | cut -d . -f1)
ARCH := $(shell uname -m)

ifeq ($(KRNL),Darwin)
    GCC = llvm-gcc
    GPP = llvm-g++
    ifeq ($(shell test $(KVER) -gt 19; echo $$?),0)
        OPTARCH += -arch x86_64 -arch arm64
        LIBARCH = macos11
    else
        LIBARCH = macos10
    endif
	LNKOPT += -framework IOKit
    # --- prevent to pthreadd stack error by xcode ---
	# OPTARCH += -fno-stack-check
else
    SUBSYS := $(shell uname -s | cut -d _ -f1)
    ifeq ($(SUBSYS),MINGW64)
        # MinGW-W64 basically runs on Windows socket, and somethis
        # This Project is not for Windows. 
        UNSUPP   = 1
        OPTARCH += -mms-bitfields -mwindows -static
        LOPTS += -lwinmm
        LIBARCH = mingw64
	else
        LIBARCH = linux_$(ARCH)
    endif
    # OPTARCH += -DUSE_OMP -fopenmp -fno-ident -s
endif

# CC FLAGS
CFLAGS += -I$(SRC_PATH)
CFLAGS += -I$(LLIB_PATH) 
CFLAGS += -Ires
CFLAGS += $(FLTKCFG_CXX)
CFLAGS += $(DEFS)
CFLAGS += $(COPTS)
CFLAGS += $(OPTARCH)

# Windows Resource Flags
WFLGAS  = $(CFLAGS)

# LINK FLAG
LFLAGS += -L$(LLIB_PATH)
LFLAGS += -static
LFLAGS += $(FLTKCFG_LDF)
LFLAGS += $(LOPTS)

# Sources
SRCS = $(wildcard $(SRC_PATH)/*.cpp)

# Windows resource
WRES = res/resource.rc

# Make object targets from SRCS.
OBJS = $(SRCS:$(SRC_PATH)/%.cpp=$(TARGET_OBJ)/%.o)
WROBJ = $(TARGET_OBJ)/resource.o

.PHONY: prepare clean

all: prepare continue

continue: $(TARGET_DIR)/$(TARGET_PKG)

prepare:
	@mkdir -p $(TARGET_DIR)
	@mkdir -p $(TARGET_OBJ)

clean:
	@echo "Cleaning built targets ..."
	@rm -rf $(TARGET_DIR)/$(TARGET_PKG).*
	@rm -rf $(TARGET_INC)/*.h
	@rm -rf $(TARGET_OBJ)/*.o

$(OBJS): $(TARGET_OBJ)/%.o: $(SRC_PATH)/%.cpp
	@echo "Building $@ ... "
	@$(GPP) $(CFLAGS) -c $< -o $@

$(WROBJ): $(WRES)
	@echo "Building windows resource ..."
	@$(WRC) -i $(WRES) $(WFLAGS) -o $@

$(TARGET_DIR)/$(TARGET_PKG): $(OBJS) $(WROBJ)
	@echo "Generating $@ ..."
	@$(GPP) $(OBJS) $(WROBJ) $(CFLAGS) $(LFLAGS) -o $@
	@echo "done."
