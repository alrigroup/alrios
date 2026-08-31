import React, { useState, useEffect } from 'react'

export default function App() {
  const [session, setSession] = useState(null)
  const [loading, setLoading] = useState(true)
  const [apps, setApps] = useState([])
  const [summary, setSummary] = useState(null)
  const [search, setSearch] = useState('')
  const [activeCategory, setActiveCategory] = useState('all')

  const getApiUrl = (endpoint, port = 9660) => {
    const host = window.location.hostname
    const currentPort = window.location.port
    if (currentPort === '3012' || currentPort === '5173' || host === 'localhost' || host === '127.0.0.1') {
      return `http://${host}:${port}${endpoint}`
    }
    return endpoint
  }

  const getAuthUrl = () => {
    const host = window.location.hostname
    if (host.includes('localhost') || host === '127.0.0.1') {
      return 'http://localhost:8080'
    }
    return 'https://alrigroup.com'
  }

  // 1. Check SSO session on load
  useEffect(() => {
    async function init() {
      try {
        const authRes = await fetch(getApiUrl('/arapi/auth/introspect', 9650), {
          method: 'GET',
          headers: { 'Content-Type': 'application/json' },
          credentials: 'include'
        })

        if (authRes.ok) {
          const authData = await authRes.json()
          if (authData.valid || authData.authenticated) {
            setSession(authData)

            // Fetch app catalog and summary
            const [catalogRes, sumRes] = await Promise.all([
              fetch(getApiUrl('/arapi/work/catalog', 9660), {
                method: 'GET',
                headers: { 'Content-Type': 'application/json' },
                credentials: 'include'
              }),
              fetch(getApiUrl('/arapi/work/summary', 9660), {
                method: 'GET',
                headers: { 'Content-Type': 'application/json' },
                credentials: 'include'
              })
            ])

            if (catalogRes.ok) {
              const catData = await catalogRes.json()
              if (catData.apps) setApps(catData.apps)
            }
            if (sumRes.ok) {
              const sumData = await sumRes.json()
              setSummary(sumData)
            }
          }
        }
      } catch (err) {
        console.warn('SSO check error:', err)
      } finally {
        setLoading(false)
      }
    }
    init()
  }, [])

  const handleLogout = async () => {
    try {
      await fetch(getApiUrl('/arapi/auth/logout', 9650), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'include'
      })
    } catch (e) {
      console.error(e)
    } finally {
      window.location.href = getAuthUrl()
    }
  }

  const getTargetUrl = (app) => {
    const host = window.location.hostname
    const portMap = {
      arwork: 3012,
      arbus: 3013,
      archat: 3014,
      ardash: 3015,
      arcloud: 3016,
      arconn: 3017,
      arstock: 3018,
      arctrl: 3019,
      arlogs: 3005,
      home: 3004,
      detroit: 3001
    }
    if (host.includes('localhost') || host === '127.0.0.1') {
      return app.local_url || `http://localhost:${portMap[app.id] || 3000}`
    }
    return app.url || `https://${app.id}.alrigroup.com`
  }

  // Filter apps by category and search
  const filteredApps = apps.filter(app => {
    const matchCategory = activeCategory === 'all' ||
      (activeCategory === 'gestao' && app.category.includes('Gestão')) ||
      (activeCategory === 'comunicacao' && app.category.includes('Comunicação')) ||
      (activeCategory === 'analytics' && app.category.includes('Intelligence')) ||
      (activeCategory === 'arquivos' && app.category.includes('Arquivos')) ||
      (activeCategory === 'operacoes' && (app.category.includes('Estoque') || app.category.includes('Atendimento'))) ||
      (activeCategory === 'master' && app.badge === 'Master')

    const matchSearch = !search.trim() ||
      app.name.toLowerCase().includes(search.toLowerCase()) ||
      app.sigla.toLowerCase().includes(search.toLowerCase()) ||
      app.description.toLowerCase().includes(search.toLowerCase()) ||
      app.category.toLowerCase().includes(search.toLowerCase())

    return matchCategory && matchSearch
  })

  if (loading) {
    return (
      <div style={{ display: 'flex', height: '100vh', alignItems: 'center', justifyContent: 'center' }}>
        <div style={{ color: 'var(--text-muted)', fontSize: '1rem', fontWeight: 500 }}>
          Carregando Hub ALRI-Workspace...
        </div>
      </div>
    )
  }

  if (!session) {
    return (
      <div style={{ display: 'flex', height: '100vh', alignItems: 'center', justifyContent: 'center', textAlign: 'center', padding: '20px' }}>
        <div style={{ maxWidth: '420px', background: 'var(--bg-card)', padding: '40px', borderRadius: 'var(--radius-lg)', border: '1px solid var(--border-subtle)' }}>
          <div style={{ fontSize: '2.5rem', marginBottom: '16px' }}>🛡️</div>
          <h2 style={{ fontSize: '1.4rem', fontWeight: 700, marginBottom: '8px' }}>Autenticação Necessária</h2>
          <p style={{ color: 'var(--text-muted)', fontSize: '0.92rem', marginBottom: '24px' }}>
            Para acessar o catálogo de aplicativos e ferramentas corporativas do ecossistema ALRIGROUP, efetue login no ARAUTH.
          </p>
          <a
            href={getAuthUrl()}
            style={{ display: 'block', padding: '14px', background: 'var(--primary)', color: '#fff', textDecoration: 'none', borderRadius: 'var(--radius-md)', fontWeight: 600 }}
          >
            Entrar via ARAUTH SSO
          </a>
        </div>
      </div>
    )
  }

  return (
    <div className="workspace-layout">
      {/* Top Navigation */}
      <header className="work-header">
        <div className="work-brand">
          <div className="work-logo">🪐</div>
          <div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <span className="work-brand-title">ALRI-Workspace</span>
              <span className="work-badge">Hub Soberano</span>
            </div>
          </div>
        </div>

        <div className="work-header-right">
          {/* Company Selector */}
          <select className="company-select" defaultValue={session.tenant || 'alrigroup'}>
            <option value="alrigroup">ALRIGROUP (Holding)</option>
            <option value="detroitgg">Detroit GG</option>
          </select>

          {/* Profile & SSO Controls */}
          <div className="profile-pill" onClick={handleLogout} title="Clique para desconectar">
            <div className="profile-avatar">
              {session.user ? session.user.substring(0, 2).toUpperCase() : 'AR'}
            </div>
            <div>
              <span style={{ fontWeight: 600 }}>{session.user}</span>
              <span style={{ color: 'var(--text-dark)', marginLeft: '6px' }}>({session.role})</span>
            </div>
            <span style={{ fontSize: '0.8rem', color: 'var(--accent-rose)' }}>🚪</span>
          </div>
        </div>
      </header>

      {/* Main Workspace Portal */}
      <main className="work-container">
        <section className="hero-section">
          <h1 className="hero-greeting">
            Olá, {session.user}!
          </h1>
          <p className="hero-sub">
            Acesse abaixo as aplicações autorizadas para sua conta corporativa na <strong>{session.tenant ? session.tenant.toUpperCase() : 'ALRIGROUP'}</strong>.
          </p>

          {/* Search Box */}
          <div className="search-wrapper">
            <span className="search-icon">🔍</span>
            <input
              type="text"
              className="search-bar"
              placeholder="Pesquisar ferramentas, relatórios, chats ou serviços..."
              value={search}
              onChange={(e) => setSearch(e.target.value)}
            />
          </div>

          {/* Category Filter Chips */}
          <div className="category-chips">
            <button
              className={`chip ${activeCategory === 'all' ? 'active' : ''}`}
              onClick={() => setActiveCategory('all')}
            >
              Todas as Ferramentas ({apps.length})
            </button>
            <button
              className={`chip ${activeCategory === 'gestao' ? 'active' : ''}`}
              onClick={() => setActiveCategory('gestao')}
            >
              🏢 Gestão & RH
            </button>
            <button
              className={`chip ${activeCategory === 'comunicacao' ? 'active' : ''}`}
              onClick={() => setActiveCategory('comunicacao')}
            >
              💬 Comunicação & Kanban
            </button>
            <button
              className={`chip ${activeCategory === 'analytics' ? 'active' : ''}`}
              onClick={() => setActiveCategory('analytics')}
            >
              📊 BI & Financeiro
            </button>
            <button
              className={`chip ${activeCategory === 'operacoes' ? 'active' : ''}`}
              onClick={() => setActiveCategory('operacoes')}
            >
              📦 Estoque & Suporte
            </button>
            <button
              className={`chip ${activeCategory === 'arquivos' ? 'active' : ''}`}
              onClick={() => setActiveCategory('arquivos')}
            >
              ☁️ Arquivos
            </button>
            {session.is_master && (
              <button
                className={`chip ${activeCategory === 'master' ? 'active' : ''}`}
                onClick={() => setActiveCategory('master')}
              >
                ⚡ Master Infra
              </button>
            )}
          </div>
        </section>

        {/* Applications Grid */}
        <section className="apps-grid">
          {filteredApps.map((app) => (
            <a
              key={app.id}
              href={getTargetUrl(app)}
              className="app-card"
              target="_blank"
              rel="noopener noreferrer"
            >
              <div>
                <div className="app-card-top">
                  <div className="app-icon-box">{app.icon}</div>
                  <span className="app-tag">{app.badge}</span>
                </div>
                <h3 className="app-name">
                  <span>{app.name}</span>
                  <span style={{ color: 'var(--text-dark)', fontSize: '0.85rem' }}>({app.sigla})</span>
                </h3>
                <p className="app-desc">{app.description}</p>
              </div>

              <div className="app-card-footer">
                <span>Abrir Aplicação</span>
                <span>&rarr;</span>
              </div>
            </a>
          ))}
        </section>
      </main>
    </div>
  )
}
