#!/usr/bin/env node
"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs/promises");
const os = require("node:os");
const path = require("node:path");

const appRoot = path.resolve(__dirname, "..");
const hubRoot = process.argv[2];
const archive = path.resolve(
  process.argv[3] || path.join(appRoot, "dist/nx-glow-0.1.0-linux.tar.gz")
);

async function main() {
  if (!hubRoot) throw new Error("Usage: node tests/hub.cjs <nx-hub-repo> [archive.tar.gz]");

  const manifestApi = require(path.join(path.resolve(hubRoot), "src/main/manifest.js"));
  const discovery = require(path.join(path.resolve(hubRoot), "src/main/discovery.js"));
  const engine = require(path.join(path.resolve(hubRoot), "src/main/install/engine.js"));
  const raw = JSON.parse(await fs.readFile(path.join(appRoot, "nx-app.json"), "utf8"));
  const checked = manifestApi.validate(raw, { owner: "nerdrx", trusted: true });
  assert.equal(checked.ok, true, JSON.stringify(checked.problems));
  assert.equal(checked.trusted, true);
  const manifestArtifact = checked.manifest.artifacts.find((entry) =>
    discovery.globMatch(entry.assetPattern, path.basename(archive))
  );
  assert.ok(manifestArtifact, `manifest does not describe ${path.basename(archive)}`);
  assert.equal(manifestArtifact.kind, "tarball-prefix");
  assert.equal(manifestArtifact.platform, "linux");

  const temp = await fs.mkdtemp(path.join(os.tmpdir(), "nx-glow-hub-test-"));
  const prefix = path.join(temp, "prefix");
  const installRoot = path.join(temp, "hub-installs");
  const dataDir = path.join(temp, "hub-data");
  const artifact = {
    ...manifestArtifact,
    id: "tarball-prefix-linux",
    version: "0.1.0",
    assetName: path.basename(archive),
    prefix,
    launchCmd: path.join(prefix, "bin/nx-glow-settings"),
  };

  try {
    await fs.access(archive);
    await fs.mkdir(path.join(prefix, "share/nx-glow"), { recursive: true });
    const sentinel = path.join(prefix, "share/nx-glow/user-sentinel.txt");
    await fs.writeFile(sentinel, "keep me\n");

    const result = await engine.install({
      app: { id: "nx-glow" },
      artifact,
      filePath: archive,
      ctx: { installRoot, dataDir, settings: {}, log() {}, emitProgress() {} },
    });
    assert.equal(result.launchable, true, "Hub should recognize the settings launcher");
    assert.equal(result.path, path.join(installRoot, "nx/nx-glow/tarball-prefix-linux"));

    const expected = [
      "share/nx-glow/src/glow.cpp",
      "share/nx-glow/src/glow.json",
      "share/nx-glow/settings.py",
      "share/nx-glow/install.sh",
      "share/nx-glow/uninstall.sh",
      "bin/nx-glow-settings",
    ];
    for (const rel of expected) {
      const file = path.join(prefix, rel);
      assert.equal((await fs.stat(file)).isFile(), true, `missing installed payload: ${rel}`);
    }
    for (const rel of ["bin/nx-glow-settings", "share/nx-glow/install.sh", "share/nx-glow/uninstall.sh"]) {
      const st = await fs.stat(path.join(prefix, rel));
      assert.notEqual(st.mode & 0o111, 0, `not executable: ${rel}`);
    }

    await engine.uninstall({
      app: { id: "nx-glow" },
      artifact,
      installedPath: result.path,
      ctx: { installRoot, dataDir, settings: {}, log() {}, emitProgress() {} },
    });
    for (const rel of expected) {
      await assert.rejects(fs.access(path.join(prefix, rel)), { code: "ENOENT" }, `left installed: ${rel}`);
    }
    assert.equal(await fs.readFile(sentinel, "utf8"), "keep me\n", "Hub must preserve unrelated files");
    console.log("NX_GLOW_HUB_INSTALL_OK");
  } finally {
    await fs.rm(temp, { recursive: true, force: true });
  }
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
