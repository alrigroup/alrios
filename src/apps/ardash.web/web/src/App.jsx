import React, { useState, useEffect } from 'react'

export default function App() {
  const [session, setSession] = useState(null)
  const [loading, setLoading] = useState(true)
  const [overview, setOverview] = useState(null)
  const [charts, setCharts] = useState([])
  const [breakdown, setBreakdown] = useState([])

  const getApiUrl = (endpoint, port = 9680) => {
    const host = window.location.hostname
    const currentPort = window.location.port
    if (currentPort === '3015' || currentPort === '5173' || host === 'localhost' || host === '127.0.0.1') {
      return `http://${host}:${port}${endpoint}`
    }
    return endpoint
  }

  useEffect(() => {
    async function init() {
      try {
        const authRes = await fetch(getApiUrl('/arapi/auth/introspect', 9650), {
          credentials: 'include'
        })
        if (authRes.ok) {
          const authData = await authRes.json()
          if (authData.valid || authData.authenticated) {
            setSession(authData)

            const [ovRes, chRes, bkRes] = await Promise.all([
              fetch(getApiUrl('/arapi/dash/overview', 9680), { credentials: 'include' }),
              fetch(getApiUrl('/arapi/dash/charts', 9680), { credentials: 'include' }),
              fetch(getApiUrl('/arapi/dash/breakdown', 9680), { credentials: 'include' })
            ])

            if (ovRes.ok) setOverview(await ovRes.json())
            if (chRes.ok) setCharts(await chRes.json())
            if (bkRes.ok) setBreakdown(await bkRes.json())
          }
        }
      } catch (err) {
        console.warn('Analytics init error:', err)
      } finally {
        setLoading(false)
      }
    }
    init()
  }, [])

  const handleExport = () => {
    window.open(getApiUrl('/arapi/dash/export?format=csv', 9680), '_blank')
  }

  if (loading) {
    return (
      <div style={{ display: 'flex', height: '100vh', alignItems: 'center', justifyContent: 'center' }}>
        <div style={{ color: 'var(--text-muted)' }}>Carregando ALRI-Dashboard BI...</div>
      </div>
    )
  }

  const maxRevenue = Math.max(...charts.map(c => c.revenue), 1)

  return (
    <div className="dash-layout">
      {/* Top Header */}
      <header className="dash-header">
        <div className="dash-brand">
          <div className="dash-logo">📊</div>
          <div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <span className="dash-brand-title">ALRI-Dashboard</span>
              <span className="dash-badge">BI Executivo</span>
            </div>
          </div>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <button
            onClick={handleExport}
            style={{
              padding: '8px 16px',
              background: 'rgba(255,255,255,0.06)',
              border: '1px solid var(--border-subtle)',
              borderRadius: 'var(--radius-sm)',
              color: '#fff',
              fontWeight: 600,
              fontSize: '0.86rem',
              cursor: 'pointer'
            }}
          >
            📥 Exportar Relatório CSV
          </button>

          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '0.86rem' }}>
            <span style={{ fontWeight: 600 }}>{session?.user}</span>
            <span style={{ color: 'var(--accent-cyan)' }}>({session?.tenant?.toUpperCase()})</span>
          </div>
        </div>
      </header>

      {/* Main Analytics Content */}
      <main className="dash-container">
        {/* Row 1: Executive KPI Cards */}
        <section className="metrics-grid">
          <div className="metric-card" style={{ borderLeft: '4px solid var(--accent-emerald)' }}>
            <div className="metric-label">
              <span>Receita Mensal (MRR)</span>
              <span>📈</span>
            </div>
            <div className="metric-val">
              R$ {overview?.mrr?.toLocaleString('pt-BR', { minimumFractionDigits: 2 })}
            </div>
            <div className="metric-sub">+{overview?.revenue_growth}% vs mês anterior</div>
          </div>

          <div className="metric-card" style={{ borderLeft: '4px solid var(--accent-cyan)' }}>
            <div className="metric-label">
              <span>Run-Rate Anual (ARR)</span>
              <span>💼</span>
            </div>
            <div className="metric-val">
              R$ {overview?.arr?.toLocaleString('pt-BR', { minimumFractionDigits: 2 })}
            </div>
            <div className="metric-sub" style={{ color: '#a5b4fc' }}>Projeção 12 meses</div>
          </div>

          <div className="metric-card" style={{ borderLeft: '4px solid var(--primary)' }}>
            <div className="metric-label">
              <span>Disponibilidade dos Sistemas</span>
              <span>⚡</span>
            </div>
            <div className="metric-val">{overview?.system_uptime}%</div>
            <div className="metric-sub">SLA Garantido</div>
          </div>

          <div className="metric-card" style={{ borderLeft: '4px solid var(--accent-amber)' }}>
            <div className="metric-label">
              <span>Índice de Resolução (Tickets)</span>
              <span>🎧</span>
            </div>
            <div className="metric-val">{overview?.resolved_tickets_rate}%</div>
            <div className="metric-sub" style={{ color: 'var(--accent-amber)' }}>Tempo médio: 14 min</div>
          </div>
        </section>

        {/* Row 2: Charts & Breakdown */}
        <section className="charts-grid">
          {/* Monthly Revenue Chart */}
          <div className="chart-card">
            <div className="chart-title">
              <span>Evolução de Faturamento & Lucro Líquido (R$)</span>
              <span style={{ fontSize: '0.8rem', color: 'var(--text-dark)' }}>Últimos 6 meses</span>
            </div>

            <div className="bar-chart-container">
              {charts.map((c, i) => {
                const heightPercent = (c.revenue / maxRevenue) * 100
                return (
                  <div key={i} className="bar-group">
                    <div
                      className="bar-visual"
                      style={{ height: `${heightPercent}%` }}
                      title={`Receita: R$ ${c.revenue.toLocaleString()} | Lucro: R$ ${c.profit.toLocaleString()}`}
                    />
                    <span className="bar-label">{c.month}</span>
                  </div>
                )
              })}
            </div>

            <div style={{ display: 'flex', gap: '20px', marginTop: '18px', fontSize: '0.82rem', color: 'var(--text-muted)' }}>
              <span style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                <span style={{ width: '10px', height: '10px', background: 'var(--accent-emerald)', borderRadius: '2px' }} /> Receita Bruta
              </span>
              <span style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                <span style={{ width: '10px', height: '10px', background: 'var(--primary)', borderRadius: '2px' }} /> Margem Operacional (~73%)
              </span>
            </div>
          </div>

          {/* Subsidiary Distribution */}
          <div className="chart-card">
            <div className="chart-title">
              <span>Distribuição por Empresa</span>
              <span style={{ fontSize: '0.8rem', color: 'var(--text-dark)' }}>Participação</span>
            </div>

            <div className="breakdown-list">
              {breakdown.map((item) => (
                <div key={item.id} className="breakdown-item">
                  <div className="breakdown-header">
                    <span>{item.name}</span>
                    <span>{item.percentage}%</span>
                  </div>
                  <div className="progress-track">
                    <div className="progress-fill" style={{ width: `${item.percentage}%` }} />
                  </div>
                  <div style={{ fontSize: '0.78rem', color: 'var(--text-dark)', textAlign: 'right' }}>
                    R$ {item.mrr.toLocaleString('pt-BR', { minimumFractionDigits: 2 })} / mês
                  </div>
                </div>
              ))}
            </div>
          </div>
        </section>
      </main>
    </div>
  )
}
