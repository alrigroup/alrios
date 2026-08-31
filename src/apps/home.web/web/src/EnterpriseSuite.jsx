/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

import { useEffect } from 'react'

export default function EnterpriseSuite() {
  useEffect(() => {
    window.location.href = '/restrict-area'
  }, [])

  return (
    <div style={{ minHeight: '100vh', display: 'flex', alignItems: 'center', justifyContent: 'center', background: '#050508', color: '#fff' }}>
      <p>Redirecionando para o ALRI Enterprise Suite...</p>
    </div>
  )
}
