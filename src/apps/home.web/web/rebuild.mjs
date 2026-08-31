/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

import { existsSync, writeFileSync, readFileSync, rmSync } from 'node:fs'
import { spawnSync } from 'node:child_process'
import { resolve, join } from 'node:path'

let npmCmd = process.platform === 'win32' ? 'npm.cmd' : 'npm'
const useShell = process.platform === 'win32'

// Auto-discover npm in known paths if not found in current PATH
if (process.platform !== 'win32') {
  const candidatePaths = [
    '/home/alexsar/.local/bin/npm',
    resolve('../../../arcore/programfiles/npm/bin/npm'),
    resolve('../../../arcore/programfiles/node/bin/npm'),
    '/usr/local/bin/npm',
    '/usr/bin/npm'
  ]
  for (const cand of candidatePaths) {
    if (existsSync(cand)) {
      npmCmd = cand
      break
    }
  }
}

const run = (cmd, args) => {
  const env = { ...process.env }
  const extraPaths = [
    '/home/alexsar/.local/bin',
    resolve('../../../arcore/programfiles/node/bin'),
    resolve('../../../arcore/programfiles/npm/bin')
  ]
  env.PATH = `${extraPaths.join(':')}:${env.PATH || ''}`
  const r = spawnSync(cmd, args, { stdio: 'inherit', shell: useShell, env })
  if (r.status !== 0) process.exit(r.status ?? 1)
}

if (!existsSync('node_modules') || !existsSync('node_modules/.bin/vite')) {
  run(npmCmd, ['ci', '--no-audit', '--no-fund'])
}
run(npmCmd, ['run', 'build'])

// Copy index.html -> main.arhtml for ARWN unit entry
const dist = resolve('dist')
if (existsSync(join(dist, 'index.html'))) {
  writeFileSync(join(dist, 'main.arhtml'), readFileSync(join(dist, 'index.html')), 'utf-8')
}

// Cleanup: delete node_modules (dist/ is preserved so arwn_build can read it next)
for (let attempts = 0; attempts < 5; attempts++) {
  if (!existsSync('node_modules')) break
  try {
    rmSync('node_modules', { recursive: true, force: true, maxRetries: 5, retryDelay: 200 })
  } catch (e) {}
}
