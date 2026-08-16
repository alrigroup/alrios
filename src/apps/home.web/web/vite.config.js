/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

const licenseHeader = `<!--
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 -->\n`

export default defineConfig({
  plugins: [
    react(),
    {
      name: 'alrigroup-license-header',
      transformIndexHtml(html) {
        return html.replace('<!DOCTYPE html>', `<!DOCTYPE html>\n${licenseHeader}`)
      }
    }
  ],
  esbuild: {
    legalComments: 'none'
  },
  build: {
    target: 'es2018',
    cssCodeSplit: false,
    emptyOutDir: true,
    rollupOptions: {
      output: {
        entryFileNames: 'main.js',
        chunkFileNames: 'main.js',
        assetFileNames: (assetInfo) => {
          if (assetInfo.name && assetInfo.name.endsWith('.css')) return 'main.css'
          return '[name][extname]'
        }
      }
    }
  }
})
