#pragma once

#include <cstddef>
#include <cstring>

#if defined(__arm__)
#define IR_QSPI_STORAGE __attribute__((section(".qspiflash_data")))
#define IR_SDRAM_STORAGE __attribute__((section(".sdram_bss")))
#else
#define IR_QSPI_STORAGE
#define IR_SDRAM_STORAGE
#endif

#if __has_include("generated_ir.h")
#include "generated_ir.h"
#else
static const float irs_qspi[][128] IR_QSPI_STORAGE = {{1.0f}};
#endif

static constexpr size_t kIrCount = sizeof(irs_qspi) / sizeof(irs_qspi[0]);
static constexpr size_t kIrSize = sizeof(irs_qspi[0]) / sizeof(irs_qspi[0][0]);
static float irs[kIrCount][kIrSize] IR_SDRAM_STORAGE;

static inline void LoadIrs() { std::memcpy(irs, irs_qspi, sizeof irs); }

#undef IR_QSPI_STORAGE
#undef IR_SDRAM_STORAGE
