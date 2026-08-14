# Project Name
# Sources
# Default: direct-head partitioned FFT convolution.
# Other examples:
#   make CPP_IR_CONV=ir_conv.cpp
#   make CPP_IR_CONV=ir_conv_fft.cpp
CPP_IR_CONV ?= ir_conv_fft_partitioned.cpp
CPP_SOURCES = main.cpp $(CPP_IR_CONV)
override CPP_SOURCES += mesa_power.cpp
TARGET ?= $(basename $(notdir $(CPP_IR_CONV)))
MESA_POWER_ENABLE ?= 0
MESA_POWER_PRESENCE_PIN ?= -1
MESA_POWER_POST_IR_PRESENCE ?= 0
MESA_POWER_PICK_ATTACK_PIN ?= -1
ENABLE_OUTPUT_STAGE_PIN ?= -1

# Library Locations
LIBDAISY_DIR ?= ./libDaisy
DAISYSP_DIR ?= ./DaisySP
CMSIS_DIR ?= $(LIBDAISY_DIR)/Drivers/CMSIS

# if USE_ARM_DSP symbol is defined, ARM optimized implementation from the 
# CMSIS library is used. Add neccessary sources to the list
C_DEFS += -DUSE_ARM_DSP
C_DEFS += -DMESA_POWER_ENABLE=$(MESA_POWER_ENABLE)
C_DEFS += -DMESA_POWER_PRESENCE_PIN=$(MESA_POWER_PRESENCE_PIN)
C_DEFS += -DMESA_POWER_POST_IR_PRESENCE=$(MESA_POWER_POST_IR_PRESENCE)
C_DEFS += -DMESA_POWER_PICK_ATTACK_PIN=$(MESA_POWER_PICK_ATTACK_PIN)
C_DEFS += -DENABLE_OUTPUT_STAGE_PIN=$(ENABLE_OUTPUT_STAGE_PIN)

ifeq ($(CPP_IR_CONV),ir_conv.cpp)
C_DEFS += -DIR_CONV_USE_DIRECT
else ifeq ($(CPP_IR_CONV),ir_conv_fft.cpp)
C_DEFS += -DIR_CONV_USE_FFT
else ifeq ($(CPP_IR_CONV),ir_conv_fft_partitioned.cpp)
C_DEFS += -DIR_CONV_USE_FFT_PARTITIONED
else
$(error Unsupported CPP_IR_CONV=$(CPP_IR_CONV))
endif
C_SOURCES = $(CMSIS_DIR)/DSP/Source/FilteringFunctions/arm_fir_f32.c   \
			$(CMSIS_DIR)/DSP/Source/FilteringFunctions/arm_fir_init_f32.c \
			$(CMSIS_DIR)/DSP/Source/TransformFunctions/arm_rfft_fast_f32.c \
			$(CMSIS_DIR)/DSP/Source/TransformFunctions/arm_cfft_f32.c \
			$(CMSIS_DIR)/DSP/Source/TransformFunctions/arm_cfft_radix8_f32.c \
			$(CMSIS_DIR)/DSP/Source/CommonTables/arm_common_tables.c \
			$(CMSIS_DIR)/DSP/Source/CommonTables/arm_const_structs.c
ASM_SOURCES += arm_bitreversal2.s

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

.DEFAULT_GOAL := rebuild

.PHONY: rebuild dfu flash
rebuild:
	$(MAKE) clean
	$(MAKE) all

dfu:
	@test -n "$(DFU_FILE)" || (echo "Usage: make dfu DFU_FILE=build/ir_conv_fft.bin"; exit 2)
	dfu-util -a 0 -s $(FLASH_ADDRESS):leave -D $(DFU_FILE) -d ,0483:$(USBPID)

flash:
	@set -- $(BUILD_DIR)/*.bin; \
	if [ "$$1" = "$(BUILD_DIR)/*.bin" ]; then \
		echo "No .bin file found in $(BUILD_DIR). Build first."; \
		exit 2; \
	fi; \
	if [ "$$#" -ne 1 ]; then \
		echo "Multiple .bin files found in $(BUILD_DIR). Use: make dfu DFU_FILE=<file>"; \
		exit 2; \
	fi; \
	$(MAKE) dfu DFU_FILE="$$1"
