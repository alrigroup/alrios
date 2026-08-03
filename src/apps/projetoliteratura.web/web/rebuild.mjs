/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

import { existsSync, writeFileSync, rmSync } from 'node:fs'
import { spawnSync } from 'node:child_process'

const npmCmd = process.platform === 'win32' ? 'npm.cmd' : 'npm'
const useShell = process.platform === 'win32'

const run = (cmd, args) => {
  const r = spawnSync(cmd, args, { stdio: 'inherit', shell: useShell })
  if (r.status !== 0) process.exit(r.status ?? 1)
}

if (!existsSync('node_modules')) {
  run(npmCmd, ['ci', '--no-audit', '--no-fund'])
}
run(npmCmd, ['run', 'build'])

const thirdPartyNotices = `========================================================================
THIRD-PARTY NOTICES AND LICENSES
========================================================================

This distribution includes third-party software components subject to the terms and
conditions of their respective licenses:

1. React / React DOM
   License: MIT License
   Copyright (c) Meta Platforms, Inc. and affiliates.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

========================================================================
`

if (existsSync('dist')) {
  writeFileSync('dist/THIRD_PARTY_LICENSES.txt', thirdPartyNotices, 'utf-8')
}

// Mandatory cleanup: delete node_modules immediately
for (let attempts = 0; attempts < 5; attempts++) {
  if (!existsSync('node_modules')) break
  try {
    rmSync('node_modules', { recursive: true, force: true, maxRetries: 5, retryDelay: 200 })
  } catch (e) {}
}
