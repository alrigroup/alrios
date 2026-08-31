/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

import { useState } from 'react'

export default function UserAvatar({ user, avatarUrl, name, username, size = 'md', className = '', onClick }) {
  const [hasError, setHasError] = useState(false)

  const rawUrl = avatarUrl || user?.avatar_url || user?.avatar || ''
  const displayName = name || user?.full_name || username || user?.username || user?.user || 'ALRI'
  const initial = (displayName.charAt(0) || 'A').toUpperCase()

  // Format CDN path: cdn.localhost:port or https://cdn.alrigroup.com
  let fullSrc = rawUrl
  if (rawUrl && rawUrl.startsWith('/')) {
    if (typeof window !== 'undefined') {
      const hostname = window.location.hostname
      if (hostname === 'localhost' || hostname === '127.0.0.1' || hostname.endsWith('.localhost')) {
        const port = window.location.port ? `:${window.location.port}` : ''
        fullSrc = `http://cdn.localhost${port}${rawUrl}`
      } else {
        fullSrc = `https://cdn.alrigroup.com${rawUrl}`
      }
    }
  }

  const sizeClasses = {
    xs: 'ent-avatar-xs',
    sm: 'ent-avatar-sm',
    md: 'ent-avatar-md',
    lg: 'ent-avatar-lg',
    xl: 'ent-avatar-xl'
  }

  const chosenSizeClass = sizeClasses[size] || sizeClasses.md

  return (
    <div
      className={`ent-avatar-container ${chosenSizeClass} ${className}`}
      onClick={onClick}
      title={displayName}
    >
      {fullSrc && !hasError ? (
        <img
          src={fullSrc}
          alt={displayName}
          className="ent-avatar-img"
          onError={() => setHasError(true)}
          loading="lazy"
        />
      ) : (
        <div className="ent-avatar-fallback">
          <span>{initial}</span>
        </div>
      )}
    </div>
  )
}
