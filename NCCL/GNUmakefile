# AMREX_HOME defines the directory in which we will find all the AMReX code.
# If you set AMREX_HOME as an environment variable, this line will be ignored
AMREX_HOME ?= ../../amrex

#DEBUG        = TRUE 
#USE_CCACHE   = TRUE
USE_OMP      = FALSE
TINY_PROFILE = TRUE
USE_MPI      = TRUE
USE_CUDA     = TRUE
BL_NO_FORT   = TRUE
COMP         = gnu
DIM          = 3

USE_NCCL     = TRUE
USE_RCCL     = FALSE
USE_NVSHMEM  = FALSE

include $(AMREX_HOME)/Tools/GNUMake/Make.defs

include ./Make.package
include $(AMREX_HOME)/Src/Base/Make.package
include $(AMREX_HOME)/Tools/GNUMake/Make.rules

# =====================================================
# Extra Additions

ifdef USE_NCCL
  USE_NCCL := $(strip $(USE_NCCL))
else
  USE_NCCL := FALSE
endif

ifdef USE_NVSHMEM
  USE_NVSHMEM := $(strip $(USE_NVSHMEM))
else
  USE_NVSHMEM := FALSE
endif


ifeq ($(USE_NCCL),TRUE)
#  NCCL_DIR := /pscratch/sd/j/jmsexton/test_amrex/nccl/install
  EXTRACXXFLAGS += -I$(NCCL_DIR)/include
  LINKFLAGS += -L$(NCCL_DIR)/lib -lnccl
  DEFINES += -DAMREX_USE_CCL
endif

ifeq ($(USE_RCCL),TRUE)
  RCCL_DIR := $(ROCM_PATH)/rccl
  EXTRACXXFLAGS += -I$(RCCL_DIR)/include
  LINKFLAGS += -L$(RCCL_DIR)/lib -lrccl
  DEFINES += -DAMREX_USE_CCL
endif

ifeq ($(USE_NVSHMEM),TRUE)
#  NVSHMEM_DIR := $(CUDATOOLKIT_HOME)/../../comm_libs/nvshmem/
  NVSHMEM_DIR := /pscratch/sd/j/jmsexton/test_amrex/nvshmem/nvshmem
##  NVSHMEM_DIR := /pscratch/sd/j/jmsexton/test_amrex/nvshmem_try_libfabric/nvshmem
  LIBFABRIC_HOME := /opt/cray/libfabric/1.15.0.0/
  EXTRACXXFLAGS += -I$(NVSHMEM_DIR)/include
  EXTRACXXFLAGS += -I$(LIBFABRIC_HOME)/include/rdma
  LINKFLAGS += -L$(NVSHMEM_DIR)/lib -lnvshmem -lnvidia-ml
  LINKFLAGS += -L$(LIBFABRIC_HOME)/lib64 -lfabric
  DEFINES += -DAMREX_USE_NVSHMEM
  DEFINES += -DAMREX_GPUS_PER_NODE=4
endif
