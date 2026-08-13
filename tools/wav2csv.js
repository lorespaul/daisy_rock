#!/usr/bin/env node

const childProcess = require("child_process");
const fs = require("fs");
const path = require("path");

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
  return path.join(parsed.dir, `${parsed.name}.csv`);
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

let outputPath = defaultOutputPath(wavPath);
let resampleMethod = "truncate";
let hasResampleMethod = false;
let allowSampleRateMismatch = false;

for (const arg of args) {
  if (arg === "--allow-sample-rate-mismatch") {
    allowSampleRateMismatch = true;
  } else if (arg.startsWith("--")) {
    fail(`unknown option: ${arg}`);
  } else if (arg.toLowerCase().endsWith(".csv") && outputPath === defaultOutputPath(wavPath)) {
    outputPath = arg;
  } else if (!hasResampleMethod) {
    resampleMethod = arg;
    hasResampleMethod = true;
  } else {
    fail(`unexpected argument: ${arg}`);
  }
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
const xStep =
  resampleMethod === "truncate" ||
  resampleMethod === "resample-truncate" ||
  resampleMethod === "modeling" ||
  resampleMethod === "approximation" ||
  resampleMethod === "modeling-approximation"
    ? 1
    : 4096 / values.length;
const rows = ["x,y", ...values.map((value, index) => `${index * xStep},${value}`)];

fs.writeFileSync(outputPath, `${rows.join("\n")}\n`);
console.log(`Wrote ${values.length} row(s) to ${outputPath}`);
