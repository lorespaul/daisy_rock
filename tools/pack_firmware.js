#!/usr/bin/env node

const fs = require("fs");
const assert = require("assert");

const base = 0x90040000;

function pack(app, ir, address, limit) {
  const offset = address - base;
  if (app.length > 480 * 1024 || app.length > offset || !ir.length ||
      !Number.isInteger(offset) || !Number.isInteger(limit) || address + ir.length > limit) {
    throw new Error("firmware or IR bank exceeds its reserved QSPI area");
  }

  const output = Buffer.alloc(offset + ir.length, 0xff);
  app.copy(output);
  ir.copy(output, offset);
  return output;
}

if (process.argv[2] === "--self-test") {
  assert.deepStrictEqual(pack(Buffer.from([1]), Buffer.from([2]), base + 3, base + 4), Buffer.from([1, 255, 255, 2]));
  assert.throws(() => pack(Buffer.from([1]), Buffer.from([2]), base + 3, base + 3));
} else {
  const [appPath, irPath, outputPath, addressArg, limitArg] = process.argv.slice(2);
  fs.writeFileSync(outputPath, pack(fs.readFileSync(appPath), fs.readFileSync(irPath), Number(addressArg), Number(limitArg)));
}
