import React, { useState, useEffect, useRef } from 'react'

export default function App() {
  const [session, setSession] = useState(null)
  const [loading, setLoading] = useState(true)

  // Navigation mode: 'channel', 'dm', 'kanban'
  const [viewMode, setViewMode] = useState('channel')
  const [activeChannel, setActiveChannel] = useState('geral')
  const [activeDmUser, setActiveDmUser] = useState('admin')

  const [channels, setChannels] = useState([])
  const [messages, setMessages] = useState([])
  const [dms, setDms] = useState([])
  const [tasks, setTasks] = useState([])

  const [msgInput, setMsgInput] = useState('')
  const [dmInput, setDmInput] = useState('')

  // Modal for new task
  const [showTaskModal, setShowTaskModal] = useState(false)
  const [taskTitle, setTaskTitle] = useState('')
  const [taskDesc, setTaskDesc] = useState('')
  const [taskPrio, setTaskPrio] = useState('medium')
  const [taskAssignee, setTaskAssignee] = useState('')

  const msgEndRef = useRef(null)

  const getApiUrl = (endpoint, port = 9690) => {
    const host = window.location.hostname
    const currentPort = window.location.port
    if (currentPort === '3017' || currentPort === '5173' || host === 'localhost' || host === '127.0.0.1') {
      return `http://${host}:${port}${endpoint}`
    }
    return endpoint
  }

  // 1. SSO Check
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
            fetchChannels()
            fetchTasks()
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

  // 2. Polling for messages or DMs
  useEffect(() => {
    if (!session) return

    let interval = null
    if (viewMode === 'channel') {
      fetchMessages(activeChannel)
      interval = setInterval(() => fetchMessages(activeChannel), 3000)
    } else if (viewMode === 'dm') {
      fetchDms(activeDmUser)
      interval = setInterval(() => fetchDms(activeDmUser), 3000)
    } else if (viewMode === 'kanban') {
      fetchTasks()
      interval = setInterval(fetchTasks, 5000)
    }

    return () => {
      if (interval) clearInterval(interval)
    }
  }, [viewMode, activeChannel, activeDmUser, session])

  useEffect(() => {
    msgEndRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [messages, dms])

  const fetchChannels = async () => {
    try {
      const res = await fetch(getApiUrl('/arapi/conn/channels', 9690), { credentials: 'include' })
      if (res.ok) setChannels(await res.json())
    } catch (e) {}
  }

  const fetchMessages = async (chan) => {
    try {
      const res = await fetch(getApiUrl(`/arapi/conn/messages?channel=${chan}`, 9690), { credentials: 'include' })
      if (res.ok) setMessages(await res.json())
    } catch (e) {}
  }

  const fetchDms = async (target) => {
    try {
      const res = await fetch(getApiUrl(`/arapi/conn/dms?with=${target}`, 9690), { credentials: 'include' })
      if (res.ok) setDms(await res.json())
    } catch (e) {}
  }

  const fetchTasks = async () => {
    try {
      const res = await fetch(getApiUrl('/arapi/conn/tasks', 9690), { credentials: 'include' })
      if (res.ok) setTasks(await res.json())
    } catch (e) {}
  }

  const handleSendMessage = async (e) => {
    e.preventDefault()
    if (!msgInput.trim()) return
    const txt = msgInput.trim()
    setMsgInput('')

    try {
      await fetch(getApiUrl('/arapi/conn/messages/post', 9690), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'include',
        body: JSON.stringify({ channel_id: activeChannel, content: txt })
      })
      fetchMessages(activeChannel)
    } catch (err) {
      console.error(err)
    }
  }

  const handleSendDm = async (e) => {
    e.preventDefault()
    if (!dmInput.trim()) return
    const txt = dmInput.trim()
    setDmInput('')

    try {
      await fetch(getApiUrl('/arapi/conn/dms/send', 9690), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'include',
        body: JSON.stringify({ recipient: activeDmUser, content: txt })
      })
      fetchDms(activeDmUser)
    } catch (err) {
      console.error(err)
    }
  }

  const handleMoveTask = async (taskId, currentStatus) => {
    const nextStatus = currentStatus === 'todo' ? 'in_progress' : currentStatus === 'in_progress' ? 'done' : 'todo'
    try {
      await fetch(getApiUrl('/arapi/conn/tasks/update', 9690), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'include',
        body: JSON.stringify({ task_id: taskId, status: nextStatus })
      })
      fetchTasks()
    } catch (err) {
      console.error(err)
    }
  }

  const handleCreateTask = async (e) => {
    e.preventDefault()
    if (!taskTitle.trim()) return
    try {
      await fetch(getApiUrl('/arapi/conn/tasks/create', 9690), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'include',
        body: JSON.stringify({
          company_id: session.tenant,
          title: taskTitle.trim(),
          description: taskDesc.trim(),
          assignee: taskAssignee.trim() || session.user,
          priority: taskPrio
        })
      })
      setTaskTitle(''); setTaskDesc(''); setTaskAssignee('');
      setShowTaskModal(false)
      fetchTasks()
    } catch (err) {
      console.error(err)
    }
  }

  if (loading) {
    return (
      <div style={{ display: 'flex', height: '100vh', alignItems: 'center', justifyContent: 'center' }}>
        <div style={{ color: 'var(--text-muted)' }}>Carregando ALRI-Connect...</div>
      </div>
    )
  }

  return (
    <div className="conn-layout">
      {/* Sidebar */}
      <aside className="conn-sidebar">
        <div className="sidebar-header">
          <div className="conn-logo">💬</div>
          <div>
            <div style={{ fontWeight: 800, fontSize: '1.05rem' }}>ALRI-Connect</div>
            <div style={{ fontSize: '0.72rem', color: 'var(--accent-cyan)' }}>Comunicação & Kanban</div>
          </div>
        </div>

        <div className="sidebar-content">
          {/* Navigation Tools */}
          <div
            className={`sidebar-item ${viewMode === 'kanban' ? 'active' : ''}`}
            onClick={() => setViewMode('kanban')}
          >
            <span>📋</span>
            <span style={{ fontWeight: 600 }}>Quadro Kanban</span>
          </div>

          {/* Channels Section */}
          <div className="section-label">
            <span>Canais de Equipe</span>
            <span style={{ cursor: 'pointer', fontSize: '0.9rem' }}>+</span>
          </div>
          {channels.map((c) => (
            <div
              key={c.id}
              className={`sidebar-item ${viewMode === 'channel' && activeChannel === c.id ? 'active' : ''}`}
              onClick={() => { setViewMode('channel'); setActiveChannel(c.id); }}
            >
              <span>#</span>
              <span>{c.name}</span>
            </div>
          ))}

          {/* Direct Messages Section */}
          <div className="section-label" style={{ marginTop: '24px' }}>
            <span>Conversas Privadas</span>
            <span style={{ fontSize: '0.7rem', color: '#6ee7b7' }}>🔒 Sigilo</span>
          </div>
          {['alexsanderalri', 'gabriel_staff', 'admin'].filter(u => u !== session?.user).map((u) => (
            <div
              key={u}
              className={`sidebar-item ${viewMode === 'dm' && activeDmUser === u ? 'active' : ''}`}
              onClick={() => { setViewMode('dm'); setActiveDmUser(u); }}
            >
              <span>👤</span>
              <span>@{u}</span>
            </div>
          ))}
        </div>

        {/* User Footer */}
        <div className="sidebar-user">
          <div className="user-avatar">{session?.user?.substring(0, 2).toUpperCase()}</div>
          <div>
            <div style={{ fontWeight: 700, fontSize: '0.86rem' }}>{session?.user}</div>
            <div style={{ fontSize: '0.72rem', color: 'var(--text-muted)' }}>{session?.tenant?.toUpperCase()}</div>
          </div>
        </div>
      </aside>

      {/* Main View Area */}
      <main className="conn-main">
        {/* VIEW 1: CHANNEL CHAT */}
        {viewMode === 'channel' && (
          <>
            <header className="chat-header">
              <div>
                <h3 style={{ fontSize: '1.15rem', fontWeight: 800 }}>#{activeChannel}</h3>
                <span style={{ color: 'var(--text-dark)', fontSize: '0.78rem' }}>
                  {channels.find(c => c.id === activeChannel)?.topic || 'Canal de comunicação'}
                </span>
              </div>
              <div style={{ fontSize: '0.8rem', color: 'var(--text-muted)' }}>
                {messages.length} mensagens
              </div>
            </header>

            <div className="messages-list">
              {messages.map((m) => (
                <div key={m.id} className="message-item">
                  <div className="message-avatar">{m.sender_user.substring(0, 2).toUpperCase()}</div>
                  <div className="message-bubble">
                    <div className="message-meta">
                      <span className="sender-name">@{m.sender_user}</span>
                      <span className="sender-company">{m.sender_company}</span>
                      <span className="message-time">{m.timestamp}</span>
                    </div>
                    <div className="message-text">{m.content}</div>
                  </div>
                </div>
              ))}
              <div ref={msgEndRef} />
            </div>

            <div className="chat-input-wrapper">
              <form className="chat-input-form" onSubmit={handleSendMessage}>
                <input
                  type="text"
                  className="chat-input"
                  placeholder={`Conversar em #${activeChannel}...`}
                  value={msgInput}
                  onChange={(e) => setMsgInput(e.target.value)}
                />
                <button type="submit" className="send-btn">Enviar</button>
              </form>
            </div>
          </>
        )}

        {/* VIEW 2: PRIVATE DIRECT MESSAGES (DMs) */}
        {viewMode === 'dm' && (
          <>
            <header className="chat-header">
              <div>
                <h3 style={{ fontSize: '1.15rem', fontWeight: 800 }}>Conversa Direta com @{activeDmUser}</h3>
                <span style={{ color: 'var(--text-dark)', fontSize: '0.78rem' }}>Canal privado criptografado de ponta a ponta</span>
              </div>
              <div className="privacy-banner">
                <span>🔒</span>
                <span>Zero-DM Shield: Mensagem Jamais Gravada em Logs</span>
              </div>
            </header>

            <div className="messages-list">
              {dms.length === 0 ? (
                <div style={{ textAlign: 'center', color: 'var(--text-dark)', marginTop: '40px', fontSize: '0.9rem' }}>
                  Nenhuma mensagem anterior. Inicie uma conversa privada segura com @{activeDmUser}.
                </div>
              ) : (
                dms.map((m) => (
                  <div key={m.id} className="message-item">
                    <div className="message-avatar">{m.sender.substring(0, 2).toUpperCase()}</div>
                    <div className="message-bubble">
                      <div className="message-meta">
                        <span className="sender-name">@{m.sender}</span>
                        <span className="message-time">{m.timestamp}</span>
                      </div>
                      <div className="message-text">{m.content}</div>
                    </div>
                  </div>
                ))
              )}
              <div ref={msgEndRef} />
            </div>

            <div className="chat-input-wrapper">
              <form className="chat-input-form" onSubmit={handleSendDm}>
                <input
                  type="text"
                  className="chat-input"
                  placeholder={`Mensagem direta confidencial para @${activeDmUser}...`}
                  value={dmInput}
                  onChange={(e) => setDmInput(e.target.value)}
                />
                <button type="submit" className="send-btn" style={{ background: 'var(--accent-emerald)' }}>Enviar Privado</button>
              </form>
            </div>
          </>
        )}

        {/* VIEW 3: KANBAN BOARD */}
        {viewMode === 'kanban' && (
          <>
            <header className="chat-header">
              <div>
                <h3 style={{ fontSize: '1.15rem', fontWeight: 800 }}>Demandas & Tarefas Corporativas</h3>
                <span style={{ color: 'var(--text-dark)', fontSize: '0.78rem' }}>Quadro Kanban sincronizado em tempo real</span>
              </div>
              <button
                className="send-btn"
                onClick={() => setShowTaskModal(true)}
              >
                + Nova Demanda
              </button>
            </header>

            <div className="kanban-board">
              {/* Column: To Do */}
              <div className="kanban-column">
                <div className="column-header">
                  <span>📌 A FAZER ({tasks.filter(t => t.status === 'todo').length})</span>
                </div>
                {tasks.filter(t => t.status === 'todo').map((t) => (
                  <div key={t.id} className="task-card" onClick={() => handleMoveTask(t.id, t.status)}>
                    <div className="task-title">{t.title}</div>
                    <div className="task-desc">{t.description}</div>
                    <div className="task-footer">
                      <span className={`prio-pill prio-${t.priority}`}>{t.priority}</span>
                      <span style={{ color: 'var(--text-dark)' }}>@{t.assignee} &bull; Avançar &rarr;</span>
                    </div>
                  </div>
                ))}
              </div>

              {/* Column: In Progress */}
              <div className="kanban-column">
                <div className="column-header">
                  <span>⚡ EM ANDAMENTO ({tasks.filter(t => t.status === 'in_progress').length})</span>
                </div>
                {tasks.filter(t => t.status === 'in_progress').map((t) => (
                  <div key={t.id} className="task-card" onClick={() => handleMoveTask(t.id, t.status)} style={{ borderLeft: '3px solid var(--accent-amber)' }}>
                    <div className="task-title">{t.title}</div>
                    <div className="task-desc">{t.description}</div>
                    <div className="task-footer">
                      <span className={`prio-pill prio-${t.priority}`}>{t.priority}</span>
                      <span style={{ color: 'var(--accent-amber)' }}>@{t.assignee} &bull; Concluir &rarr;</span>
                    </div>
                  </div>
                ))}
              </div>

              {/* Column: Done */}
              <div className="kanban-column">
                <div className="column-header">
                  <span>✅ CONCLUÍDO ({tasks.filter(t => t.status === 'done').length})</span>
                </div>
                {tasks.filter(t => t.status === 'done').map((t) => (
                  <div key={t.id} className="task-card" onClick={() => handleMoveTask(t.id, t.status)} style={{ borderLeft: '3px solid var(--accent-emerald)', opacity: 0.85 }}>
                    <div className="task-title" style={{ textDecoration: 'line-through' }}>{t.title}</div>
                    <div className="task-desc">{t.description}</div>
                    <div className="task-footer">
                      <span className="prio-pill" style={{ background: 'rgba(16, 185, 129, 0.15)', color: '#6ee7b7' }}>Pronto</span>
                      <span style={{ color: 'var(--text-dark)' }}>@{t.assignee}</span>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          </>
        )}
      </main>

      {/* MODAL: NOVA DEMANDA */}
      {showTaskModal && (
        <div style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.75)', backdropFilter: 'blur(10px)', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 999 }}>
          <div style={{ background: '#0d111d', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-lg)', padding: '32px', width: '100%', maxWidth: '460px' }}>
            <h3 style={{ fontSize: '1.25rem', fontWeight: 700, marginBottom: '18px' }}>Criar Nova Demanda / Tarefa</h3>
            <form onSubmit={handleCreateTask}>
              <input
                style={{ width: '100%', padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', marginBottom: '12px', outline: 'none' }}
                placeholder="Título da Tarefa"
                value={taskTitle}
                onChange={e => setTaskTitle(e.target.value)}
                required
              />
              <textarea
                style={{ width: '100%', height: '80px', padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', marginBottom: '12px', outline: 'none', resize: 'none' }}
                placeholder="Descrição e escopo da demanda"
                value={taskDesc}
                onChange={e => setTaskDesc(e.target.value)}
              />
              <div style={{ display: 'flex', gap: '10px', marginBottom: '18px' }}>
                <input
                  style={{ flex: 1, padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', outline: 'none' }}
                  placeholder="Responsável (@username)"
                  value={taskAssignee}
                  onChange={e => setTaskAssignee(e.target.value)}
                />
                <select
                  style={{ flex: 1, padding: '10px 14px', background: 'rgba(255,255,255,0.04)', border: '1px solid var(--border-subtle)', borderRadius: 'var(--radius-sm)', color: '#fff', outline: 'none' }}
                  value={taskPrio}
                  onChange={e => setTaskPrio(e.target.value)}
                >
                  <option value="low">Baixa Prioridade</option>
                  <option value="medium">Média Prioridade</option>
                  <option value="high">Alta Prioridade</option>
                  <option value="urgent">Urgente</option>
                </select>
              </div>

              <div style={{ display: 'flex', gap: '10px' }}>
                <button type="button" className="send-btn" style={{ flex: 1, background: 'rgba(255,255,255,0.06)' }} onClick={() => setShowTaskModal(false)}>Cancelar</button>
                <button type="submit" className="send-btn" style={{ flex: 1 }}>Salvar Tarefa</button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  )
}
