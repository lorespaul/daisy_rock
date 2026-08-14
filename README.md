# IR Convolution

## Getting the Source

First off, there are a few ways to clone and initialize the repo with its
submodules.

You can do either of the following:

```bash
git clone --recursive https://github.com/electro-smith/DaisyExamples
```

or:

```bash
git clone https://github.com/electro-smith/DaisyExamples
git submodule update --init
```

The firmware links against static libraries generated inside the `libDaisy` and
`DaisySP` submodules. After cloning, initialize and build them with:

```bash
tools/bootstrap.sh
```

Equivalent manual commands:

```bash
git submodule update --init --recursive
make -C libDaisy
make -C DaisySP
```

This example convolves the live stereo input, mixed to mono, with an imported
impulse response (IR). It is intended for low-latency guitar/cabinet-style use
on Daisy Seed.

Recommended workflow: use `tools/build_ir.js` to generate the IR header, then
compile the firmware with `make`.

```bash
node tools/build_ir.js path/to/ir.wav 2048
```

The script only writes `generated_ir.h`; it does not compile or flash firmware.

## Convert a WAV IR

Manual conversion is also available:

```bash
node tools/wav2ir.js path/to/ir.wav 2048
```

Supported generated IR lengths:

```txt
128, 256, 512, 1024, 2048, 4096
```

The C++ examples read the length from `kIrSize` in `generated_ir.h`; it is not
a compiler define.

If you convert manually, set `kIrSize` and paste the generated array into
`generated_ir.h`:

```cpp
static constexpr size_t kIrSize = 2048;
static const float ir[kIrSize] = {1.0f};
```

The IR is declared `static const`, so on STM32 it is placed in flash/rodata
rather than regular RAM.

## Which implementation to use

Each implementation can compile against any `kIrSize` from `generated_ir.h`.
For low-latency live guitar, this split is still a useful starting point:

```txt
short IRs  -> ir_conv.cpp
medium IRs -> ir_conv_fft.cpp
long IRs   -> ir_conv_fft_partitioned.cpp (default)
```

The FFT versions use 64-sample blocks/partitions. At 48 kHz this is about
1.33 ms of algorithmic latency.

## Generate an IR Header

Generate `generated_ir.h` from a WAV file:

```bash
node tools/build_ir.js path/to/ir.wav 128
node tools/build_ir.js path/to/ir.wav 256
node tools/build_ir.js path/to/ir.wav 512
node tools/build_ir.js path/to/ir.wav 1024
node tools/build_ir.js path/to/ir.wav 2048
node tools/build_ir.js path/to/ir.wav 4096
```

## Build commands

Default direct-head partitioned FFT convolution:

```bash
make
```

Direct convolution:

```bash
make CPP_IR_CONV=ir_conv.cpp
```

Direct-head FFT convolution:

```bash
make CPP_IR_CONV=ir_conv_fft.cpp
```

Direct-head partitioned FFT convolution:

```bash
make CPP_IR_CONV=ir_conv_fft_partitioned.cpp
```

The default build target runs `make clean` first, so switching generated IR
length or `CPP_IR_CONV` starts from a clean build directory.

Enable an output-stage mute/release pin after Daisy initialization:

```bash
make CPP_IR_CONV=ir_conv_fft_partitioned.cpp ENABLE_OUTPUT_STAGE_PIN=15
```

When `ENABLE_OUTPUT_STAGE_PIN` is left at the default `-1`, no output GPIO is
configured.

## Flash to Daisy Seed

Put the Daisy Seed in DFU/bootloader mode, then flash the already-built binary.
This does not compile first; it uploads the only `.bin` file in `build`.

After a clean build:

```bash
make flash
```

If you want to choose a binary explicitly:

```bash
make dfu DFU_FILE=build/ir_conv.bin
make dfu DFU_FILE=build/ir_conv_fft.bin
make dfu DFU_FILE=build/ir_conv_fft_partitioned.bin
```

The original libDaisy `program-dfu` target is still available if you prefer to
flash the binary selected by `TARGET`.

## Notes

- `ir_conv.cpp` has effectively zero algorithmic latency, but CPU cost grows
  linearly with IR length.
- `ir_conv_fft.cpp` uses a 64-sample direct head plus an RFFT tail for medium
  IRs.
- `ir_conv_fft_partitioned.cpp` uses a 64-sample direct head plus an RFFT
  partitioned tail. The first 64 IR samples are processed immediately per
  sample; the remaining IR tail is processed in 64-sample FFT partitions.
- A 32-sample FFT block would lower latency further, but it roughly doubles
  block processing frequency and increases CPU pressure.
