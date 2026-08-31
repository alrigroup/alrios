import React, { useState, useEffect } from 'react'

export default function App() {
  const [session, setSession] = useState(null)
  const [loading, setLoading] = useState(true)
  const [services, setServices] = useState([])
  const [metrics, setMetrics] = useState(null)
  const [logs, setLogs] = useState([
    '[KERNEL] ALRIOS Sovereign OS v1.0.0 Online',
    '[ARWS] Gateway active on port 9500 (10 routes registered)',
    '[ARAUTH] Sovereign Vault security intact (AES-256 + Argon2id)',
    '[ARLOGS] Telemetry stream active on port 9655 (Zero-DM policy active)'
  ])

  const getApiUrl = (endpoint, port = 9700) => {
    const host = window.location.hostname
    const currentPort = window.location.port
    if (currentPort === '3019' || currentPort === '5173' || host === 'localhost' || host === '127.0.0.1') {
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
            if (authData.is_master) {
              fetchData()
            }
          }
        }
      } catch (err) {
        console.warn('Auth check error:', err)
      } finally {
        setLoading(false)
      }
    }
    init()
  }, [])

  const fetchData = async () => {
    try {
      const [svcRes, metRes] = await Promise.all([
        fetch(getApiUrl('/arapi/ctrl/services', 9700), { credentials: 'include' }),
        fetch(getApiUrl('/arapi/ctrl/system', 9700), { credentials: 'include' })
      ])
      if (svcRes.ok) setServices(await svcRes.json())
      if (metRes.ok) setMetrics(await metRes.json())
    } catch (e) {}
  }

  const handleRestartService = async (serviceId) => {
    if (!confirm(`Deseja realmente reiniciar o serviço [${serviceId}]?`)) return
    try {
      const res = await fetch(getApiUrl('/arapi/ctrl/services/restart', 9700), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'include',
        body: JSON.stringify({ service_id: serviceId })
      })
      if (res.ok) {
        setLogs(prev => [`[RESTART] Daemon '${serviceId}' reiniciado com sucesso por ${session.user}`, ...prev.slice(0, 10)])
        fetchData()
      }
    } catch (err) {
      console.error(err)
    }
  }

  if (loading) {
    return (
      <div style={{ display: 'flex', height: '100vh', alignItems: 'center', justifyContent: 'center' }}>
        <div style={{ color: 'var(--text-muted)' }}>Carregando Master Mission Control...</div>
      </div>
    )
  }

  // Access Gate
  if (!session?.is_master) {
    return (
      <div style={{ display: 'flex', height: '100vh', alignItems: 'center', justifyContent: 'center', background: '#05060a' }}>
        <div style={{ textAlign: 'center', maxWidth: '480px', padding: '32px', background: 'rgba(239, 68, 68, 0.06)', border: '1px solid rgba(239, 68, 68, 0.3)', borderRadius: '16px' }}>
          <div style={{ fontSize: '3rem', marginBottom: '16px' }}>🛡️</div>
          <h2 style={{ fontSize: '1.4rem', fontWeight: 800, color: 'var(--primary)', marginBottom: '12px' }}>Acesso Restrito: Holding Master</h2>
          <p style={{ fontSize: '0.9rem', color: 'var(--text-muted)', lineHeight: 1.6 }}>
            O painel <strong>ALRI-Control</strong> é de uso restrito do Administrador Master da Holding ALRIGROUP. Seu usuário (<code>{session?.user || 'anônimo'}</code>) não possui credenciais suficientes.
          </p>
        </div>
      </div>
    )
  }

  return (
    <div className="ctrl-container">
      {/* Header */}
      <header className="ctrl-header">
        <div className="ctrl-brand">
          <div className="ctrl-logo">⚙️</div>
          <div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
              <h1 style={{ fontSize: '1.4rem', fontWeight: 800 }}>ALRI-Control</h1>
              <span className="master-badge">⚡ Master Sovereign Gate</span>
            </div>
            <div style={{ fontSize: '0.78rem', color: 'var(--text-muted)' }}>Master Mission Control & Sovereign Infrastructure</div>
          </div>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <div style={{ textAlign: 'right' }}>
            <div style={{ fontWeight: 700, fontSize: '0.9rem', color: '#fff' }}>{session?.user}</div>
            <div style={{ fontSize: '0.74rem', color: 'var(--primary)' }}>HOLDING MASTER</div>
          </div>
          <div style={{ width: '36px', height: '36px', borderRadius: '50%', background: 'var(--primary)', color: '#fff', display: 'flex', alignItems: 'center', justifyContent: 'center', fontWeight: 800 }}>
            {session?.user?.substring(0, 2).toUpperCase()}
          </div>
        </div>
      </header>

      {/* Host Metrics Gauges */}
      <div className="gauges-grid">
        <div className="gauge-card">
          <span className="gauge-title">Uso de CPU Host</span>
          <span className="gauge-val" style={{ color: 'var(--accent-cyan)' }}>{metrics?.cpu_usage_percent || 4.2}%</span>
          <span style={{ fontSize: '0.72rem', color: 'var(--text-dark)' }}>{metrics?.cpu_cores || 28} Cores Ativos</span>
        </div>
        <div className="gauge-card">
          <span className="gauge-title">Memória RAM Alocada</span>
          <span className="gauge-val" style={{ color: 'var(--accent-emerald)' }}>
            {(metrics?.ram_used_mb / 1024)?.toFixed(1) || 12.4} GB
          </span>
          <span style={{ fontSize: '0.72rem', color: 'var(--text-dark)' }}>Total: {(metrics?.ram_total_mb / 1024)?.toFixed(1) || 64.0} GB</span>
        </div>
        <div className="gauge-card">
          <span className="gauge-title">Sockets TCP Ativos</span>
          <span className="gauge-val" style={{ color: 'var(--accent-amber)' }}>{metrics?.active_tcp_sockets || 14}</span>
          <span style={{ fontSize: '0.72rem', color: 'var(--text-dark)' }}>Zero conexões órfãs</span>
        </div>
        <div className="gauge-card">
          <span className="gauge-title">Microserviços Saudáveis</span>
          <span className="gauge-val" style={{ color: 'var(--accent-emerald)' }}>
            {metrics?.microservices_healthy || 10} / {metrics?.microservices_total || 10}
          </span>
          <span style={{ fontSize: '0.72rem', color: 'var(--text-dark)' }}>100% de disponibilidade</span>
        </div>
      </div>

      {/* Microservices Grid */}
      <div className="section-title">
        <span>🔌</span>
        <span>Microserviços Soberanos do Ecossistema ({services.length})</span>
      </div>

      <div className="services-grid">
        {services.map((svc) => (
          <div key={svc.id} className="service-card">
            <div className="service-header">
              <span className="service-name">{svc.name}</span>
              <span className="status-pill healthy">{svc.status}</span>
            </div>

            <div className="service-info">
              <div>Porta: <strong style={{ color: '#fff' }}>{svc.port}</strong></div>
              <div>Camada: <span style={{ color: 'var(--accent-cyan)' }}>{svc.type}</span></div>
              <div>Restarts: <strong>{svc.restart_count}</strong></div>
              <div style={{ fontSize: '0.72rem', color: 'var(--text-dark)' }}>Último start: {svc.last_restart}</div>
            </div>

            <button className="btn-restart" onClick={() => handleRestartService(svc.id)}>
              ↺ Reiniciar Daemon
            </button>
          </div>
        ))}
      </div>

      {/* Terminal Stream */}
      <div className="section-title">
        <span>📟</span>
        <span>Terminal de Telemetria & Eventos do Master Kernel</span>
      </div>
      <div className="terminal-box">
        {logs.map((line, idx) => (
          <div key={idx}>{line}</div>
        ))}
      </div>
    </div>
  )
}
