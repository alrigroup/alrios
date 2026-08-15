/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

import { readFileSync, writeFileSync } from 'node:fs'

const data = readFileSync('src/apps/home.web/home_web_data.h', 'utf8')
const m = data.match(/const translations = \{[\s\S]*?\};\s*\n/)
if (!m) {
  console.error('translations block not found')
  process.exit(1)
}
let body = m[0].replace(/\r\n/g, '\n').replace(/;\s*$/, '')
body += '\n\ntranslations.en.monitor_h2 = "Ecosystem Monitor";\n'
body += 'translations.en.monitor_sub = "Live dashboard rendered by Vue 3 inside the React SPA.";\n'
body += 'translations.pt.monitor_h2 = "Monitor do Ecossistema";\n'
body += 'translations.pt.monitor_sub = "Painel reativo renderizado por Vue 3 dentro do SPA React.";\n'
body += '\nexport default translations;\n'
writeFileSync('src/apps/home.web/web/src/i18n.js', body, 'utf8')
console.log('i18n.js gerado:', body.length, 'bytes')
