// SPDX-License-Identifier: MIT
// AI-assisted deterministic adaptation of the pinned original replay parser.
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
function apply(root, checkOnly = false) {
    root = fs.realpathSync(root);
    const patch = JSON.parse(fs.readFileSync(path.join(root, 'tools/replay-parser-patch.json'), 'utf8'));
    const packageJSON = JSON.parse(fs.readFileSync(path.join(root, 'package.json'), 'utf8'));
    const lock = JSON.parse(fs.readFileSync(path.join(root, 'package-lock.json'), 'utf8'));
    assert.equal(packageJSON.dependencies[patch.package], patch.version, 'Parser version must be exact');
    assert.equal(lock.packages[''].dependencies[patch.package], patch.version, 'Root lock version differs');
    assert.deepEqual(lock.packages['node_modules/' + patch.package], patch.upstream, 'Registry package lock pin differs');
    const packageRoot = path.join(root, 'node_modules', patch.package);
    assert.equal(fs.realpathSync(packageRoot), packageRoot, 'Refuse external/symlinked dependency');
    assert.equal(JSON.parse(fs.readFileSync(path.join(packageRoot, 'package.json'), 'utf8')).version, patch.version);
    const target = path.join(packageRoot, patch.file);
    assert.equal(fs.realpathSync(target), target, 'Refuse symlinked parser source');
    const hash = value => crypto.createHash('sha256').update(value).digest('hex');
    assert.equal(hash(patch.replacement), patch.patchedSHA256, 'Replacement bytes differ');
    const current = hash(fs.readFileSync(target));
    if (current === patch.patchedSHA256) return { status: 'already-patched', sha256: current };
    assert.equal(current, patch.originalSHA256, 'Unexpected parser bytes; refusing overwrite');
    assert.equal(checkOnly, false, 'Replay parser patch missing: run the package postinstall patch step');
    fs.writeFileSync(target, patch.replacement, 'utf8');
    assert.equal(hash(fs.readFileSync(target)), patch.patchedSHA256);
    return { status: 'patched', sha256: patch.patchedSHA256 };
}
module.exports = { apply };
if (require.main === module) {
    assert(process.argv.slice(2).every(arg => arg === '--check'), 'Unknown argument');
    console.log(JSON.stringify(apply(path.resolve(__dirname, '..'), process.argv.includes('--check'))));
}
