/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import './projetoliteratura.css'
import ProjetoLiteratura from './ProjetoLiteratura'

createRoot(document.getElementById('root')).render(
  <StrictMode>
    <ProjetoLiteratura />
  </StrictMode>
)
