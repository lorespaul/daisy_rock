#!/usr/bin/env node

const fs = require("fs");
const path = require("path");

const VALID_SIZES = new Set([128, 256, 512, 1024, 2048, 4096]);
const MIN_FADE_OUT_SAMPLES = 8;
const MODELING_ITERATIONS = 32;

function usage() {
  const script = path.basename(process.argv[1]);
  console.error(
    `Usage: node ${script} <file.wav> <128|256|512|1024|2048|4096> [truncate|modeling] [--allow-sample-rate-mismatch]`,
  );
}

function fail(message) {
  console.error(`Error: ${message}`);
  usage();
  process.exit(1);
}

function readAscii(buffer, offset, length) {
  return buffer.toString("ascii", offset, offset + length);
}

function findChunks(buffer) {
  if (buffer.length < 12 || readAscii(buffer, 0, 4) !== "RIFF" || readAscii(buffer, 8, 4) !== "WAVE") {
    fail("input is not a RIFF/WAVE file");
  }

  const chunks = new Map();
  let offset = 12;

  while (offset + 8 <= buffer.length) {
    const id = readAscii(buffer, offset, 4);
    const size = buffer.readUInt32LE(offset + 4);
    const start = offset + 8;
    const end = start + size;

    if (end > buffer.length) {
      fail(`chunk '${id}' extends past end of file`);
    }

    if (!chunks.has(id)) {
      chunks.set(id, { start, size });
    }

    offset = end + (size % 2);
  }
  return chunks;
}

function parseFmt(buffer, chunk) {
  if (!chunk || chunk.size < 16) {
    fail("missing or invalid 'fmt ' chunk");
  }

  const offset = chunk.start;
  let audioFormat = buffer.readUInt16LE(offset);

  if (audioFormat === 0xfffe && chunk.size >= 40) {
    const subFormatCode = buffer.readUInt16LE(offset + 24);
    if (subFormatCode === 1 || subFormatCode === 3) {
      audioFormat = subFormatCode;
    }
  }

  return {
    audioFormat,
    channels: buffer.readUInt16LE(offset + 2),
    sampleRate: buffer.readUInt32LE(offset + 4),
    blockAlign: buffer.readUInt16LE(offset + 12),
    bitsPerSample: buffer.readUInt16LE(offset + 14),
  };
}

function readSample(buffer, offset, audioFormat, bitsPerSample) {
  if (audioFormat === 3) {
    if (bitsPerSample === 32) {
      return buffer.readFloatLE(offset);
    }
    if (bitsPerSample === 64) {
      return buffer.readDoubleLE(offset);
    }
    fail(`unsupported IEEE float bit depth: ${bitsPerSample}`);
  }

  if (audioFormat !== 1) {
    fail(`unsupported WAV format: ${audioFormat}. Only PCM and IEEE float are supported`);
  }

  switch (bitsPerSample) {
    case 8:
      return (buffer.readUInt8(offset) - 128) / 128;
    case 16:
      return buffer.readInt16LE(offset) / 32768;
    case 24:
      return buffer.readIntLE(offset, 3) / 8388608;
    case 32:
      return buffer.readInt32LE(offset) / 2147483648;
    default:
      fail(`unsupported PCM bit depth: ${bitsPerSample}`);
  }
}

function wavToMonoSamples(buffer) {
  const chunks = findChunks(buffer);
  const fmt = parseFmt(buffer, chunks.get("fmt "));
  const data = chunks.get("data");

  if (!data || data.size === 0) {
    fail("missing or empty 'data' chunk");
  }
  if (fmt.channels < 1) {
    fail("WAV file has no channels");
  }
  if (fmt.blockAlign <= 0) {
    fail("invalid block alignment");
  }

  const bytesPerSample = fmt.bitsPerSample / 8;
  if (!Number.isInteger(bytesPerSample)) {
    fail(`invalid bit depth: ${fmt.bitsPerSample}`);
  }

  const expectedBlockAlign = bytesPerSample * fmt.channels;
  if (fmt.blockAlign < expectedBlockAlign) {
    fail("invalid WAV block alignment for channel count and bit depth");
  }

  const frameCount = Math.floor(data.size / fmt.blockAlign);
  const samples = new Array(frameCount);

  for (let frame = 0; frame < frameCount; frame++) {
    const frameOffset = data.start + frame * fmt.blockAlign;
    let sum = 0;

    for (let channel = 0; channel < fmt.channels; channel++) {
      const sampleOffset = frameOffset + channel * bytesPerSample;
      sum += readSample(buffer, sampleOffset, fmt.audioFormat, fmt.bitsPerSample);
    }

    samples[frame] = sum / fmt.channels;
  }

  return { samples, fmt };
}

function formatFloat(value) {
  if (!Number.isFinite(value)) {
    return "0.0f";
  }

  const normalized = Math.abs(value) < 1e-20 ? 0 : value;
  let text = normalized.toPrecision(9);

  if (!/[.eE]/.test(text)) {
    text += ".0";
  }

  return `${text}f`;
}

function fft(real, imag, inverse = false) {
  const n = real.length;
  if (n === 0 || (n & (n - 1)) !== 0) {
    fail("FFT input length must be a power of two");
  }

  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;

    if (i < j) {
      [real[i], real[j]] = [real[j], real[i]];
      [imag[i], imag[j]] = [imag[j], imag[i]];
    }
  }

  for (let length = 2; length <= n; length <<= 1) {
    const angle = ((inverse ? 2 : -2) * Math.PI) / length;
    const wLenReal = Math.cos(angle);
    const wLenImag = Math.sin(angle);

    for (let i = 0; i < n; i += length) {
      let wReal = 1;
      let wImag = 0;

      for (let j = 0; j < length / 2; j++) {
        const uReal = real[i + j];
        const uImag = imag[i + j];
        const vReal = real[i + j + length / 2] * wReal - imag[i + j + length / 2] * wImag;
        const vImag = real[i + j + length / 2] * wImag + imag[i + j + length / 2] * wReal;

        real[i + j] = uReal + vReal;
        imag[i + j] = uImag + vImag;
        real[i + j + length / 2] = uReal - vReal;
        imag[i + j + length / 2] = uImag - vImag;

        const nextWReal = wReal * wLenReal - wImag * wLenImag;
        wImag = wReal * wLenImag + wImag * wLenReal;
        wReal = nextWReal;
      }
    }
  }

  if (inverse) {
    for (let i = 0; i < n; i++) {
      real[i] /= n;
      imag[i] /= n;
    }
  }
}

function nextPowerOfTwo(value) {
  let result = 1;
  while (result < value) {
    result <<= 1;
  }
  return result;
}

function fadeOutLength(size) {
  return Math.max(MIN_FADE_OUT_SAMPLES, size / 32);
}

function applyFadeOut(values, activeLength) {
  const fadeLength = Math.min(fadeOutLength(values.length), activeLength);
  for (let i = 0; i < fadeLength; i++) {
    const index = activeLength - fadeLength + i;
    const normalized = fadeLength === 1 ? 1 : i / (fadeLength - 1);
    const gain = 0.5 * (1 + Math.cos(Math.PI * normalized));
    values[index] *= gain;
  }
}

function resampleTruncate(samples, size) {
  const result = Array(size).fill(0);
  const copyLength = Math.min(samples.length, size);

  for (let i = 0; i < copyLength; i++) {
    result[i] = samples[i];
  }

  applyFadeOut(result, copyLength);

  return result;
}

function resampleModelingApproximation(samples, size) {
  const fftSize = nextPowerOfTwo(Math.max(samples.length, size));
  const targetReal = Array(fftSize).fill(0);
  const targetImag = Array(fftSize).fill(0);
  const real = Array(fftSize).fill(0);
  const imag = Array(fftSize).fill(0);

  for (let i = 0; i < samples.length; i++) {
    targetReal[i] = samples[i];
  }
  for (let i = 0; i < Math.min(samples.length, size); i++) {
    real[i] = samples[i];
  }

  fft(targetReal, targetImag);
  const targetMagnitude = targetReal.map((realValue, index) => Math.hypot(realValue, targetImag[index]));

  for (let iteration = 0; iteration < MODELING_ITERATIONS; iteration++) {
    fft(real, imag);

    for (let i = 0; i < fftSize; i++) {
      const magnitude = Math.hypot(real[i], imag[i]);
      if (magnitude < 1e-20) {
        real[i] = targetMagnitude[i];
        imag[i] = 0;
      } else {
        const scale = targetMagnitude[i] / magnitude;
        real[i] *= scale;
        imag[i] *= scale;
      }
    }

    fft(real, imag, true);

    for (let i = 0; i < fftSize; i++) {
      if (i >= size) {
        real[i] = 0;
      }
      imag[i] = 0;
    }
  }

  const result = real.slice(0, size);
  applyFadeOut(result, Math.min(samples.length, size));
  return result;
}

function printArray(values, samples, size, sourcePath, fmt) {
  console.log(`// Source: ${sourcePath}`);
  console.log(`// WAV: ${fmt.channels} channel(s), ${fmt.sampleRate} Hz, ${fmt.bitsPerSample}-bit`);
  console.log(`// Resampled from ${samples.length} sample(s) to ${size} sample(s)`);
  console.log(`static const float ir[${size}] = {`);

  for (let i = 0; i < values.length; i += 8) {
    const line = values.slice(i, i + 8).map(formatFloat).join(", ");
    console.log(`    ${line}${i + 8 < values.length ? "," : ""}`);
  }

  console.log("};");
}

function main() {
  const [, , wavPath, sizeArg, ...args] = process.argv;

  if (!wavPath || !sizeArg) {
    fail("missing arguments");
  }

  let resampleMethod = "truncate";
  let hasResampleMethod = false;
  let allowSampleRateMismatch = false;

  for (const arg of args) {
    if (arg === "--allow-sample-rate-mismatch") {
      allowSampleRateMismatch = true;
    } else if (arg.startsWith("--")) {
      fail(`unknown option: ${arg}`);
    } else if (!hasResampleMethod) {
      resampleMethod = arg;
      hasResampleMethod = true;
    } else {
      fail(`unexpected argument: ${arg}`);
    }
  }

  const size = Number(sizeArg);
  if (!VALID_SIZES.has(size)) {
    fail("size must be one of: 128, 256, 512, 1024, 2048, 4096");
  }

  let wavBuffer;
  try {
    wavBuffer = fs.readFileSync(wavPath);
  } catch (error) {
    fail(`cannot read '${wavPath}': ${error.message}`);
  }

  const { samples, fmt } = wavToMonoSamples(wavBuffer);
  if (fmt.sampleRate !== 48000 && !allowSampleRateMismatch) {
    fail(
      `WAV sample rate is ${fmt.sampleRate} Hz, expected 48000 Hz. ` +
        "Pass --allow-sample-rate-mismatch to bypass this check.",
    );
  }

  let values;
  switch (resampleMethod) {
    case "truncate":
    case "resample-truncate":
      values = resampleTruncate(samples, size);
      break;
    case "modeling":
    case "approximation":
    case "modeling-approximation":
      values = resampleModelingApproximation(samples, size);
      break;
    default:
      fail(`unsupported conversion method: ${resampleMethod}`);
  }
  printArray(values, samples, size, wavPath, fmt);
}

if (require.main === module) {
  main();
}

module.exports = {
  fft,
};
