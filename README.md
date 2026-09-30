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

## Updating Submodules

Submodule URLs are stored in `.gitmodules`, but the exact checked-out commit is
stored by the parent repository as a gitlink. You can inspect the pinned commits
with:

```bash
git ls-files --stage DaisySP libDaisy stmlib
git submodule status --recursive
```

To update a submodule intentionally, check out the desired tag, branch, or
commit inside the submodule, rebuild the dependencies, test the firmware, then
stage the updated gitlink from the parent repository:

```bash
cd libDaisy
git fetch --tags
git checkout <tag-or-commit>
cd ..

cd DaisySP
git fetch --tags
git checkout <tag-or-commit>
cd ..

tools/bootstrap.sh
make
git add libDaisy DaisySP
```

Use the same pattern for `stmlib` if its version changes. Avoid relying on
automatic floating updates for normal builds; explicit updates keep the firmware
build reproducible from a clean clone.

This example convolves the live stereo input, mixed to mono, with an imported
impulse response (IR). It is intended for low-latency guitar/cabinet-style use
on Daisy Seed.

Recommended workflow: use `tools/build_ir.js` to generate the IR header, then
compile the firmware with `make`.

```bash
node tools/build_ir.js path/to/first.wav path/to/second.wav 4096
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

`generated_ir.h` contains only the float array. The tracked `ir.h` includes it,
derives `kIrSize` and `kIrCount`, and handles the QSPI-to-SDRAM copy. The IR
length is not a compiler define.

If you convert manually, paste only the array into `generated_ir.h`:

```cpp
static const float irs_qspi[][2048] IR_QSPI_STORAGE = {{1.0f}};
```

### QSPI memory map

The Seed's external QSPI flash spans `0x90000000-0x907FFFFF` (8 MiB):

| QSPI addresses | Size | Contents |
| --- | ---: | --- |
| `0x90000000-0x9003FFFF` | 256 KiB | Left free by the Daisy bootloader; the bootloader code is in internal flash. |
| `0x90040000-0x900B7FFF` | 480 KiB | Firmware image copied to internal SRAM at boot. Bytes after the actual firmware are `0xFF` padding in the combined `.bin`. |
| `0x900B8000-0x9045FFFF` | 3,744 KiB | IR bank, starting immediately after the SRAM copy area. Its size depends on the generated IR count. |
| `0x90460000-0x907FEFFF` | 3,708 KiB | Reserved for future persistent data, allocated from the end backward. |
| `0x907FF000-0x907FFFFF` | 4 KiB | Final erase sector: selected IR index. The value itself uses only a few bytes. |

`make` packs firmware and IRs into one contiguous image for USB DFU. The
`0x90460000` boundary is aligned to the bootloader's 64 KiB erase sectors;
the build rejects an IR bank that reaches the persistent area. The selection
address is the QSPI base `0x90000000` plus the `0x7FF000` offset passed to
`PersistentStorage::Init()`. After `hw.Init()` initializes SDRAM,
`convolver.Init()` copies the IR bank from QSPI to SDRAM. Convolution reads
`irs` from SDRAM.
All WAVs are converted to the same `kIrSize` and keep their command-line order.

To select the next IR with a momentary button wired from a Seed pin to GND:

```bash
make IR_SELECTOR_PIN=14
```

The button uses the internal pull-up. The selected index is saved in the last
4 KiB sector of the Seed's external QSPI flash and restored at startup; an
invalid saved index falls back to 0. The default `BOOT_SRAM` build executes
from SRAM, so the QSPI can store this setting. Changing IR briefly
stops audio while the FFT and flash are updated.
The delay and spring reverb sample buffers also live in SDRAM.

## Which implementation to use

Each implementation can compile against any `kIrSize` derived in `ir.h`.
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

Default direct-head partitioned FFT convolution (`APP_TYPE=BOOT_SRAM`):

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

Enable the guitar delay controls:

```bash
make \
  DELAY_ENABLE_PIN=16 \
  DELAY_LEVEL_PIN=17 \
  DELAY_TIME_PIN=18 \
  DELAY_FEEDBACK_PIN=19
```

`DELAY_ENABLE_PIN` uses the internal pull-up: connect it to GND to bypass the
delay; leave it open to enable it. `DELAY_LEVEL_PIN`, `DELAY_TIME_PIN`, and `DELAY_FEEDBACK_PIN` are ADC
inputs for potentiometers. The delay is enabled only when all four delay pins
are set.

Enable the spring reverb controls:

```bash
make REVERB_ENABLE_PIN=20 REVERB_LEVEL_PIN=21
```

`REVERB_ENABLE_PIN` uses the internal pull-up: connect it to GND to bypass the
reverb; leave it open to enable it. `REVERB_LEVEL_PIN` is an ADC input for the
wet level: the minimum setting retains a subtle reverb, while the maximum gives
a fuller tank effect. Both pins are required.

## Flash to Daisy Seed

Install the standard Daisy bootloader once: put the Seed in STM32 DFU mode
(hold BOOT while pressing RESET), then run:

```bash
make program-boot
```

This replaces the previous internal-flash firmware. For later updates, compile
with the same `make` command as before. It deletes and recreates `build/`, then
prepares the complete image for flashing. The image name follows `CPP_IR_CONV`.
Press RESET without holding BOOT. While the Daisy bootloader LED is pulsing, run:

```bash
make flash
```

Press BOOT during the pulsing window if you need more time; this extends the
Daisy bootloader's DFU window. Holding BOOT while pressing RESET enters the
STM32 DFU mode used only for installing the Daisy bootloader. `make flash`
uploads the prepared image and does not compile during the DFU window.

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


## Latest build
`$ make CPP_IR_CONV=ir_conv_fft_partitioned.cpp ENABLE_OUTPUT_STAGE_PIN=16 REVERB_ENABLE_PIN=14 REVERB_LEVEL_PIN=15 DELAY_ENABLE_PIN=13 DELAY_LEVEL_PIN=17 DELAY_TIME_PIN=18 DELAY_FEEDBACK_PIN=19 IR_SELECTOR_PIN=12`
`$ make flash`
