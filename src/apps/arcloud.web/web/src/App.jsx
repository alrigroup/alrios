import React, { useState, useEffect } from 'react'

export default function App() {
  const [session, setSession] = useState(null)
  const [loading, setLoading] = useState(true)
  const [files, setFiles] = useState([])
  const [quota, setQuota] = useState(null)
  const [searchTerm, setSearchTerm] = useState('')

  // Upload modal
  const [showModal, setShowModal] = useState(false)
  const [newFileName, setNewFileName] = useState('')
  const [newFileSize, setNewFileSize] = useState('2048000')
  const [newFileMime, setNewFileMime] = useState('application/pdf')

  const getApiUrl = (endpoint, port = 9695) => {
    const host = window.location.hostname
    const currentPort = window.location.port
    if (currentPort === '3016' || currentPort === '5173' || host === 'localhost' || host === '127.0.0.1') {
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
      const [fRes, qRes] = await Promise.all([
        fetch(getApiUrl('/arapi/cloud/files', 9695), { credentials: 'include' }),
        fetch(getApiUrl('/arapi/cloud/quota', 9695), { credentials: 'include' })
      ])
      if (fRes.ok) setFiles(await fRes.json())
      if (qRes.ok) setQuota(await qRes.json())
    } catch (e) {}
  }

  const handleCreateFile = async (e) => {
    e.preventDefault()
    if (!newFileName.trim()) return
    try {
      await fetch(getApiUrl('/arapi/cloud/files/create', 9695), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'include',
        body: JSON.stringify({
          company_id: session.tenant,
          name: newFileName.trim(),
          size_bytes: parseInt(newFileSize) || 1024,
          mime_type: newFileMime
        })
      })
      setNewFileName('')
      setShowModal(false)
      fetchData()
    } catch (err) {
      console.error(err)
    }
  }

  const handleDeleteFile = async (fileId, e) => {
    e.stopPropagation()
    if (!confirm('Deseja realmente remover este arquivo?')) return
    try {
      await fetch(getApiUrl('/arapi/cloud/files/delete', 9695), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'include',
        body: JSON.stringify({ file_id: fileId })
      })
      fetchData()
    } catch (err) {
      console.error(err)
    }
  }

  const getFileIcon = (name, mime) => {
    if (name.endsWith('.pdf') || mime.includes('pdf')) return '📄'
    if (name.endsWith('.zip') || name.endsWith('.tar') || mime.includes('zip')) return '📦'
    if (name.endsWith('.docx') || name.endsWith('.doc')) return '📝'
    if (name.endsWith('.png') || name.endsWith('.jpg') || mime.includes('image')) return '🖼️'
    return '📁'
  }

  const formatBytes = (bytes) => {
    if (bytes === 0) return '0 B'
    const k = 1024
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB']
    const i = Math.floor(Math.log(bytes) / Math.log(k))
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i]
  }

  if (loading) {
    return (
      <div style={{ display: 'flex', height: '100vh', alignItems: 'center', justifyContent: 'center' }}>
        <div style={{ color: 'var(--text-muted)' }}>Carregando ALRI-Cloud Drive...</div>
      </div>
    )
  }

  const filteredFiles = files.filter(f => f.name.toLowerCase().includes(searchTerm.toLowerCase()))

  return (
    <div className="cloud-layout">
      {/* Sidebar */}
      <aside className="cloud-sidebar">
        <div className="sidebar-header">
          <div className="cloud-logo">☁️</div>
          <div>
            <div style={{ fontWeight: 800, fontSize: '1.05rem' }}>ALRI-Cloud</div>
            <div style={{ fontSize: '0.72rem', color: 'var(--accent-cyan)' }}>Drive Corporativo</div>
          </div>
        </div>

        <div className="sidebar-content">
          <div className="nav-item active">
            <span>📁</span>
            <span>Todos os Arquivos</span>
          </div>
          <div className="nav-item">
            <span>⭐</span>
            <span>Favoritos</span>
          </div>
          <div className="nav-item">
            <span>🗑️</span>
            <span>Lixeira</span>
          </div>
        </div>

        {/* Quota Indicator */}
        <div className="quota-box">
          <div className="quota-header">
            <span>Armazenamento</span>
            <span>{quota?.used_mb?.toFixed(1)} MB / {quota?.quota_limit_gb} GB</span>
          </div>
          <div className="quota-track">
            <div className="quota-fill" style={{ width: `${Math.min(quota?.percent_used || 5, 100)}%` }} />
          </div>
          <div style={{ fontSize: '0.72rem', color: 'var(--text-dark)' }}>
            {quota?.percent_used?.toFixed(2)}% da cota utilizada
          </div>
        </div>

        <div style={{ padding: '14px 20px', borderTop: '1px solid var(--border-subtle)', display: 'flex', alignItems: 'center', gap: '10px' }}>
          <div style={{ width: '32px', height: '32px', borderRadius: '50%', background: 'var(--primary)', display: 'flex', alignItems: 'center', justifyContent: 'center', fontWeight: 700, fontSize: '0.8rem' }}>
            {session?.user?.substring(0, 2).toUpperCase()}
          </div>
          <div>
            <div style={{ fontWeight: 700, fontSize: '0.86rem' }}>{session?.user}</div>
            <div style={{ fontSize: '0.72rem', color: 'var(--accent-cyan)' }}>{session?.tenant?.toUpperCase()}</div>
          </div>
        </div>
      </aside>

      {/* Main Drive View */}
      <main className="cloud-main">
        <header className="cloud-topbar">
          <input
            type="text"
            className="search-input"
            placeholder="Buscar arquivos e documentos..."
            value={searchTerm}
            onChange={e => setSearchTerm(e.target.value)}
          />

          <button className="btn-upload" onClick={() => setShowModal(true)}>
            + Novo Arquivo
          </button>
        </header>

        <div className="drive-content">
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '24px' }}>
            <h2 style={{ fontSize: '1.25rem', fontWeight: 800 }}>Documentos & Arquivos ({filteredFiles.length})</h2>
            <span style={{ fontSize: '0.84rem', color: 'var(--text-dark)' }}>Empresa: {session?.tenant?.toUpperCase()}</span>
          </div>

          <div className="files-grid">
            {filteredFiles.map((file) => (
              <div key={file.id} className="file-card">
                <button className="btn-delete-file" onClick={(e) => handleDeleteFile(file.id, e)} title="Remover Arquivo">
                  ✕
                </button>
                <div className="file-icon">{getFileIcon(file.name, file.mime_type)}</div>
                <div className="file-name" title={file.name}>{file.name}</div>
                <div className="file-meta">
                  <span>{formatBytes(file.size_bytes)}</span>
                  <span>{file.created_at?.split(' ')[0]}</span>
                </div>
              </div>
            ))}
          </div>
        </div>
      </main>

      {/* MODAL: NOVO ARQUIVO */}
      {showModal && (
        <div style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.75)', backdropFilter: 'blur(10px)', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 999 }}>
          <div style={{ background: '#0d111d', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-lg)', padding: '32px', width: '100%', maxWidth: '440px' }}>
            <h3 style={{ fontSize: '1.25rem', fontWeight: 700, marginBottom: '18px' }}>Adicionar Documento / Arquivo</h3>
            <form onSubmit={handleCreateFile}>
              <input
                style={{ width: '100%', padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', marginBottom: '12px', outline: 'none' }}
                placeholder="Nome do arquivo (ex: Relatorio_Q3.pdf)"
                value={newFileName}
                onChange={e => setNewFileName(e.target.value)}
                required
              />
              <select
                style={{ width: '100%', padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', marginBottom: '12px', outline: 'none' }}
                value={newFileMime}
                onChange={e => setNewFileMime(e.target.value)}
              >
                <option value="application/pdf">Documento PDF (.pdf)</option>
                <option value="application/vnd.openxmlformats">Word Document (.docx)</option>
                <option value="application/zip">Arquivo Compactado (.zip)</option>
                <option value="image/png">Imagem PNG (.png)</option>
              </select>
              <input
                style={{ width: '100%', padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', marginBottom: '18px', outline: 'none' }}
                placeholder="Tamanho em Bytes (ex: 2048000)"
                value={newFileSize}
                onChange={e => setNewFileSize(e.target.value)}
              />

              <div style={{ display: 'flex', gap: '10px' }}>
                <button type="button" className="btn-upload" style={{ flex: 1, background: 'rgba(255,255,255,0.06)' }} onClick={() => setShowModal(false)}>Cancelar</button>
                <button type="submit" className="btn-upload" style={{ flex: 1 }}>Salvar Arquivo</button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  )
}
