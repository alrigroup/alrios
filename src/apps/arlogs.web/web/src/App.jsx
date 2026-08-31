import React, { useState, useEffect, useRef } from 'react'

export default function App() {
  const [session, setSession] = useState(null)
  const [loadingAuth, setLoadingAuth] = useState(true)

  const [logs, setLogs] = useState([])
  const [metrics, setMetrics] = useState({ total: 0, errors: 0, security: 0, services: {} })
  const [loadingLogs, setLoadingLogs] = useState(false)
  const [liveStream, setLiveStream] = useState(true)

  const [serviceFilter, setServiceFilter] = useState('all')
  const [severityFilter, setSeverityFilter] = useState('all')
  const [tenantFilter, setTenantFilter] = useState('all')
  const [searchQuery, setSearchQuery] = useState('')

  const timerRef = useRef(null)

  const getApiUrl = (endpoint, port = 9655) => {
    const host = window.location.hostname
    const currentPort = window.location.port
    if (currentPort === '3005' || currentPort === '5173' || host === 'localhost' || host === '127.0.0.1') {
      return `http://${host}:${port}${endpoint}`
    }
    return endpoint
  }

  // 1. Authenticate with ARAUTH SSO
  useEffect(() => {
    async function checkAuth() {
      try {
        const res = await fetch(getApiUrl('/arapi/auth/introspect', 9650), {
          method: 'GET',
          headers: { 'Content-Type': 'application/json' },
          credentials: 'include'
        })
        if (res.ok) {
          const data = await res.json()
          if (data.valid || data.authenticated) {
            setSession(data)
          }
        }
      } catch (err) {
        console.warn('Auth check fallback:', err)
      } finally {
        setLoadingAuth(false)
      }
    }
    checkAuth()
  }, [])

  // 2. Fetch Logs and Metrics
  const fetchLogsAndMetrics = async () => {
    try {
      // Build query string
      const params = new URLSearchParams()
      if (serviceFilter !== 'all') params.append('service', serviceFilter)
      if (severityFilter !== 'all') params.append('severity', severityFilter)
      if (tenantFilter !== 'all') params.append('tenant', tenantFilter)
      if (searchQuery.trim()) params.append('q', searchQuery.trim())
      params.append('limit', '200')

      const [logsRes, metricsRes] = await Promise.all([
        fetch(getApiUrl(`/arapi/logs/query?${params.toString()}`, 9655), {
          method: 'GET',
          headers: { 'Content-Type': 'application/json' },
          credentials: 'include'
        }),
        fetch(getApiUrl('/arapi/logs/metrics', 9655), {
          method: 'GET',
          headers: { 'Content-Type': 'application/json' },
          credentials: 'include'
        })
      ])

      if (logsRes.ok) {
        const logsData = await logsRes.json()
        setLogs(logsData)
      }
      if (metricsRes.ok) {
        const metricsData = await metricsRes.json()
        setMetrics(metricsData)
      }
    } catch (err) {
      console.warn('Error fetching logs:', err)
    }
  }

  useEffect(() => {
    fetchLogsAndMetrics()
  }, [serviceFilter, severityFilter, tenantFilter, searchQuery])

  // 3. Live polling interval
  useEffect(() => {
    if (liveStream) {
      timerRef.current = setInterval(() => {
        fetchLogsAndMetrics()
      }, 3000)
    } else {
      if (timerRef.current) clearInterval(timerRef.current)
    }
    return () => {
      if (timerRef.current) clearInterval(timerRef.current)
    }
  }, [liveStream, serviceFilter, severityFilter, tenantFilter, searchQuery])

  const exportToJson = () => {
    const dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(logs, null, 2))
    const dlAnchor = document.createElement('a')
    dlAnchor.setAttribute("href", dataStr)
    dlAnchor.setAttribute("download", `arlogs_export_${Date.now()}.json`)
    dlAnchor.click()
  }

  const exportToCsv = () => {
    if (logs.length === 0) return
    const headers = ["ID", "Timestamp", "User", "Tenant", "Service", "Action", "Severity", "Status", "IP", "Details"]
    const rows = logs.map(l => [
      l.id, `"${l.timestamp}"`, `"${l.user}"`, `"${l.tenant}"`, `"${l.service}"`,
      `"${l.action}"`, `"${l.severity}"`, l.status_code, `"${l.ip}"`, `"${(l.details || '').replace(/"/g, '""')}"`
    ])
    const csvContent = "data:text/csv;charset=utf-8," + [headers.join(","), ...rows.map(r => r.join(","))].join("\n")
    const dlAnchor = document.createElement('a')
    dlAnchor.setAttribute("href", encodeURI(csvContent))
    dlAnchor.setAttribute("download", `arlogs_export_${Date.now()}.csv`)
    dlAnchor.click()
  }

  if (loadingAuth) {
    return (
      <div style={{ display: 'flex', height: '100vh', alignItems: 'center', justifyContent: 'center' }}>
        <div style={{ color: 'var(--text-muted)', fontSize: '0.95rem' }}>Inicializando ARLOGS Console...</div>
      </div>
    )
  }

  return (
    <div className="app-layout">
      {/* Top Header */}
      <header className="app-header">
        <div className="header-brand">
          <div className="brand-icon">📜</div>
          <div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <span className="brand-title">ALRI-Logs</span>
              <span className="brand-tag">Auditoria Soberana</span>
            </div>
          </div>
        </div>

        <div className="header-actions">
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '0.8rem', color: 'var(--accent-emerald)' }}>
            <span className="pulse-dot"></span>
            <span>Telemetria em Tempo Real</span>
          </div>

          <div className="user-pill">
            <div className="user-avatar-sm">
              {session?.user ? session.user.substring(0, 2).toUpperCase() : 'AD'}
            </div>
            <span>{session?.user || 'Administrador'}</span>
            <span style={{ color: 'var(--text-dark)' }}>&bull;</span>
            <span style={{ color: '#a5b4fc', fontSize: '0.78rem' }}>{session?.tenant || 'Holding'}</span>
          </div>
        </div>
      </header>

      {/* Main App Container */}
      <main className="main-container">
        {/* Metric Cards Grid */}
        <section className="metrics-grid">
          <div className="metric-card">
            <div className="metric-label">
              <span>Total de Eventos Ingeridos</span>
              <span style={{ color: 'var(--primary)' }}>📊</span>
            </div>
            <div className="metric-val">{metrics.total.toLocaleString()}</div>
            <div className="metric-sub">Barramento ARDB & Telemetria</div>
          </div>

          <div className="metric-card">
            <div className="metric-label">
              <span>Alertas de Segurança</span>
              <span style={{ color: 'var(--accent-emerald)' }}>🛡️</span>
            </div>
            <div className="metric-val" style={{ color: 'var(--accent-emerald)' }}>
              {metrics.security.toLocaleString()}
            </div>
            <div className="metric-sub">Autenticação e Mutações de Acesso</div>
          </div>

          <div className="metric-card">
            <div className="metric-label">
              <span>Erros & Exceções</span>
              <span style={{ color: 'var(--accent-rose)' }}>⚠️</span>
            </div>
            <div className="metric-val" style={{ color: metrics.errors > 0 ? 'var(--accent-rose)' : 'var(--text-main)' }}>
              {metrics.errors.toLocaleString()}
            </div>
            <div className="metric-sub">HTTP 4xx/5xx e falhas de runtime</div>
          </div>

          <div className="metric-card">
            <div className="metric-label">
              <span>Isolamento de DMs</span>
              <span style={{ color: 'var(--accent-cyan)' }}>🔒</span>
            </div>
            <div className="metric-val" style={{ fontSize: '1.25rem', color: 'var(--accent-cyan)' }}>
              100% SIGILOSO
            </div>
            <div className="metric-sub">Mensagens ARCONN blindadas de log</div>
          </div>
        </section>

        {/* Filter and Action Controls */}
        <section className="control-bar">
          <div className="filter-group">
            <input
              type="text"
              className="search-input"
              placeholder="Buscar ação, usuário, IP ou detalhe..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
            />

            <select
              className="select-filter"
              value={serviceFilter}
              onChange={(e) => setServiceFilter(e.target.value)}
            >
              <option value="all">Todos os Serviços</option>
              <option value="arauth">ARAUTH (Identidade)</option>
              <option value="arbus">ARBUS (Gestão & RH)</option>
              <option value="arconn">ARCONN (Comunicação)</option>
              <option value="ardash">ARDASH (Dashboard BI)</option>
              <option value="archat">ARCHAT (Atendimento)</option>
              <option value="arcloud">ARCLOUD (Arquivos)</option>
              <option value="arctrl">ARCTRL (Servidores)</option>
              <option value="arstock">ARSTOCK (Estoque)</option>
              <option value="arwork">ARWORK (Hub Central)</option>
              <option value="arapilogs">ARLOGS (Auditoria)</option>
            </select>

            <select
              className="select-filter"
              value={severityFilter}
              onChange={(e) => setSeverityFilter(e.target.value)}
            >
              <option value="all">Todas as Severidades</option>
              <option value="INFO">INFO</option>
              <option value="WARN">WARN</option>
              <option value="ERROR">ERROR</option>
              <option value="SECURITY">SECURITY</option>
            </select>

            {session?.is_master && (
              <select
                className="select-filter"
                value={tenantFilter}
                onChange={(e) => setTenantFilter(e.target.value)}
              >
                <option value="all">Todas as Empresas (Holding)</option>
                <option value="alrigroup">ALRIGROUP Holding</option>
                <option value="detroitgg">Detroit GG</option>
              </select>
            )}
          </div>

          <div className="filter-group">
            <button
              type="button"
              className={`btn-icon ${liveStream ? 'active' : ''}`}
              onClick={() => setLiveStream(!liveStream)}
            >
              <span>{liveStream ? '🟢' : '⏸️'}</span>
              <span>{liveStream ? 'Live Ativo' : 'Pausado'}</span>
            </button>

            <button type="button" className="btn-icon" onClick={fetchLogsAndMetrics}>
              🔄 Atualizar
            </button>

            <button type="button" className="btn-icon" onClick={exportToCsv}>
              📥 Exportar CSV
            </button>

            <button type="button" className="btn-icon" onClick={exportToJson}>
              📦 JSON
            </button>
          </div>
        </section>

        {/* Live Logs Stream Table */}
        <section className="logs-table-container">
          <div className="logs-table-header">
            <span>Fluxo de Eventos ({logs.length} registros exibidos)</span>
            <span style={{ fontSize: '0.78rem' }}>Ordenado por: Mais Recente</span>
          </div>

          <div className="logs-list">
            {logs.length === 0 ? (
              <div style={{ padding: '40px 20px', textAlign: 'center', color: 'var(--text-muted)' }}>
                Nenhum evento registrado com os filtros selecionados.
              </div>
            ) : (
              logs.map((item) => (
                <div key={item.id} className="log-row">
                  <div style={{ color: 'var(--text-dark)', fontSize: '0.78rem' }}>
                    {item.timestamp}
                  </div>

                  <div>
                    <span className={`badge badge-${(item.severity || 'info').toLowerCase()}`}>
                      {item.severity}
                    </span>
                  </div>

                  <div className="service-tag">
                    {item.service}
                  </div>

                  <div className="tenant-tag tenant-col">
                    {item.tenant}
                  </div>

                  <div className="user-tag user-col">
                    {item.user}
                  </div>

                  <div className="details-cell" title={item.details}>
                    <strong style={{ color: 'var(--text-main)', marginRight: '6px' }}>{item.action}:</strong>
                    <span>{item.details}</span>
                  </div>

                  <div className={`status-cell status-${item.status_code >= 400 ? (item.status_code >= 500 ? '5xx' : '4xx') : '2xx'}`}>
                    {item.status_code}
                  </div>
                </div>
              ))
            )}
          </div>
        </section>
      </main>
    </div>
  )
}
