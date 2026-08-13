#!/usr/bin/env node

const childProcess = require("child_process");
const fs = require("fs");
const path = require("path");
const { fft } = require("./wav2ir.js");

const SAMPLE_RATE_HZ = 48000;
const MAX_OUTPUT_FREQ_HZ = SAMPLE_RATE_HZ / 2;
const DB_FLOOR_MAGNITUDE = 1e-20;

function usage() {
  const script = path.basename(process.argv[1]);
  console.error(
    `Usage: node ${script} <file.wav> <128|256|512|1024|2048|4096> [output.csv] [truncate|modeling] [--allow-sample-rate-mismatch]`,
  );
}

function fail(message) {
  console.error(`Error: ${message}`);
  usage();
  process.exit(1);
}

function defaultOutputPath(wavPath) {
  const parsed = path.parse(wavPath);
  return path.join(parsed.dir, `${parsed.name}_fft.csv`);
}

function parseIrValues(wav2irOutput) {
  const match = wav2irOutput.match(/static\s+const\s+float\s+ir\s*\[\s*\d+\s*\]\s*=\s*\{([\s\S]*?)\};/);
  if (!match) {
    fail("could not find generated IR array in wav2ir output");
  }

  return match[1]
    .split(",")
    .map((value) => value.trim().replace(/f$/i, ""))
    .filter(Boolean)
    .map((value) => {
      const number = Number(value);
      if (!Number.isFinite(number)) {
        fail(`invalid generated sample value: ${value}`);
      }
      return number;
    });
}

const [, , wavPath, sizeArg, ...args] = process.argv;

if (!wavPath || !sizeArg) {
  fail("missing arguments");
}

let outputPath = null;
let resampleMethod = "truncate";
let hasResampleMethod = false;
let allowSampleRateMismatch = false;

for (const arg of args) {
  if (arg === "--allow-sample-rate-mismatch") {
    allowSampleRateMismatch = true;
  } else if (arg.startsWith("--")) {
    fail(`unknown option: ${arg}`);
  } else if (arg.toLowerCase().endsWith(".csv") && !outputPath) {
    outputPath = arg;
  } else if (!hasResampleMethod) {
    resampleMethod = arg;
    hasResampleMethod = true;
  } else {
    fail(`unexpected argument: ${arg}`);
  }
}

if (!outputPath) {
  outputPath = defaultOutputPath(wavPath);
}

const wav2irArgs = [path.join(__dirname, "wav2ir.js"), wavPath, sizeArg];
if (resampleMethod) {
  wav2irArgs.push(resampleMethod);
}
if (allowSampleRateMismatch) {
  wav2irArgs.push("--allow-sample-rate-mismatch");
}

const result = childProcess.spawnSync(process.execPath, wav2irArgs, {
  encoding: "utf8",
  stdio: ["ignore", "pipe", "pipe"],
});

if (result.error) {
  fail(`wav2ir failed: ${result.error.message}`);
}

if (result.status !== 0) {
  if (result.stderr) {
    process.stderr.write(result.stderr);
  }
  process.exit(result.status ?? 1);
}

const values = parseIrValues(result.stdout);
const real = values.slice();
const imag = Array(values.length).fill(0);

fft(real, imag);

const halfSize = values.length / 2;
const bins = [];
for (let bin = 0; bin <= halfSize; bin++) {
  const freq = (bin * (SAMPLE_RATE_HZ / 2)) / halfSize;
  if (freq > MAX_OUTPUT_FREQ_HZ) {
    break;
  }

  const magnitude = Math.hypot(real[bin], imag[bin]);
  bins.push({ freq, magnitude });
}

function magnitudeToDb(magnitude) {
  return 20 * Math.log10(Math.max(magnitude, DB_FLOOR_MAGNITUDE));
}

const rows = ["freq,fft"];
for (const { freq, magnitude } of bins) {
  rows.push(`${freq},${magnitudeToDb(magnitude)}`);
}

fs.writeFileSync(outputPath, `${rows.join("\n")}\n`);
console.log(`Wrote ${bins.length} row(s) to ${outputPath}`);
