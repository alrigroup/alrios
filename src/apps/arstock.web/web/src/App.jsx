import React, { useState, useEffect } from 'react'

export default function App() {
  const [session, setSession] = useState(null)
  const [loading, setLoading] = useState(true)
  const [items, setItems] = useState([])
  const [summary, setSummary] = useState(null)
  const [searchTerm, setSearchTerm] = useState('')

  // Modals
  const [showCreateModal, setShowCreateModal] = useState(false)
  const [newItem, setNewItem] = useState({
    sku: '',
    name: '',
    category: 'Infraestrutura',
    quantity: '5',
    min_quantity: '2',
    unit_cost: '1000.0',
    location: 'Almoxarifado Central'
  })

  const [showMoveModal, setShowMoveModal] = useState(false)
  const [selectedItem, setSelectedItem] = useState(null)
  const [moveType, setMoveType] = useState('IN')
  const [moveQty, setMoveQty] = useState('1')
  const [moveReason, setMoveReason] = useState('Entrada de reposição')

  const getApiUrl = (endpoint, port = 9685) => {
    const host = window.location.hostname
    const currentPort = window.location.port
    if (currentPort === '3018' || currentPort === '5173' || host === 'localhost' || host === '127.0.0.1') {
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
            fetchData()
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
      const [itRes, smRes] = await Promise.all([
        fetch(getApiUrl('/arapi/stock/items', 9685), { credentials: 'include' }),
        fetch(getApiUrl('/arapi/stock/summary', 9685), { credentials: 'include' })
      ])
      if (itRes.ok) setItems(await itRes.json())
      if (smRes.ok) setSummary(await smRes.json())
    } catch (e) {}
  }

  const handleCreateItem = async (e) => {
    e.preventDefault()
    try {
      await fetch(getApiUrl('/arapi/stock/items/create', 9685), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'include',
        body: JSON.stringify({
          company_id: session.tenant,
          ...newItem
        })
      })
      setShowCreateModal(false)
      setNewItem({
        sku: '',
        name: '',
        category: 'Infraestrutura',
        quantity: '5',
        min_quantity: '2',
        unit_cost: '1000.0',
        location: 'Almoxarifado Central'
      })
      fetchData()
    } catch (err) {
      console.error(err)
    }
  }

  const handleRecordMovement = async (e) => {
    e.preventDefault()
    if (!selectedItem) return
    try {
      const res = await fetch(getApiUrl('/arapi/stock/items/movement', 9685), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'include',
        body: JSON.stringify({
          item_id: selectedItem.id,
          type: moveType,
          qty_change: parseInt(moveQty) || 1,
          reason: moveReason
        })
      })
      if (res.ok) {
        setShowMoveModal(false)
        fetchData()
      } else {
        const err = await res.json()
        alert(err.error || 'Erro ao movimentar estoque')
      }
    } catch (err) {
      console.error(err)
    }
  }

  const formatBRL = (val) => {
    return (val || 0).toLocaleString('pt-BR', { style: 'currency', currency: 'BRL' })
  }

  if (loading) {
    return (
      <div style={{ display: 'flex', height: '100vh', alignItems: 'center', justifyContent: 'center' }}>
        <div style={{ color: 'var(--text-muted)' }}>Carregando ALRI-Stock...</div>
      </div>
    )
  }

  const filteredItems = items.filter(
    it => it.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
          it.sku.toLowerCase().includes(searchTerm.toLowerCase()) ||
          it.category.toLowerCase().includes(searchTerm.toLowerCase())
  )

  return (
    <div className="stock-container">
      {/* Header */}
      <header className="stock-header">
        <div className="stock-brand">
          <div className="stock-logo">📦</div>
          <div>
            <h1 style={{ fontSize: '1.4rem', fontWeight: 800 }}>ALRI-Stock</h1>
            <div style={{ fontSize: '0.78rem', color: 'var(--accent-cyan)' }}>Gestão de Estoque & Patrimônio Soberano</div>
          </div>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <div style={{ textAlign: 'right' }}>
            <div style={{ fontWeight: 700, fontSize: '0.9rem' }}>{session?.user}</div>
            <div style={{ fontSize: '0.74rem', color: 'var(--text-muted)' }}>Empresa: {session?.tenant?.toUpperCase()}</div>
          </div>
          <div style={{ width: '36px', height: '36px', borderRadius: '50%', background: 'var(--primary)', color: '#000', display: 'flex', alignItems: 'center', justifyContent: 'center', fontWeight: 800 }}>
            {session?.user?.substring(0, 2).toUpperCase()}
          </div>
        </div>
      </header>

      {/* KPI Cards */}
      <div className="kpi-grid">
        <div className="kpi-card">
          <span className="kpi-title">Valor Total do Patrimônio</span>
          <span className="kpi-val" style={{ color: 'var(--primary)' }}>{formatBRL(summary?.total_inventory_value)}</span>
        </div>
        <div className="kpi-card">
          <span className="kpi-title">Total de SKUs Ativos</span>
          <span className="kpi-val">{summary?.total_skus || 0}</span>
        </div>
        <div className="kpi-card">
          <span className="kpi-title">Unidades Físicas / Itens</span>
          <span className="kpi-val">{summary?.total_units || 0}</span>
        </div>
        <div className={`kpi-card ${summary?.low_stock_alerts > 0 ? 'alert' : ''}`}>
          <span className="kpi-title">Alertas de Reposição</span>
          <span className="kpi-val" style={{ color: summary?.low_stock_alerts > 0 ? 'var(--accent-rose)' : 'var(--accent-emerald)' }}>
            {summary?.low_stock_alerts || 0}
          </span>
        </div>
      </div>

      {/* Action & Search Bar */}
      <div className="controls-bar">
        <input
          type="text"
          className="search-input"
          placeholder="Buscar por SKU, nome ou categoria..."
          value={searchTerm}
          onChange={e => setSearchTerm(e.target.value)}
        />

        <button className="btn-primary" onClick={() => setShowCreateModal(true)}>
          + Cadastrar Item / Ativo
        </button>
      </div>

      {/* Inventory Table */}
      <div className="table-card">
        <table className="stock-table">
          <thead>
            <tr>
              <th>SKU</th>
              <th>Nome do Ativo / Item</th>
              <th>Categoria</th>
              <th>Estoque / Qtd</th>
              <th>Custo Unit.</th>
              <th>Valor Total</th>
              <th>Localização</th>
              <th>Ações</th>
            </tr>
          </thead>
          <tbody>
            {filteredItems.map((item) => {
              const isLow = item.quantity <= item.min_quantity
              return (
                <tr key={item.id}>
                  <td><span className="sku-badge">{item.sku}</span></td>
                  <td><strong style={{ color: '#fff' }}>{item.name}</strong></td>
                  <td><span style={{ color: 'var(--text-muted)' }}>{item.category}</span></td>
                  <td>
                    <span className={`qty-badge ${isLow ? 'low' : 'ok'}`}>
                      {isLow ? '⚠️' : '✓'} {item.quantity} un (Mín: {item.min_quantity})
                    </span>
                  </td>
                  <td>{formatBRL(item.unit_cost)}</td>
                  <td><strong style={{ color: 'var(--primary)' }}>{formatBRL(item.total_value)}</strong></td>
                  <td><span style={{ fontSize: '0.8rem', color: 'var(--text-dark)' }}>{item.location}</span></td>
                  <td>
                    <button
                      className="btn-move"
                      onClick={() => {
                        setSelectedItem(item)
                        setShowMoveModal(true)
                      }}
                    >
                      Entrada / Saída
                    </button>
                  </td>
                </tr>
              )
            })}
          </tbody>
        </table>
      </div>

      {/* MODAL: NOVO ITEM */}
      {showCreateModal && (
        <div style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.75)', backdropFilter: 'blur(10px)', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 999 }}>
          <div style={{ background: '#0e1220', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-lg)', padding: '32px', width: '100%', maxWidth: '480px' }}>
            <h3 style={{ fontSize: '1.25rem', fontWeight: 700, marginBottom: '18px' }}>Cadastrar Novo Ativo no Estoque</h3>
            <form onSubmit={handleCreateItem}>
              <input
                style={{ width: '100%', padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', marginBottom: '10px', outline: 'none' }}
                placeholder="Código SKU (ex: SRV-AMD-64)"
                value={newItem.sku}
                onChange={e => setNewItem({ ...newItem, sku: e.target.value })}
                required
              />
              <input
                style={{ width: '100%', padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', marginBottom: '10px', outline: 'none' }}
                placeholder="Nome do Item / Ativo"
                value={newItem.name}
                onChange={e => setNewItem({ ...newItem, name: e.target.value })}
                required
              />
              <input
                style={{ width: '100%', padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', marginBottom: '10px', outline: 'none' }}
                placeholder="Categoria (ex: Infraestrutura, Periféricos)"
                value={newItem.category}
                onChange={e => setNewItem({ ...newItem, category: e.target.value })}
              />
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px', marginBottom: '10px' }}>
                <input
                  style={{ width: '100%', padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', outline: 'none' }}
                  placeholder="Qtd Inicial"
                  type="number"
                  value={newItem.quantity}
                  onChange={e => setNewItem({ ...newItem, quantity: e.target.value })}
                />
                <input
                  style={{ width: '100%', padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', outline: 'none' }}
                  placeholder="Qtd Mínima Alerta"
                  type="number"
                  value={newItem.min_quantity}
                  onChange={e => setNewItem({ ...newItem, min_quantity: e.target.value })}
                />
              </div>
              <input
                style={{ width: '100%', padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', marginBottom: '10px', outline: 'none' }}
                placeholder="Custo Unitário R$ (ex: 2500.00)"
                value={newItem.unit_cost}
                onChange={e => setNewItem({ ...newItem, unit_cost: e.target.value })}
              />
              <input
                style={{ width: '100%', padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', marginBottom: '18px', outline: 'none' }}
                placeholder="Localização (ex: Datacenter Sala 2)"
                value={newItem.location}
                onChange={e => setNewItem({ ...newItem, location: e.target.value })}
              />

              <div style={{ display: 'flex', gap: '10px' }}>
                <button type="button" className="btn-primary" style={{ flex: 1, background: 'rgba(255,255,255,0.06)', color: '#fff' }} onClick={() => setShowCreateModal(false)}>Cancelar</button>
                <button type="submit" className="btn-primary" style={{ flex: 1 }}>Salvar Item</button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* MODAL: MOVIMENTAÇÃO DE ESTOQUE */}
      {showMoveModal && selectedItem && (
        <div style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.75)', backdropFilter: 'blur(10px)', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 999 }}>
          <div style={{ background: '#0e1220', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-lg)', padding: '32px', width: '100%', maxWidth: '440px' }}>
            <h3 style={{ fontSize: '1.25rem', fontWeight: 700, marginBottom: '8px' }}>Movimentar Estoque</h3>
            <div style={{ fontSize: '0.86rem', color: 'var(--text-muted)', marginBottom: '18px' }}>
              Item: <strong>{selectedItem.name}</strong> ({selectedItem.sku}) | Saldo Atual: <strong>{selectedItem.quantity} un</strong>
            </div>

            <form onSubmit={handleRecordMovement}>
              <div style={{ display: 'flex', gap: '10px', marginBottom: '12px' }}>
                <button
                  type="button"
                  style={{ flex: 1, padding: '10px', borderRadius: '8px', border: '1px solid var(--border-subtle)', background: moveType === 'IN' ? 'var(--accent-emerald)' : 'rgba(255,255,255,0.04)', color: moveType === 'IN' ? '#000' : '#fff', fontWeight: 700, cursor: 'pointer' }}
                  onClick={() => setMoveType('IN')}
                >
                  + Entrada (Reposição)
                </button>
                <button
                  type="button"
                  style={{ flex: 1, padding: '10px', borderRadius: '8px', border: '1px solid var(--border-subtle)', background: moveType === 'OUT' ? 'var(--accent-rose)' : 'rgba(255,255,255,0.04)', color: moveType === 'OUT' ? '#fff' : '#fff', fontWeight: 700, cursor: 'pointer' }}
                  onClick={() => setMoveType('OUT')}
                >
                  - Saída (Baixa)
                </button>
              </div>

              <input
                style={{ width: '100%', padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', marginBottom: '12px', outline: 'none' }}
                placeholder="Quantidade"
                type="number"
                min="1"
                value={moveQty}
                onChange={e => setMoveQty(e.target.value)}
                required
              />

              <input
                style={{ width: '100%', padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', marginBottom: '18px', outline: 'none' }}
                placeholder="Motivo (ex: Entrega para colaborador, Reposição de compra)"
                value={moveReason}
                onChange={e => setMoveReason(e.target.value)}
                required
              />

              <div style={{ display: 'flex', gap: '10px' }}>
                <button type="button" className="btn-primary" style={{ flex: 1, background: 'rgba(255,255,255,0.06)', color: '#fff' }} onClick={() => setShowMoveModal(false)}>Cancelar</button>
                <button type="submit" className="btn-primary" style={{ flex: 1 }}>Confirmar</button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  )
}
