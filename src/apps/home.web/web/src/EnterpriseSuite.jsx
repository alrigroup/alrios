/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

import { useState, useEffect } from 'react'

export default function EnterpriseSuite({ authSession, onLogout, lang, onBackToPublic }) {
  const [activeTab, setActiveTab] = useState('dashboard') // dashboard | companies | employees | kanban | chat
  const [companies, setCompanies] = useState([])
  const [departments, setDepartments] = useState([])
  const [employees, setEmployees] = useState([])
  const [tasks, setTasks] = useState([])
  const [messages, setMessages] = useState([])
  const [activeChannel, setActiveChannel] = useState('general')
  const [chatInput, setChatInput] = useState('')
  const [loading, setLoading] = useState(false)
  const [notification, setNotification] = useState({ type: '', message: '' })

  // Modals
  const [newEmployeeModal, setNewEmployeeModal] = useState(false)
  const [passwordModal, setPasswordModal] = useState({ open: false, targetUser: '' })
  const [newTaskModal, setNewTaskModal] = useState(false)

  // Form states
  const [newEmpForm, setNewEmpForm] = useState({
    username: '',
    password: '',
    full_name: '',
    email: '',
    phone: '',
    company_id: 'alrigroup',
    department_id: 'dept_eng',
    position_title: 'Engenheiro de Software',
    hierarchy_level: 10,
    role: 'user'
  })

  const [newPassInput, setNewPassInput] = useState('')
  const [newTaskForm, setNewTaskForm] = useState({
    title: '',
    description: '',
    priority: 'medium',
    assigned_to: authSession.user,
    company_id: 'alrigroup'
  })

  const getApiBaseUrl = () => {
    if (typeof window !== 'undefined' && (window.location.port === '3001' || window.location.port === '5173')) {
      return `http://${window.location.hostname}:9670`
    }
    return ''
  }

  const notify = (type, message) => {
    setNotification({ type, message })
    setTimeout(() => setNotification({ type: '', message: '' }), 4000)
  }

  const authHeaders = () => ({
    'Content-Type': 'application/json',
    'Authorization': `Bearer ${authSession.sessionToken}`
  })

  // Fetch initial enterprise data
  const fetchData = async () => {
    setLoading(true)
    const base = getApiBaseUrl()
    try {
      const [resComp, resDept, resEmp, resTask, resMsg] = await Promise.all([
        fetch(`${base}/arapi/enterprise/companies`, { headers: authHeaders() }),
        fetch(`${base}/arapi/enterprise/departments`, { headers: authHeaders() }),
        fetch(`${base}/arapi/enterprise/employees`, { headers: authHeaders() }),
        fetch(`${base}/arapi/enterprise/tasks`, { headers: authHeaders() }),
        fetch(`${base}/arapi/enterprise/messages`, { headers: authHeaders() })
      ])

      if (resComp.ok) {
        const d = await resComp.json()
        setCompanies(d.companies || [])
      }
      if (resDept.ok) {
        const d = await resDept.json()
        setDepartments(d.departments || [])
      }
      if (resEmp.ok) {
        const d = await resEmp.json()
        setEmployees(d.employees || [])
      }
      if (resTask.ok) {
        const d = await resTask.json()
        setTasks(d.tasks || [])
      }
      if (resMsg.ok) {
        const d = await resMsg.json()
        setMessages(d.messages || [])
      }
    } catch (e) {
      console.warn('[Enterprise] Sync error:', e)
    }
    setLoading(false)
  }

  useEffect(() => {
    fetchData()
  }, [])

  // Auto-refresh chat every 4s when in chat tab
  useEffect(() => {
    if (activeTab !== 'chat') return
    const interval = setInterval(async () => {
      try {
        const base = getApiBaseUrl()
        const res = await fetch(`${base}/arapi/enterprise/messages`, { headers: authHeaders() })
        if (res.ok) {
          const d = await res.json()
          setMessages(d.messages || [])
        }
      } catch (e) {}
    }, 4000)
    return () => clearInterval(interval)
  }, [activeTab])

  // Create Employee
  const handleCreateEmployee = async (e) => {
    e.preventDefault()
    const base = getApiBaseUrl()
    try {
      const res = await fetch(`${base}/arapi/enterprise/employees`, {
        method: 'POST',
        headers: authHeaders(),
        body: JSON.stringify(newEmpForm)
      })
      const data = await res.json()
      if (res.ok) {
        notify('success', 'Colaborador cadastrado e sincronizado com o cofre ARAUTH com sucesso!')
        setNewEmployeeModal(false)
        setNewEmpForm({
          username: '',
          password: '',
          full_name: '',
          email: '',
          phone: '',
          company_id: 'alrigroup',
          department_id: 'dept_eng',
          position_title: 'Engenheiro de Software',
          hierarchy_level: 10,
          role: 'user'
        })
        fetchData()
      } else {
        notify('error', data.error || 'Falha ao cadastrar colaborador')
      }
    } catch (err) {
      notify('error', 'Erro de conexão com o servidor')
    }
  }

  // Change Password
  const handleChangePassword = async (e) => {
    e.preventDefault()
    if (!newPassInput || newPassInput.length < 6) {
      notify('error', 'A nova senha deve conter pelo menos 6 caracteres')
      return
    }
    const base = getApiBaseUrl()
    try {
      const res = await fetch(`${base}/arapi/enterprise/employees/passwd`, {
        method: 'POST',
        headers: authHeaders(),
        body: JSON.stringify({
          username: passwordModal.targetUser,
          new_password: newPassInput
        })
      })
      const data = await res.json()
      if (res.ok) {
        notify('success', `Senha do usuário '${passwordModal.targetUser}' alterada e sessões anteriores revogadas!`)
        setPasswordModal({ open: false, targetUser: '' })
        setNewPassInput('')
      } else {
        notify('error', data.error || 'Permissão insuficiente ou erro no cofre')
      }
    } catch (err) {
      notify('error', 'Erro de comunicação com o servidor')
    }
  }

  // Create Task
  const handleCreateTask = async (e) => {
    e.preventDefault()
    const base = getApiBaseUrl()
    try {
      const res = await fetch(`${base}/arapi/enterprise/tasks`, {
        method: 'POST',
        headers: authHeaders(),
        body: JSON.stringify(newTaskForm)
      })
      if (res.ok) {
        notify('success', 'Nova demanda criada no quadro Kanban!')
        setNewTaskModal(false)
        setNewTaskForm({
          title: '',
          description: '',
          priority: 'medium',
          assigned_to: authSession.user,
          company_id: 'alrigroup'
        })
        fetchData()
      } else {
        notify('error', 'Falha ao criar demanda')
      }
    } catch (err) {
      notify('error', 'Erro ao registrar demanda')
    }
  }

  // Move Task
  const handleMoveTask = async (taskId, newCol) => {
    // Optimistic update
    setTasks(prev => prev.map(t => t.id === taskId ? { ...t, column_status: newCol } : t))
    const base = getApiBaseUrl()
    try {
      await fetch(`${base}/arapi/enterprise/tasks/move`, {
        method: 'PUT',
        headers: authHeaders(),
        body: JSON.stringify({ id: taskId, column_status: newCol })
      })
    } catch (err) {
      fetchData()
    }
  }

  // Send Chat Message
  const handleSendMessage = async (e) => {
    e.preventDefault()
    if (!chatInput.trim()) return
    const text = chatInput.trim()
    setChatInput('')

    // Optimistic local add
    const tempMsg = {
      id: `tmp_${Date.now()}`,
      channel_id: activeChannel,
      sender_user: authSession.user,
      ciphertext: text,
      created_at: Date.now()
    }
    setMessages(prev => [...prev, tempMsg])

    const base = getApiBaseUrl()
    try {
      await fetch(`${base}/arapi/enterprise/messages`, {
        method: 'POST',
        headers: authHeaders(),
        body: JSON.stringify({ channel_id: activeChannel, message: text })
      })
    } catch (err) {
      console.warn('Chat send error:', err)
    }
  }

  const kanbanColumns = [
    { id: 'backlog', title: '📋 Backlog / Demandas', color: '#888888' },
    { id: 'in_progress', title: '⚡ Em Execução', color: '#ffaa00' },
    { id: 'review', title: '🛡️ Revisão & Auditoria', color: '#3399ff' },
    { id: 'done', title: '✅ Concluído', color: '#00cc66' }
  ]

  const channels = [
    { id: 'general', name: '#geral', desc: 'Canal Geral da Holding' },
    { id: 'exec', name: '#diretoria', desc: 'Comunicação Executiva & Estratégica' },
    { id: 'eng', name: '#engenharia', desc: 'Sistemas, ALRIOS e Kernel' },
    { id: 'ops', name: '#operacoes', desc: 'Operações e Gaming Detroit GG' }
  ]

  return (
    <div className="ent-container">
      {/* Top Notification Toast */}
      {notification.message && (
        <div className={`ent-toast ${notification.type}`}>
          {notification.type === 'success' ? '✓ ' : '⚠ '} {notification.message}
        </div>
      )}

      {/* Corporate Sidebar */}
      <aside className="ent-sidebar">
        <div className="ent-sidebar-brand">
          <div className="ent-logo-box">
            <span className="ent-logo-text">ALRI</span>
            <span className="ent-logo-badge">ENTERPRISE</span>
          </div>
        </div>

        <div className="ent-user-card">
          <div className="ent-avatar">{authSession.user.slice(0, 2).toUpperCase()}</div>
          <div className="ent-user-info">
            <span className="ent-username">{authSession.user}</span>
            <span className="ent-user-badge">{authSession.role.toUpperCase()}</span>
          </div>
        </div>

        <nav className="ent-nav">
          <button
            className={`ent-nav-item ${activeTab === 'dashboard' ? 'active' : ''}`}
            onClick={() => setActiveTab('dashboard')}
          >
            <span className="ent-icon">📊</span>
            <span>Dashboard Executivo</span>
          </button>

          <button
            className={`ent-nav-item ${activeTab === 'companies' ? 'active' : ''}`}
            onClick={() => setActiveTab('companies')}
          >
            <span className="ent-icon">🏢</span>
            <span>Holding & Subsidiárias</span>
          </button>

          <button
            className={`ent-nav-item ${activeTab === 'employees' ? 'active' : ''}`}
            onClick={() => setActiveTab('employees')}
          >
            <span className="ent-icon">👥</span>
            <span>Gestão de Pessoas & RBAC</span>
          </button>

          <button
            className={`ent-nav-item ${activeTab === 'kanban' ? 'active' : ''}`}
            onClick={() => setActiveTab('kanban')}
          >
            <span className="ent-icon">📋</span>
            <span>Demandas & Kanban</span>
          </button>

          <button
            className={`ent-nav-item ${activeTab === 'chat' ? 'active' : ''}`}
            onClick={() => setActiveTab('chat')}
          >
            <span className="ent-icon">💬</span>
            <span>Mensageria Segura</span>
          </button>
        </nav>

        <div className="ent-sidebar-footer">
          <button className="ent-btn-ghost" onClick={onBackToPublic}>
            ← Portal Público
          </button>
          <button className="ent-btn-danger" onClick={onLogout}>
            Desconectar
          </button>
        </div>
      </aside>

      {/* Main Enterprise Viewport */}
      <main className="ent-main">
        {/* Header Bar */}
        <header className="ent-header">
          <div>
            <h1 className="ent-header-title">
              {activeTab === 'dashboard' && 'Visão Geral & Métricas da Holding'}
              {activeTab === 'companies' && 'Estrutura Multi-Empresa & Subsidiárias'}
              {activeTab === 'employees' && 'Gestão de Colaboradores & Permissões Soberanas'}
              {activeTab === 'kanban' && 'Quadro de Demandas Executivas (Workflow)'}
              {activeTab === 'chat' && 'Mensageria Segura Inter-Empresas (E2EE)'}
            </h1>
            <p className="ent-header-sub">
              Ambiente de Governança Soberana ALRI GROUP — Tenant Ativo: <strong>{authSession.tenant}</strong>
            </p>
          </div>

          <div className="ent-header-actions">
            <button className="ent-btn-secondary" onClick={fetchData} title="Sincronizar Dados">
              🔄 Atualizar
            </button>
            {activeTab === 'employees' && (
              <button className="ent-btn-primary" onClick={() => setNewEmployeeModal(true)}>
                + Novo Colaborador
              </button>
            )}
            {activeTab === 'kanban' && (
              <button className="ent-btn-primary" onClick={() => setNewTaskModal(true)}>
                + Nova Demanda
              </button>
            )}
          </div>
        </header>

        {/* Content Area */}
        <div className="ent-content">
          {/* TAB 1: DASHBOARD */}
          {activeTab === 'dashboard' && (
            <div className="ent-dashboard-grid">
              <div className="ent-stat-card">
                <div className="ent-stat-header">
                  <span>🏢 EMPRESAS DO GRUPO</span>
                  <span className="ent-stat-pill">Holding</span>
                </div>
                <div className="ent-stat-value">{companies.length}</div>
                <div className="ent-stat-desc">Subsidiárias ativas e segregadas</div>
              </div>

              <div className="ent-stat-card">
                <div className="ent-stat-header">
                  <span>👥 QUADRO DE COLABORADORES</span>
                  <span className="ent-stat-pill">ARAUTH</span>
                </div>
                <div className="ent-stat-value">{employees.length}</div>
                <div className="ent-stat-desc">Identidades soberanas com RBAC ativo</div>
              </div>

              <div className="ent-stat-card">
                <div className="ent-stat-header">
                  <span>📋 DEMANDAS EM EXECUÇÃO</span>
                  <span className="ent-stat-pill">Kanban</span>
                </div>
                <div className="ent-stat-value">{tasks.filter(t => t.column_status !== 'done').length}</div>
                <div className="ent-stat-desc">{tasks.filter(t => t.column_status === 'done').length} demandas concluídas</div>
              </div>

              <div className="ent-stat-card highlight">
                <div className="ent-stat-header">
                  <span>🔒 SEGURANÇA DO COFRE</span>
                  <span className="ent-stat-pill pulse">PROTEGIDO</span>
                </div>
                <div className="ent-stat-value">FIPS 140-3</div>
                <div className="ent-stat-desc">Criptografia PBKDF2-HMAC-SHA512 + Mem Vault</div>
              </div>

              {/* Companies Quick Overview */}
              <div className="ent-card full-width">
                <h3 className="ent-card-title">Divisões Corporativas da Holding ALRI</h3>
                <div className="ent-companies-grid">
                  {companies.map(c => (
                    <div key={c.id} className="ent-company-mini-card">
                      <div className="ent-comp-badge">{c.code}</div>
                      <div>
                        <h4>{c.name}</h4>
                        <span className="ent-comp-id">{c.id} {c.is_holding ? '• Holding Master' : ''}</span>
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          )}

          {/* TAB 2: COMPANIES */}
          {activeTab === 'companies' && (
            <div className="ent-companies-view">
              <div className="ent-card">
                <h3 className="ent-card-title">Subsidiárias e Departamentos Registrados</h3>
                <div className="ent-table-wrapper">
                  <table className="ent-table">
                    <thead>
                      <tr>
                        <th>Código</th>
                        <th>Empresa / Subsidiária</th>
                        <th>Tipo</th>
                        <th>Departamentos</th>
                        <th>Status</th>
                      </tr>
                    </thead>
                    <tbody>
                      {companies.map(c => {
                        const compDepts = departments.filter(d => d.company_id === c.id)
                        return (
                          <tr key={c.id}>
                            <td><span className="ent-tag">{c.code}</span></td>
                            <td>
                              <strong>{c.name}</strong>
                              <div className="ent-text-muted">ID: {c.id}</div>
                            </td>
                            <td>{c.is_holding ? <span className="ent-badge-gold">HOLDING MASTER</span> : <span className="ent-badge-blue">SUBSIDIÁRIA</span>}</td>
                            <td>
                              <div className="ent-dept-tags">
                                {compDepts.length > 0 ? compDepts.map(d => (
                                  <span key={d.id} className="ent-dept-pill">{d.name}</span>
                                )) : <span className="ent-text-muted">Sem departamentos</span>}
                              </div>
                            </td>
                            <td><span className="ent-status-online">● Operacional</span></td>
                          </tr>
                        )
                      })}
                    </tbody>
                  </table>
                </div>
              </div>
            </div>
          )}

          {/* TAB 3: EMPLOYEES & RBAC */}
          {activeTab === 'employees' && (
            <div className="ent-employees-view">
              <div className="ent-card">
                <div className="ent-card-header-bar">
                  <h3 className="ent-card-title">Quadro de Funcionários e Credenciais ARAUTH</h3>
                  <span className="ent-card-count">{employees.length} colaboradores</span>
                </div>

                <div className="ent-table-wrapper">
                  <table className="ent-table">
                    <thead>
                      <tr>
                        <th>Usuário</th>
                        <th>Nome Completo</th>
                        <th>Empresa & Departamento</th>
                        <th>Cargo & Nível</th>
                        <th>Contato</th>
                        <th>Ações</th>
                      </tr>
                    </thead>
                    <tbody>
                      {employees.map(emp => {
                        const comp = companies.find(c => c.id === emp.company_id)
                        const dept = departments.find(d => d.id === emp.department_id)
                        return (
                          <tr key={emp.id}>
                            <td>
                              <div className="ent-user-cell">
                                <div className="ent-cell-avatar">{emp.username.slice(0, 2).toUpperCase()}</div>
                                <div>
                                  <strong>{emp.username}</strong>
                                  <div className="ent-text-muted">{emp.is_active ? 'Ativo' : 'Inativo'}</div>
                                </div>
                              </div>
                            </td>
                            <td>{emp.full_name}</td>
                            <td>
                              <div><strong>{comp ? comp.name : emp.company_id}</strong></div>
                              <div className="ent-text-muted">{dept ? dept.name : emp.department_id}</div>
                            </td>
                            <td>
                              <div>{emp.position_title}</div>
                              <span className={`ent-level-badge level-${emp.hierarchy_level}`}>
                                Nível {emp.hierarchy_level} {emp.hierarchy_level === 1 ? '(Master)' : emp.hierarchy_level <= 3 ? '(Diretoria)' : '(Operacional)'}
                              </span>
                            </td>
                            <td>
                              <div>{emp.email}</div>
                              <div className="ent-text-muted">{emp.phone || '-'}</div>
                            </td>
                            <td>
                              <div className="ent-action-buttons">
                                <button
                                  className="ent-btn-sm-warning"
                                  onClick={() => setPasswordModal({ open: true, targetUser: emp.username })}
                                  title="Trocar Senha e Revogar Sessões"
                                >
                                  🔑 Senha
                                </button>
                              </div>
                            </td>
                          </tr>
                        )
                      })}
                    </tbody>
                  </table>
                </div>
              </div>
            </div>
          )}

          {/* TAB 4: KANBAN WORKFLOW */}
          {activeTab === 'kanban' && (
            <div className="ent-kanban-board">
              {kanbanColumns.map(col => {
                const colTasks = tasks.filter(t => t.column_status === col.id)
                return (
                  <div key={col.id} className="ent-kanban-col">
                    <div className="ent-kanban-col-header" style={{ borderTopColor: col.color }}>
                      <span className="ent-kanban-title">{col.title}</span>
                      <span className="ent-kanban-badge">{colTasks.length}</span>
                    </div>

                    <div className="ent-kanban-cards">
                      {colTasks.map(task => (
                        <div key={task.id} className="ent-kanban-card">
                          <div className="ent-kanban-card-top">
                            <span className={`ent-priority-pill ${task.priority}`}>
                              {task.priority.toUpperCase()}
                            </span>
                            <span className="ent-task-company">{task.company_id}</span>
                          </div>

                          <h4 className="ent-task-title">{task.title}</h4>
                          {task.description && (
                            <p className="ent-task-desc">{task.description}</p>
                          )}

                          <div className="ent-task-footer">
                            <span className="ent-task-assigned">👤 {task.assigned_to}</span>
                            <div className="ent-task-moves">
                              {col.id !== 'backlog' && (
                                <button
                                  className="ent-move-btn"
                                  onClick={() => handleMoveTask(task.id, col.id === 'done' ? 'review' : col.id === 'review' ? 'in_progress' : 'backlog')}
                                  title="Mover para esquerda"
                                >
                                  ←
                                </button>
                              )}
                              {col.id !== 'done' && (
                                <button
                                  className="ent-move-btn"
                                  onClick={() => handleMoveTask(task.id, col.id === 'backlog' ? 'in_progress' : col.id === 'in_progress' ? 'review' : 'done')}
                                  title="Mover para direita"
                                >
                                  →
                                </button>
                              )}
                            </div>
                          </div>
                        </div>
                      ))}

                      {colTasks.length === 0 && (
                        <div className="ent-kanban-empty">Nenhuma demanda</div>
                      )}
                    </div>
                  </div>
                )
              })}
            </div>
          )}

          {/* TAB 5: SECURE MESSAGING */}
          {activeTab === 'chat' && (
            <div className="ent-chat-container">
              {/* Channel Selector */}
              <div className="ent-chat-channels">
                <div className="ent-chat-channels-header">Canais Corporativos</div>
                {channels.map(ch => (
                  <button
                    key={ch.id}
                    className={`ent-channel-btn ${activeChannel === ch.id ? 'active' : ''}`}
                    onClick={() => setActiveChannel(ch.id)}
                  >
                    <div className="ent-channel-name">{ch.name}</div>
                    <div className="ent-channel-desc">{ch.desc}</div>
                  </button>
                ))}
              </div>

              {/* Message Thread */}
              <div className="ent-chat-thread-box">
                <div className="ent-chat-header">
                  <div>
                    <h3>#{activeChannel}</h3>
                    <span className="ent-chat-e2ee-badge">🔒 Criptografia Soberana Ativa</span>
                  </div>
                </div>

                <div className="ent-chat-messages">
                  {messages
                    .filter(m => !m.channel_id || m.channel_id === activeChannel)
                    .map(m => {
                      const isMe = m.sender_user === authSession.user
                      return (
                        <div key={m.id} className={`ent-chat-msg ${isMe ? 'me' : 'other'}`}>
                          <div className="ent-msg-meta">
                            <span className="ent-msg-user">{m.sender_user}</span>
                            <span className="ent-msg-time">
                              {new Date(m.created_at || Date.now()).toLocaleTimeString()}
                            </span>
                          </div>
                          <div className="ent-msg-body">{m.ciphertext}</div>
                        </div>
                      )
                    })}
                </div>

                <form className="ent-chat-input-bar" onSubmit={handleSendMessage}>
                  <input
                    type="text"
                    placeholder={`Conversar em #${activeChannel}...`}
                    value={chatInput}
                    onChange={(e) => setChatInput(e.target.value)}
                  />
                  <button type="submit" className="ent-btn-primary">
                    Enviar
                  </button>
                </form>
              </div>
            </div>
          )}
        </div>
      </main>

      {/* MODAL: NOVO COLABORADOR */}
      {newEmployeeModal && (
        <div className="ent-modal-overlay">
          <div className="ent-modal">
            <div className="ent-modal-header">
              <h3>Cadastrar Novo Colaborador</h3>
              <button className="ent-modal-close" onClick={() => setNewEmployeeModal(false)}>✕</button>
            </div>
            <form onSubmit={handleCreateEmployee}>
              <div className="ent-form-grid">
                <div className="ent-form-group">
                  <label>Usuário de Login (ARAUTH)</label>
                  <input
                    type="text"
                    required
                    placeholder="ex: joao.silva"
                    value={newEmpForm.username}
                    onChange={e => setNewEmpForm({ ...newEmpForm, username: e.target.value })}
                  />
                </div>

                <div className="ent-form-group">
                  <label>Senha Inicial</label>
                  <input
                    type="password"
                    required
                    placeholder="Senha forte inicial"
                    value={newEmpForm.password}
                    onChange={e => setNewEmpForm({ ...newEmpForm, password: e.target.value })}
                  />
                </div>

                <div className="ent-form-group full">
                  <label>Nome Completo</label>
                  <input
                    type="text"
                    required
                    placeholder="Nome completo do colaborador"
                    value={newEmpForm.full_name}
                    onChange={e => setNewEmpForm({ ...newEmpForm, full_name: e.target.value })}
                  />
                </div>

                <div className="ent-form-group">
                  <label>E-mail Corporativo</label>
                  <input
                    type="email"
                    required
                    placeholder="colaborador@alrigroup.com"
                    value={newEmpForm.email}
                    onChange={e => setNewEmpForm({ ...newEmpForm, email: e.target.value })}
                  />
                </div>

                <div className="ent-form-group">
                  <label>Telefone / WhatsApp</label>
                  <input
                    type="text"
                    placeholder="+55 11 98888-0000"
                    value={newEmpForm.phone}
                    onChange={e => setNewEmpForm({ ...newEmpForm, phone: e.target.value })}
                  />
                </div>

                <div className="ent-form-group">
                  <label>Empresa / Subsidiária</label>
                  <select
                    value={newEmpForm.company_id}
                    onChange={e => setNewEmpForm({ ...newEmpForm, company_id: e.target.value })}
                  >
                    {companies.map(c => (
                      <option key={c.id} value={c.id}>{c.name}</option>
                    ))}
                  </select>
                </div>

                <div className="ent-form-group">
                  <label>Departamento</label>
                  <select
                    value={newEmpForm.department_id}
                    onChange={e => setNewEmpForm({ ...newEmpForm, department_id: e.target.value })}
                  >
                    {departments
                      .filter(d => d.company_id === newEmpForm.company_id)
                      .map(d => (
                        <option key={d.id} value={d.id}>{d.name}</option>
                      ))}
                  </select>
                </div>

                <div className="ent-form-group">
                  <label>Título do Cargo</label>
                  <input
                    type="text"
                    required
                    placeholder="ex: Coordenador de Infra"
                    value={newEmpForm.position_title}
                    onChange={e => setNewEmpForm({ ...newEmpForm, position_title: e.target.value })}
                  />
                </div>

                <div className="ent-form-group">
                  <label>Nível Hierárquico</label>
                  <select
                    value={newEmpForm.hierarchy_level}
                    onChange={e => setNewEmpForm({ ...newEmpForm, hierarchy_level: parseInt(e.target.value, 10) })}
                  >
                    <option value={2}>Nível 2 — Diretoria Executiva</option>
                    <option value={3}>Nível 3 — Gerência / Coordenação</option>
                    <option value={10}>Nível 10 — Colaborador / Especialista</option>
                  </select>
                </div>
              </div>

              <div className="ent-modal-footer">
                <button type="button" className="ent-btn-ghost" onClick={() => setNewEmployeeModal(false)}>
                  Cancelar
                </button>
                <button type="submit" className="ent-btn-primary">
                  Cadastrar Colaborador
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* MODAL: ALTERAR SENHA */}
      {passwordModal.open && (
        <div className="ent-modal-overlay">
          <div className="ent-modal small">
            <div className="ent-modal-header">
              <h3>Alterar Senha do Usuário</h3>
              <button className="ent-modal-close" onClick={() => setPasswordModal({ open: false, targetUser: '' })}>✕</button>
            </div>
            <form onSubmit={handleChangePassword}>
              <p className="ent-modal-desc">
                Defina a nova senha para o usuário <strong>{passwordModal.targetUser}</strong>. Todas as sessões ativas anteriores serão revogadas no cofre ARAUTH.
              </p>
              <div className="ent-form-group">
                <label>Nova Senha</label>
                <input
                  type="password"
                  required
                  placeholder="Digite a nova senha segura"
                  value={newPassInput}
                  onChange={e => setNewPassInput(e.target.value)}
                />
              </div>

              <div className="ent-modal-footer">
                <button type="button" className="ent-btn-ghost" onClick={() => setPasswordModal({ open: false, targetUser: '' })}>
                  Cancelar
                </button>
                <button type="submit" className="ent-btn-primary">
                  Atualizar Senha
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* MODAL: NOVA TAREFA KANBAN */}
      {newTaskModal && (
        <div className="ent-modal-overlay">
          <div className="ent-modal">
            <div className="ent-modal-header">
              <h3>Criar Nova Demanda (Kanban)</h3>
              <button className="ent-modal-close" onClick={() => setNewTaskModal(false)}>✕</button>
            </div>
            <form onSubmit={handleCreateTask}>
              <div className="ent-form-group">
                <label>Título da Demanda</label>
                <input
                  type="text"
                  required
                  placeholder="ex: Implementação de nova API"
                  value={newTaskForm.title}
                  onChange={e => setNewTaskForm({ ...newTaskForm, title: e.target.value })}
                />
              </div>

              <div className="ent-form-group">
                <label>Descrição detalhada</label>
                <textarea
                  rows={3}
                  placeholder="Escopo, objetivos e critérios de aceite..."
                  value={newTaskForm.description}
                  onChange={e => setNewTaskForm({ ...newTaskForm, description: e.target.value })}
                />
              </div>

              <div className="ent-form-grid">
                <div className="ent-form-group">
                  <label>Prioridade</label>
                  <select
                    value={newTaskForm.priority}
                    onChange={e => setNewTaskForm({ ...newTaskForm, priority: e.target.value })}
                  >
                    <option value="low">Baixa</option>
                    <option value="medium">Média</option>
                    <option value="high">Alta</option>
                    <option value="critical">Crítica (Urgência)</option>
                  </select>
                </div>

                <div className="ent-form-group">
                  <label>Responsável</label>
                  <select
                    value={newTaskForm.assigned_to}
                    onChange={e => setNewTaskForm({ ...newTaskForm, assigned_to: e.target.value })}
                  >
                    {employees.map(emp => (
                      <option key={emp.id} value={emp.username}>{emp.full_name} ({emp.username})</option>
                    ))}
                  </select>
                </div>

                <div className="ent-form-group">
                  <label>Empresa</label>
                  <select
                    value={newTaskForm.company_id}
                    onChange={e => setNewTaskForm({ ...newTaskForm, company_id: e.target.value })}
                  >
                    {companies.map(c => (
                      <option key={c.id} value={c.id}>{c.name}</option>
                    ))}
                  </select>
                </div>
              </div>

              <div className="ent-modal-footer">
                <button type="button" className="ent-btn-ghost" onClick={() => setNewTaskModal(false)}>
                  Cancelar
                </button>
                <button type="submit" className="ent-btn-primary">
                  Criar Demanda
                </button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  )
}
