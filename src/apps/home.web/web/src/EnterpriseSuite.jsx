import React, { useState, useEffect } from 'react'

export default function EnterpriseSuite({ userSession, authSession, onLogout, onBackToPortal, onBackToPublic }) {
  const [activeTab, setActiveTab] = useState('dashboard') // dashboard, companies, employees, roles, tasks, chat
  const [selectedCompanyFilter, setSelectedCompanyFilter] = useState('*')
  
  const handleBackToPublic = onBackToPublic || onBackToPortal

  // Data States
  const [me, setMe] = useState(null)
  const [companies, setCompanies] = useState([])
  const [departments, setDepartments] = useState([])
  const [employees, setEmployees] = useState([])
  const [roles, setRoles] = useState([])
  const [channels, setChannels] = useState([])
  const [tasks, setTasks] = useState([])
  const [messages, setMessages] = useState([])
  const [activeChannel, setActiveChannel] = useState('general')
  const [chatInput, setChatInput] = useState('')

  // Modals
  const [isEmployeeModalOpen, setIsEmployeeModalOpen] = useState(false)
  const [isEditEmployeeModalOpen, setIsEditEmployeeModalOpen] = useState(false)
  const [isPasswdModalOpen, setIsPasswdModalOpen] = useState(false)
  const [isRoleModalOpen, setIsRoleModalOpen] = useState(false)
  const [isChannelModalOpen, setIsChannelModalOpen] = useState(false)
  const [isTaskModalOpen, setIsTaskModalOpen] = useState(false)

  // Forms
  const [empForm, setEmpForm] = useState({
    username: '', password: '', full_name: '', email: '', phone: '',
    company_id: 'alrigroup', department_id: 'dept_eng', position_title: 'Engenheiro Pleno', hierarchy_level: 10
  })

  const [editEmpForm, setEditEmpForm] = useState({
    id: '', username: '', full_name: '', email: '', phone: '',
    company_id: '', department_id: '', position_title: '', hierarchy_level: 10, is_active: 1
  })

  const [passwdForm, setPasswdForm] = useState({
    username: '', full_name: '', new_password: '', admin_password: ''
  })

  const [roleForm, setRoleForm] = useState({
    name: '', company_id: 'alrigroup', hierarchy_level: 10,
    permissions: ['tasks.own', 'chat.send']
  })

  const [channelForm, setChannelForm] = useState({
    name: '', desc: '', company_id: 'alrigroup', is_private: false
  })

  const [taskForm, setTaskForm] = useState({
    title: '', description: '', priority: 'medium', assigned_to: '', company_id: 'alrigroup'
  })

  // Notifications
  const [feedback, setFeedback] = useState({ type: '', message: '' })

  const getAuthToken = () => {
    return localStorage.getItem('ar_session_token') || (userSession && userSession.sessionToken) || (authSession && authSession.sessionToken) || ''
  }

  const getApiUrl = (endpoint) => {
    if (typeof window !== 'undefined' && (window.location.port === '3001' || window.location.port === '5173')) {
      return `http://${window.location.hostname}:9670/arapi/enterprise${endpoint}`
    }
    return `/arapi/enterprise${endpoint}`
  }

  const notify = (msg, type = 'success') => {
    setFeedback({ type, message: msg })
    setTimeout(() => setFeedback({ type: '', message: '' }), 4000)
  }

  // Load Me
  const fetchMe = async () => {
    const token = getAuthToken()
    if (!token) return
    try {
      const res = await fetch(getApiUrl('/me'), {
        headers: { 'Authorization': `Bearer ${token}` }
      })
      if (res.ok) {
        const data = await res.json()
        setMe(data)
        if (data.hierarchy_level > 1 && data.company_id) {
          setSelectedCompanyFilter(data.company_id)
        }
      }
    } catch (e) {
      console.error('Error fetching /me', e)
    }
  }

  // Load All Core Data
  const fetchData = async () => {
    const token = getAuthToken()
    if (!token) return

    const headers = { 'Authorization': `Bearer ${token}` }

    try {
      const [resComp, resDept, resEmp, resRoles, resChannels, resTasks] = await Promise.all([
        fetch(getApiUrl('/companies'), { headers }).then(r => r.json()),
        fetch(getApiUrl('/departments'), { headers }).then(r => r.json()),
        fetch(getApiUrl('/employees'), { headers }).then(r => r.json()),
        fetch(getApiUrl('/roles'), { headers }).then(r => r.json()),
        fetch(getApiUrl('/channels'), { headers }).then(r => r.json()),
        fetch(getApiUrl('/tasks'), { headers }).then(r => r.json())
      ])

      if (resComp.companies) setCompanies(resComp.companies)
      if (resDept.departments) setDepartments(resDept.departments)
      if (resEmp.employees) setEmployees(resEmp.employees)
      if (resRoles.roles) setRoles(resRoles.roles)
      if (resChannels.channels) setChannels(resChannels.channels)
      if (resTasks.tasks) setTasks(resTasks.tasks)
    } catch (e) {
      console.error('Error loading enterprise suite data', e)
    }
  }

  // Load Messages for Active Channel
  const fetchMessages = async () => {
    const token = getAuthToken()
    if (!token) return
    try {
      const res = await fetch(getApiUrl(`/messages?channel_id=${activeChannel}`), {
        headers: { 'Authorization': `Bearer ${token}` }
      })
      if (res.ok) {
        const data = await res.json()
        if (data.messages) setMessages(data.messages)
      }
    } catch (e) {
      console.error('Error fetching messages', e)
    }
  }

  useEffect(() => {
    fetchMe()
    fetchData()
  }, [])

  useEffect(() => {
    fetchMessages()
    const interval = setInterval(fetchMessages, 3000)
    return () => clearInterval(interval)
  }, [activeChannel])

  // Handlers: Employees
  const handleCreateEmployee = async (e) => {
    e.preventDefault()
    const token = getAuthToken()
    try {
      const res = await fetch(getApiUrl('/employees'), {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}`, 'Content-Type': 'application/json' },
        body: JSON.stringify(empForm)
      })
      const data = await res.json()
      if (res.ok && data.status === 'success') {
        notify('Colaborador cadastrado e sincronizado no cofre ARAUTH com sucesso!')
        setIsEmployeeModalOpen(false)
        setEmpForm({
          username: '', password: '', full_name: '', email: '', phone: '',
          company_id: 'alrigroup', department_id: 'dept_eng', position_title: 'Engenheiro Pleno', hierarchy_level: 10
        })
        fetchData()
      } else {
        notify(data.error || 'Erro ao cadastrar colaborador.', 'error')
      }
    } catch (err) {
      notify('Erro de conexão ao criar colaborador.', 'error')
    }
  }

  const openEditModal = (emp) => {
    setEditEmpForm({
      id: emp.id,
      username: emp.username,
      full_name: emp.full_name,
      email: emp.email || '',
      phone: emp.phone || '',
      company_id: emp.company_id || 'alrigroup',
      department_id: emp.department_id || 'dept_eng',
      position_title: emp.position_title || '',
      hierarchy_level: emp.hierarchy_level || 10,
      is_active: emp.is_active ? 1 : 0
    })
    setIsEditEmployeeModalOpen(true)
  }

  const handleUpdateEmployee = async (e) => {
    e.preventDefault()
    const token = getAuthToken()
    try {
      const res = await fetch(getApiUrl('/employees/update'), {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}`, 'Content-Type': 'application/json' },
        body: JSON.stringify(editEmpForm)
      })
      const data = await res.json()
      if (res.ok && data.status === 'success') {
        notify('Dados do colaborador atualizados com sucesso!')
        setIsEditEmployeeModalOpen(false)
        fetchData()
      } else {
        notify(data.error || 'Erro ao atualizar colaborador.', 'error')
      }
    } catch (err) {
      notify('Erro de conexão ao atualizar colaborador.', 'error')
    }
  }

  const openPasswdModal = (emp) => {
    setPasswdForm({
      username: emp.username,
      full_name: emp.full_name,
      new_password: '',
      admin_password: ''
    })
    setIsPasswdModalOpen(true)
  }

  const handleResetPassword = async (e) => {
    e.preventDefault()
    if (!passwdForm.new_password || !passwdForm.admin_password) {
      notify('Preencha a nova senha e a sua senha de administrador.', 'error')
      return
    }
    const token = getAuthToken()
    try {
      const res = await fetch(getApiUrl('/employees/passwd'), {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}`, 'Content-Type': 'application/json' },
        body: JSON.stringify(passwdForm)
      })
      const data = await res.json()
      if (res.ok && data.status === 'success') {
        notify(`Senha de ${passwdForm.username} redefinida com sucesso!`)
        setIsPasswdModalOpen(false)
      } else {
        notify(data.error || 'Falha na verificação da senha do administrador.', 'error')
      }
    } catch (err) {
      notify('Erro de conexão ao redefinir senha.', 'error')
    }
  }

  const handleDeleteEmployee = async (emp) => {
    if (!window.confirm(`Tem certeza que deseja remover o colaborador ${emp.full_name} (@${emp.username})?`)) return
    const token = getAuthToken()
    try {
      const res = await fetch(getApiUrl('/employees/delete'), {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}`, 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: emp.id })
      })
      if (res.ok) {
        notify('Colaborador removido com sucesso.')
        fetchData()
      } else {
        const data = await res.json()
        notify(data.error || 'Erro ao remover colaborador.', 'error')
      }
    } catch (e) {
      notify('Erro ao conectar ao servidor.', 'error')
    }
  }

  // Handlers: Roles (RBAC)
  const togglePermission = (perm) => {
    setRoleForm(prev => {
      const exists = prev.permissions.includes(perm)
      const updated = exists ? prev.permissions.filter(p => p !== perm) : [...prev.permissions, perm]
      return { ...prev, permissions: updated }
    })
  }

  const handleCreateRole = async (e) => {
    e.preventDefault()
    const token = getAuthToken()
    try {
      const payload = {
        name: roleForm.name,
        company_id: roleForm.company_id,
        hierarchy_level: parseInt(roleForm.hierarchy_level, 10),
        permissions: roleForm.permissions.join(',')
      }
      const res = await fetch(getApiUrl('/roles'), {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}`, 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      })
      const data = await res.json()
      if (res.ok && data.status === 'success') {
        notify('Novo cargo e matriz RBAC criados com sucesso!')
        setIsRoleModalOpen(false)
        setRoleForm({ name: '', company_id: 'alrigroup', hierarchy_level: 10, permissions: ['tasks.own', 'chat.send'] })
        fetchData()
      } else {
        notify(data.error || 'Erro ao criar cargo.', 'error')
      }
    } catch (e) {
      notify('Erro de conexão ao criar cargo.', 'error')
    }
  }

  const handleDeleteRole = async (roleId) => {
    if (!window.confirm('Deseja excluir este cargo?')) return
    const token = getAuthToken()
    try {
      await fetch(getApiUrl('/roles/delete'), {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}`, 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: roleId })
      })
      notify('Cargo excluído.')
      fetchData()
    } catch (e) {
      notify('Erro ao excluir cargo.', 'error')
    }
  }

  // Handlers: Channels
  const handleCreateChannel = async (e) => {
    e.preventDefault()
    const token = getAuthToken()
    try {
      const res = await fetch(getApiUrl('/channels'), {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}`, 'Content-Type': 'application/json' },
        body: JSON.stringify(channelForm)
      })
      const data = await res.json()
      if (res.ok && data.status === 'success') {
        notify('Canal de comunicação criado com sucesso!')
        setIsChannelModalOpen(false)
        setChannelForm({ name: '', desc: '', company_id: 'alrigroup', is_private: false })
        fetchData()
      } else {
        notify(data.error || 'Erro ao criar canal.', 'error')
      }
    } catch (e) {
      notify('Erro de conexão ao criar canal.', 'error')
    }
  }

  // Handlers: Tasks
  const handleCreateTask = async (e) => {
    e.preventDefault()
    const token = getAuthToken()
    try {
      const res = await fetch(getApiUrl('/tasks'), {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}`, 'Content-Type': 'application/json' },
        body: JSON.stringify(taskForm)
      })
      const data = await res.json()
      if (res.ok && data.status === 'success') {
        notify('Demanda cadastrada no Kanban!')
        setIsTaskModalOpen(false)
        setTaskForm({ title: '', description: '', priority: 'medium', assigned_to: '', company_id: 'alrigroup' })
        fetchData()
      } else {
        notify(data.error || 'Erro ao criar demanda.', 'error')
      }
    } catch (e) {
      notify('Erro de conexão ao criar demanda.', 'error')
    }
  }

  const handleMoveTask = async (taskId, newCol) => {
    const token = getAuthToken()
    try {
      await fetch(getApiUrl('/tasks/move'), {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}`, 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: taskId, column_status: newCol })
      })
      fetchData()
    } catch (e) {
      console.error('Error moving task', e)
    }
  }

  // Handlers: Chat
  const handleSendMessage = async (e) => {
    e.preventDefault()
    if (!chatInput.trim()) return
    const token = getAuthToken()
    try {
      const res = await fetch(getApiUrl('/messages'), {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}`, 'Content-Type': 'application/json' },
        body: JSON.stringify({ channel_id: activeChannel, message: chatInput.trim() })
      })
      if (res.ok) {
        setChatInput('')
        fetchMessages()
      }
    } catch (e) {
      console.error('Error sending message', e)
    }
  }

  // Filtered Lists
  const filteredEmployees = employees.filter(emp => {
    if (selectedCompanyFilter === '*') return true
    return emp.company_id === selectedCompanyFilter
  })

  const filteredTasks = tasks.filter(t => {
    if (selectedCompanyFilter === '*') return true
    return t.company_id === selectedCompanyFilter
  })

  const filteredRoles = roles.filter(r => {
    if (selectedCompanyFilter === '*') return true
    return r.company_id === selectedCompanyFilter
  })

  const isMasterAdmin = me && (me.hierarchy_level === 1 || me.user === 'alexsanderalri' || me.username === 'alexsanderalri')
  const canManageRoles = me && (me.hierarchy_level <= 2 || isMasterAdmin)
  const canCreateEmployees = me && (me.hierarchy_level <= 3 || isMasterAdmin)

  return (
    <div className="ent-root">
      {/* Toast Feedback */}
      {feedback.message && (
        <div className={`ent-toast ${feedback.type === 'error' ? 'ent-toast-err' : 'ent-toast-ok'}`}>
          <i className={feedback.type === 'error' ? 'fa-solid fa-triangle-exclamation' : 'fa-solid fa-circle-check'} />
          <span>{feedback.message}</span>
        </div>
      )}

      {/* Top Navbar */}
      <header className="ent-header">
        <div className="ent-brand">
          <div className="ent-logo-badge">ALRI</div>
          <div>
            <h1 className="ent-title">Enterprise Suite</h1>
            <span className="ent-subtitle">Área Restrita Corporativa & Governanca Soberana</span>
          </div>
        </div>

        {/* Multi-Company Global Filter */}
        <div className="ent-header-center">
          <div className="ent-filter-pill">
            <i className="fa-solid fa-building" />
            <span className="ent-filter-label">Empresa:</span>
            <select
              value={selectedCompanyFilter}
              onChange={(e) => setSelectedCompanyFilter(e.target.value)}
              className="ent-select-filter"
            >
              <option value="*">Todas as Empresas (Holding)</option>
              {companies.map(c => (
                <option key={c.id} value={c.id}>{c.name} ({c.code})</option>
              ))}
            </select>
            {selectedCompanyFilter !== '*' && (
              <span className="ent-badge ent-badge-lock">
                <i className="fa-solid fa-lock" /> Filtrado
              </span>
            )}
          </div>
        </div>

        <div className="ent-user-panel">
          <div className="ent-user-info">
            <span className="ent-user-name">{me?.full_name || me?.user || 'Administrador'}</span>
            <span className="ent-user-role">
              {me?.position_title || me?.role} &bull; Nível {me?.hierarchy_level || 1}
            </span>
          </div>
          <button className="ent-btn-ghost" onClick={handleBackToPublic} title="Voltar ao Portal Público">
            <i className="fa-solid fa-house" /> Portal
          </button>
          <button className="ent-btn-danger-sm" onClick={onLogout} title="Encerrar Sessão">
            <i className="fa-solid fa-power-off" />
          </button>
        </div>
      </header>

      {/* Main Container */}
      <div className="ent-body">
        {/* Sidebar */}
        <aside className="ent-sidebar">
          <nav className="ent-nav">
            <button
              className={`ent-nav-item ${activeTab === 'dashboard' ? 'active' : ''}`}
              onClick={() => setActiveTab('dashboard')}
            >
              <i className="fa-solid fa-chart-pie" /> Dashboard Executivo
            </button>
            <button
              className={`ent-nav-item ${activeTab === 'companies' ? 'active' : ''}`}
              onClick={() => setActiveTab('companies')}
            >
              <i className="fa-solid fa-sitemap" /> Holding & Divisões
            </button>
            <button
              className={`ent-nav-item ${activeTab === 'employees' ? 'active' : ''}`}
              onClick={() => setActiveTab('employees')}
            >
              <i className="fa-solid fa-users-gear" /> Quadro de Funcionários
            </button>
            {canManageRoles && (
              <button
                className={`ent-nav-item ${activeTab === 'roles' ? 'active' : ''}`}
                onClick={() => setActiveTab('roles')}
              >
                <i className="fa-solid fa-shield-halved" /> Cargos & RBAC
              </button>
            )}
            <button
              className={`ent-nav-item ${activeTab === 'tasks' ? 'active' : ''}`}
              onClick={() => setActiveTab('tasks')}
            >
              <i className="fa-solid fa-list-check" /> Demandas & Kanban
            </button>
            <button
              className={`ent-nav-item ${activeTab === 'chat' ? 'active' : ''}`}
              onClick={() => setActiveTab('chat')}
            >
              <i className="fa-solid fa-comments" /> Mensageria Segura
            </button>
          </nav>

          <div className="ent-sidebar-footer">
            <div className="ent-sec-status">
              <span className="ent-pulse" />
              <span>ARAUTH Vault &bull; FIPS 140-3</span>
            </div>
            <div className="ent-version-info">ALRIOS v1.0.0 &bull; Enterprise</div>
          </div>
        </aside>

        {/* Content Area */}
        <main className="ent-content">
          {/* TAB 1: DASHBOARD */}
          {activeTab === 'dashboard' && (
            <div className="ent-tab-content">
              <div className="ent-section-title">
                <h2>Visão Executiva Consolidada</h2>
                <p>Governança operacional de subsidiárias, auditoria de acesso e fluxo de demandas.</p>
              </div>

              <div className="ent-stats-grid">
                <div className="ent-card ent-stat-card">
                  <div className="ent-stat-icon red"><i className="fa-solid fa-building" /></div>
                  <div>
                    <span className="ent-stat-num">{companies.length}</span>
                    <span className="ent-stat-lbl">Empresas do Grupo</span>
                  </div>
                </div>
                <div className="ent-card ent-stat-card">
                  <div className="ent-stat-icon gold"><i className="fa-solid fa-users" /></div>
                  <div>
                    <span className="ent-stat-num">{filteredEmployees.length}</span>
                    <span className="ent-stat-lbl">Colaboradores no Escopo</span>
                  </div>
                </div>
                <div className="ent-card ent-stat-card">
                  <div className="ent-stat-icon blue"><i className="fa-solid fa-list-check" /></div>
                  <div>
                    <span className="ent-stat-num">{filteredTasks.length}</span>
                    <span className="ent-stat-lbl">Demandas Ativas</span>
                  </div>
                </div>
                <div className="ent-card ent-stat-card">
                  <div className="ent-stat-icon green"><i className="fa-solid fa-shield-check" /></div>
                  <div>
                    <span className="ent-stat-num">100%</span>
                    <span className="ent-stat-lbl">Integridade ARAUTH</span>
                  </div>
                </div>
              </div>

              <div className="ent-grid-2col">
                <div className="ent-card">
                  <h3 className="ent-card-title"><i className="fa-solid fa-layer-group" /> Estrutura Organizacional</h3>
                  <div className="ent-tree-list">
                    {companies.map(c => (
                      <div key={c.id} className="ent-tree-item">
                        <div className="ent-tree-comp">
                          <span className={`ent-badge ${c.is_holding ? 'ent-badge-gold' : 'ent-badge-gray'}`}>
                            {c.is_holding ? 'Holding Master' : 'Subsidiária'}
                          </span>
                          <strong>{c.name}</strong> ({c.code})
                        </div>
                        <div className="ent-tree-depts">
                          {departments.filter(d => d.company_id === c.id).map(d => (
                            <span key={d.id} className="ent-dept-pill">
                              <i className="fa-solid fa-folder-tree" /> {d.name}
                            </span>
                          ))}
                        </div>
                      </div>
                    ))}
                  </div>
                </div>

                <div className="ent-card">
                  <h3 className="ent-card-title"><i className="fa-solid fa-clock-rotate-left" /> Atividades Recentes</h3>
                  <div className="ent-activity-stream">
                    <div className="ent-act-item">
                      <i className="fa-solid fa-key gold" />
                      <div>
                        <strong>Cofre ARAUTH Ativo</strong>
                        <p>Autenticação mútua e sessões auditadas por hardware e IPC.</p>
                      </div>
                    </div>
                    <div className="ent-act-item">
                      <i className="fa-solid fa-user-shield red" />
                      <div>
                        <strong>Hierarquia RBAC Soberana</strong>
                        <p>Delegação de acessos em níveis de 1 a 10 com confirmação de senha de administrador.</p>
                      </div>
                    </div>
                    <div className="ent-act-item">
                      <i className="fa-solid fa-diagram-project blue" />
                      <div>
                        <strong>Demandas Multi-Empresa</strong>
                        <p>Controle de tarefas sincronizado no kernel soberano.</p>
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          )}

          {/* TAB 2: COMPANIES */}
          {activeTab === 'companies' && (
            <div className="ent-tab-content">
              <div className="ent-section-title">
                <h2>Holding & Subsidiárias</h2>
                <p>Isolamento contábil, operacional e de acesso no mesmo cluster ALRIOS.</p>
              </div>

              <div className="ent-grid-cards">
                {companies.map(c => (
                  <div key={c.id} className="ent-card ent-company-card">
                    <div className="ent-comp-head">
                      <div className="ent-comp-icon"><i className="fa-solid fa-building" /></div>
                      <div>
                        <span className={`ent-badge ${c.is_holding ? 'ent-badge-gold' : 'ent-badge-red'}`}>
                          {c.is_holding ? 'Holding Master' : 'Subsidiária'}
                        </span>
                        <h3>{c.name}</h3>
                        <span className="ent-sub-code">ID: {c.id} &bull; Código: {c.code}</span>
                      </div>
                    </div>

                    <div className="ent-comp-body">
                      <h4>Departamentos & Lideranças:</h4>
                      <ul className="ent-dept-list">
                        {departments.filter(d => d.company_id === c.id).map(d => (
                          <li key={d.id}>
                            <span><i className="fa-solid fa-angle-right" /> {d.name}</span>
                            <span className="ent-leader-badge">Líder: @{d.leader_user}</span>
                          </li>
                        ))}
                      </ul>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* TAB 3: EMPLOYEES */}
          {activeTab === 'employees' && (
            <div className="ent-tab-content">
              <div className="ent-section-head">
                <div>
                  <h2>Quadro de Funcionários & Hierarquia</h2>
                  <p>Gestão de identidades, cargos, dados cadastrais e credenciais no cofre ARAUTH.</p>
                </div>
                {canCreateEmployees && (
                  <button className="ent-btn-primary" onClick={() => setIsEmployeeModalOpen(true)}>
                    <i className="fa-solid fa-user-plus" /> Cadastrar Colaborador
                  </button>
                )}
              </div>

              <div className="ent-card">
                <table className="ent-table">
                  <thead>
                    <tr>
                      <th>Colaborador</th>
                      <th>Login / E-mail</th>
                      <th>Telefone</th>
                      <th>Empresa / Depto</th>
                      <th>Cargo & Nível</th>
                      <th>Status</th>
                      <th>Ações</th>
                    </tr>
                  </thead>
                  <tbody>
                    {filteredEmployees.map(emp => (
                      <tr key={emp.id}>
                        <td>
                          <div className="ent-emp-cell">
                            <div className="ent-avatar">
                              {emp.full_name ? emp.full_name.charAt(0).toUpperCase() : 'U'}
                            </div>
                            <div>
                              <strong>{emp.full_name}</strong>
                              <span className="ent-sub-info">ID: {emp.id}</span>
                            </div>
                          </div>
                        </td>
                        <td>
                          <div>@{emp.username}</div>
                          <span className="ent-sub-info">{emp.email || '—'}</span>
                        </td>
                        <td>{emp.phone || '—'}</td>
                        <td>
                          <div><strong>{companies.find(c => c.id === emp.company_id)?.name || emp.company_id}</strong></div>
                          <span className="ent-sub-info">{departments.find(d => d.id === emp.department_id)?.name || emp.department_id}</span>
                        </td>
                        <td>
                          <span className={`ent-badge ${emp.hierarchy_level === 1 ? 'ent-badge-gold' : emp.hierarchy_level <= 3 ? 'ent-badge-red' : 'ent-badge-gray'}`}>
                            Nível {emp.hierarchy_level} &bull; {emp.position_title}
                          </span>
                        </td>
                        <td>
                          <span className={`ent-status-pill ${emp.is_active ? 'active' : 'inactive'}`}>
                            {emp.is_active ? 'Ativo' : 'Inativo'}
                          </span>
                        </td>
                        <td>
                          <div className="ent-action-buttons">
                            {/* Editar: Apenas superiores em hierarquia ou o próprio usuário */}
                            {((me?.hierarchy_level || 10) < emp.hierarchy_level || me?.user === emp.username || me?.username === emp.username || isMasterAdmin) && (
                              <button
                                className="ent-btn-action"
                                title="Editar Dados Cadastrais"
                                onClick={() => openEditModal(emp)}
                              >
                                <i className="fa-solid fa-pen" />
                              </button>
                            )}
                            {/* Alterar Senha: Apenas gestores com hierarquia estritamente superior ao alvo */}
                            {canCreateEmployees && ((me?.hierarchy_level || 10) < emp.hierarchy_level || isMasterAdmin) && (
                              <button
                                className="ent-btn-action gold"
                                title="Alterar Senha (Requer Senha Admin)"
                                onClick={() => openPasswdModal(emp)}
                              >
                                <i className="fa-solid fa-key" />
                              </button>
                            )}
                            {/* Excluir: Apenas gestores com hierarquia estritamente superior ao alvo (nunca o CEO master) */}
                            {canCreateEmployees && ((me?.hierarchy_level || 10) < emp.hierarchy_level || isMasterAdmin) && (emp.username !== 'alexsanderalri') && (
                              <button
                                className="ent-btn-action red"
                                title="Excluir Colaborador"
                                onClick={() => handleDeleteEmployee(emp)}
                              >
                                <i className="fa-solid fa-trash" />
                              </button>
                            )}
                          </div>
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>
          )}

          {/* TAB 4: ROLES & RBAC */}
          {activeTab === 'roles' && (
            <div className="ent-tab-content">
              <div className="ent-section-head">
                <div>
                  <h2>Cargos & Matriz de Permissões (RBAC)</h2>
                  <p>Definição soberana de hierarquia executiva, autonomia e restrições por função.</p>
                </div>
                <button className="ent-btn-primary" onClick={() => setIsRoleModalOpen(true)}>
                  <i className="fa-solid fa-shield-plus" /> Criar Novo Cargo
                </button>
              </div>

              <div className="ent-grid-cards">
                {filteredRoles.map(r => (
                  <div key={r.id} className="ent-card ent-role-card">
                    <div className="ent-role-head">
                      <div>
                        <span className={`ent-badge ${r.hierarchy_level === 1 ? 'ent-badge-gold' : r.hierarchy_level <= 3 ? 'ent-badge-red' : 'ent-badge-gray'}`}>
                          Nível {r.hierarchy_level}
                        </span>
                        <h3>{r.name}</h3>
                        <span className="ent-sub-code">Empresa: {companies.find(c => c.id === r.company_id)?.name || r.company_id}</span>
                      </div>
                      {r.hierarchy_level > (me?.hierarchy_level || 10) && (
                        <button className="ent-btn-icon-danger" onClick={() => handleDeleteRole(r.id)}>
                          <i className="fa-solid fa-trash" />
                        </button>
                      )}
                    </div>

                    <div className="ent-role-body">
                      <h4>Permissões Concedidas:</h4>
                      <div className="ent-perm-tags">
                        {r.permissions ? r.permissions.split(',').map((p, idx) => (
                          <span key={idx} className="ent-perm-tag">
                            <i className="fa-solid fa-check" /> {p.trim()}
                          </span>
                        )) : <span className="ent-sub-info">Nenhuma permissão específica</span>}
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* TAB 5: TASKS (KANBAN) */}
          {activeTab === 'tasks' && (
            <div className="ent-tab-content">
              <div className="ent-section-head">
                <div>
                  <h2>Demandas & Fluxo de Trabalho (Kanban)</h2>
                  <p>Distribuição de tarefas, prazos e responsabilidades entre divisões e líderes.</p>
                </div>
                <button className="ent-btn-primary" onClick={() => setIsTaskModalOpen(true)}>
                  <i className="fa-solid fa-plus" /> Nova Demanda
                </button>
              </div>

              <div className="ent-kanban-board">
                {/* Backlog */}
                <div className="ent-kanban-col">
                  <div className="ent-kanban-col-head">
                    <span className="ent-col-pill backlog" />
                    <h3>Backlog</h3>
                    <span className="ent-col-count">{filteredTasks.filter(t => t.column_status === 'backlog').length}</span>
                  </div>
                  <div className="ent-kanban-cards">
                    {filteredTasks.filter(t => t.column_status === 'backlog').map(t => (
                      <div key={t.id} className="ent-card ent-task-card">
                        <div className="ent-task-badges">
                          <span className={`ent-badge ent-badge-prio ${t.priority}`}>{t.priority.toUpperCase()}</span>
                          <span className="ent-badge ent-badge-gray">{companies.find(c => c.id === t.company_id)?.code || t.company_id}</span>
                        </div>
                        <h4>{t.title}</h4>
                        <p>{t.description}</p>
                        <div className="ent-task-foot">
                          <span><i className="fa-solid fa-user-astronaut" /> @{t.assigned_to}</span>
                          <div className="ent-task-moves">
                            <button onClick={() => handleMoveTask(t.id, 'in_progress')} title="Iniciar Demanda">
                              <i className="fa-solid fa-arrow-right" />
                            </button>
                          </div>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>

                {/* In Progress */}
                <div className="ent-kanban-col">
                  <div className="ent-kanban-col-head">
                    <span className="ent-col-pill in-progress" />
                    <h3>Em Execução</h3>
                    <span className="ent-col-count">{filteredTasks.filter(t => t.column_status === 'in_progress').length}</span>
                  </div>
                  <div className="ent-kanban-cards">
                    {filteredTasks.filter(t => t.column_status === 'in_progress').map(t => (
                      <div key={t.id} className="ent-card ent-task-card">
                        <div className="ent-task-badges">
                          <span className={`ent-badge ent-badge-prio ${t.priority}`}>{t.priority.toUpperCase()}</span>
                          <span className="ent-badge ent-badge-gray">{companies.find(c => c.id === t.company_id)?.code || t.company_id}</span>
                        </div>
                        <h4>{t.title}</h4>
                        <p>{t.description}</p>
                        <div className="ent-task-foot">
                          <span><i className="fa-solid fa-user-astronaut" /> @{t.assigned_to}</span>
                          <div className="ent-task-moves">
                            <button onClick={() => handleMoveTask(t.id, 'backlog')} title="Voltar para Backlog">
                              <i className="fa-solid fa-arrow-left" />
                            </button>
                            <button onClick={() => handleMoveTask(t.id, 'review')} title="Enviar para Revisão">
                              <i className="fa-solid fa-arrow-right" />
                            </button>
                          </div>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>

                {/* Review */}
                <div className="ent-kanban-col">
                  <div className="ent-kanban-col-head">
                    <span className="ent-col-pill review" />
                    <h3>Revisão & Auditoria</h3>
                    <span className="ent-col-count">{filteredTasks.filter(t => t.column_status === 'review').length}</span>
                  </div>
                  <div className="ent-kanban-cards">
                    {filteredTasks.filter(t => t.column_status === 'review').map(t => (
                      <div key={t.id} className="ent-card ent-task-card">
                        <div className="ent-task-badges">
                          <span className={`ent-badge ent-badge-prio ${t.priority}`}>{t.priority.toUpperCase()}</span>
                          <span className="ent-badge ent-badge-gray">{companies.find(c => c.id === t.company_id)?.code || t.company_id}</span>
                        </div>
                        <h4>{t.title}</h4>
                        <p>{t.description}</p>
                        <div className="ent-task-foot">
                          <span><i className="fa-solid fa-user-astronaut" /> @{t.assigned_to}</span>
                          <div className="ent-task-moves">
                            <button onClick={() => handleMoveTask(t.id, 'in_progress')} title="Retornar Execução">
                              <i className="fa-solid fa-arrow-left" />
                            </button>
                            <button onClick={() => handleMoveTask(t.id, 'done')} title="Aprovar e Concluir">
                              <i className="fa-solid fa-check" />
                            </button>
                          </div>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>

                {/* Done */}
                <div className="ent-kanban-col">
                  <div className="ent-kanban-col-head">
                    <span className="ent-col-pill done" />
                    <h3>Concluído</h3>
                    <span className="ent-col-count">{filteredTasks.filter(t => t.column_status === 'done').length}</span>
                  </div>
                  <div className="ent-kanban-cards">
                    {filteredTasks.filter(t => t.column_status === 'done').map(t => (
                      <div key={t.id} className="ent-card ent-task-card ent-task-done">
                        <div className="ent-task-badges">
                          <span className="ent-badge ent-badge-green"><i className="fa-solid fa-check-double" /> OK</span>
                          <span className="ent-badge ent-badge-gray">{companies.find(c => c.id === t.company_id)?.code || t.company_id}</span>
                        </div>
                        <h4>{t.title}</h4>
                        <p>{t.description}</p>
                        <div className="ent-task-foot">
                          <span><i className="fa-solid fa-user-check" /> @{t.assigned_to}</span>
                          <div className="ent-task-moves">
                            <button onClick={() => handleMoveTask(t.id, 'review')} title="Reabrir Demanda">
                              <i className="fa-solid fa-rotate-left" />
                            </button>
                          </div>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>
              </div>
            </div>
          )}

          {/* TAB 6: CHAT */}
          {activeTab === 'chat' && (
            <div className="ent-tab-content ent-chat-layout">
              <div className="ent-chat-sidebar">
                <div className="ent-chat-sidebar-head">
                  <h3><i className="fa-solid fa-comments" /> Canais</h3>
                  <button className="ent-btn-icon" onClick={() => setIsChannelModalOpen(true)} title="Criar Novo Canal">
                    <i className="fa-solid fa-plus" />
                  </button>
                </div>
                <div className="ent-channel-list">
                  {channels.map(ch => (
                    <button
                      key={ch.id}
                      className={`ent-channel-btn ${activeChannel === ch.id ? 'active' : ''}`}
                      onClick={() => setActiveChannel(ch.id)}
                    >
                      <i className={ch.is_private ? 'fa-solid fa-lock' : 'fa-solid fa-hashtag'} />
                      <span>{ch.name}</span>
                    </button>
                  ))}
                </div>
              </div>

              <div className="ent-chat-main">
                <div className="ent-chat-header">
                  <div>
                    <h3>#{channels.find(c => c.id === activeChannel)?.name || activeChannel}</h3>
                    <span className="ent-chat-desc">
                      {channels.find(c => c.id === activeChannel)?.desc || 'Canal corporativo seguro'}
                    </span>
                  </div>
                  <span className="ent-badge ent-badge-gold"><i className="fa-solid fa-lock" /> E2EE Ativo</span>
                </div>

                <div className="ent-chat-messages">
                  {messages.length === 0 ? (
                    <div className="ent-chat-empty">
                      <i className="fa-regular fa-comment-dots" />
                      <p>Nenhuma mensagem ainda neste canal. Inicie a conversa!</p>
                    </div>
                  ) : (
                    messages.map(msg => (
                      <div key={msg.id} className={`ent-chat-msg ${msg.sender_user === me?.user ? 'mine' : ''}`}>
                        <div className="ent-msg-avatar">
                          {msg.sender_user.charAt(0).toUpperCase()}
                        </div>
                        <div className="ent-msg-bubble">
                          <div className="ent-msg-head">
                            <strong>@{msg.sender_user}</strong>
                            <span className="ent-msg-time">
                              {new Date(msg.created_at).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
                            </span>
                          </div>
                          <div className="ent-msg-text">{msg.ciphertext}</div>
                        </div>
                      </div>
                    ))
                  )}
                </div>

                <form className="ent-chat-input-row" onSubmit={handleSendMessage}>
                  <input
                    type="text"
                    placeholder={`Conversar em #${channels.find(c => c.id === activeChannel)?.name || activeChannel}...`}
                    value={chatInput}
                    onChange={(e) => setChatInput(e.target.value)}
                  />
                  <button type="submit" className="ent-btn-send">
                    <i className="fa-solid fa-paper-plane" />
                  </button>
                </form>
              </div>
            </div>
          )}
        </main>
      </div>

      {/* MODAL: NOVO COLABORADOR */}
      {isEmployeeModalOpen && (
        <div className="ent-modal-overlay">
          <div className="ent-modal">
            <div className="ent-modal-head">
              <h3><i className="fa-solid fa-user-plus" /> Cadastrar Novo Colaborador</h3>
              <button className="ent-modal-close" onClick={() => setIsEmployeeModalOpen(false)}>
                <i className="fa-solid fa-xmark" />
              </button>
            </div>
            <form onSubmit={handleCreateEmployee} className="ent-modal-form">
              <div className="ent-form-grid">
                <div className="ent-form-group">
                  <label>Nome Completo:</label>
                  <input
                    type="text" required placeholder="Ex: João da Silva"
                    value={empForm.full_name}
                    onChange={(e) => setEmpForm({ ...empForm, full_name: e.target.value })}
                  />
                </div>
                <div className="ent-form-group">
                  <label>Usuário de Login (ARAUTH):</label>
                  <input
                    type="text" required placeholder="Ex: joao.silva"
                    value={empForm.username}
                    onChange={(e) => setEmpForm({ ...empForm, username: e.target.value })}
                  />
                </div>
              </div>

              <div className="ent-form-grid">
                <div className="ent-form-group">
                  <label>Senha Inicial:</label>
                  <input
                    type="password" required placeholder="Senha forte inicial"
                    value={empForm.password}
                    onChange={(e) => setEmpForm({ ...empForm, password: e.target.value })}
                  />
                </div>
                <div className="ent-form-group">
                  <label>Telefone / WhatsApp:</label>
                  <input
                    type="text" placeholder="+55 11 99999-9999"
                    value={empForm.phone}
                    onChange={(e) => setEmpForm({ ...empForm, phone: e.target.value })}
                  />
                </div>
              </div>

              <div className="ent-form-group">
                <label>E-mail Corporativo:</label>
                <input
                  type="email" placeholder="joao@alrigroup.com"
                  value={empForm.email}
                  onChange={(e) => setEmpForm({ ...empForm, email: e.target.value })}
                />
              </div>

              <div className="ent-form-grid">
                <div className="ent-form-group">
                  <label>Empresa / Subsidiária:</label>
                  <select
                    value={empForm.company_id}
                    onChange={(e) => setEmpForm({ ...empForm, company_id: e.target.value })}
                  >
                    {companies.map(c => (
                      <option key={c.id} value={c.id}>{c.name}</option>
                    ))}
                  </select>
                </div>
                <div className="ent-form-group">
                  <label>Departamento:</label>
                  <select
                    value={empForm.department_id}
                    onChange={(e) => setEmpForm({ ...empForm, department_id: e.target.value })}
                  >
                    {departments.filter(d => d.company_id === empForm.company_id).map(d => (
                      <option key={d.id} value={d.id}>{d.name}</option>
                    ))}
                  </select>
                </div>
              </div>

              <div className="ent-form-grid">
                <div className="ent-form-group">
                  <label>Cargo / Título:</label>
                  <input
                    type="text" required placeholder="Ex: Engenheiro de Software"
                    value={empForm.position_title}
                    onChange={(e) => setEmpForm({ ...empForm, position_title: e.target.value })}
                  />
                </div>
                <div className="ent-form-group">
                  <label>Nível Hierárquico (RBAC):</label>
                  <select
                    value={empForm.hierarchy_level}
                    onChange={(e) => setEmpForm({ ...empForm, hierarchy_level: parseInt(e.target.value, 10) })}
                  >
                    {isMasterAdmin && <option value={1}>Nível 1 - Master / CEO</option>}
                    {me?.hierarchy_level <= 2 && <option value={2}>Nível 2 - Diretoria de Divisão</option>}
                    {me?.hierarchy_level <= 3 && <option value={3}>Nível 3 - Gerência / Coordenação</option>}
                    <option value={10}>Nível 10 - Colaborador / Especialista</option>
                  </select>
                </div>
              </div>

              <div className="ent-modal-foot">
                <button type="button" className="ent-btn-ghost" onClick={() => setIsEmployeeModalOpen(false)}>
                  Cancelar
                </button>
                <button type="submit" className="ent-btn-primary">
                  <i className="fa-solid fa-shield-check" /> Criar Colaborador
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* MODAL: EDITAR COLABORADOR */}
      {isEditEmployeeModalOpen && (
        <div className="ent-modal-overlay">
          <div className="ent-modal">
            <div className="ent-modal-head">
              <h3><i className="fa-solid fa-user-pen" /> Editar Dados do Colaborador</h3>
              <button className="ent-modal-close" onClick={() => setIsEditEmployeeModalOpen(false)}>
                <i className="fa-solid fa-xmark" />
              </button>
            </div>
            <form onSubmit={handleUpdateEmployee} className="ent-modal-form">
              <div className="ent-form-grid">
                <div className="ent-form-group">
                  <label>Nome Completo:</label>
                  <input
                    type="text" required
                    value={editEmpForm.full_name}
                    onChange={(e) => setEditEmpForm({ ...editEmpForm, full_name: e.target.value })}
                  />
                </div>
                <div className="ent-form-group">
                  <label>Login (Username):</label>
                  <input
                    type="text" required
                    value={editEmpForm.username}
                    onChange={(e) => setEditEmpForm({ ...editEmpForm, username: e.target.value })}
                  />
                </div>
              </div>

              <div className="ent-form-grid">
                <div className="ent-form-group">
                  <label>E-mail:</label>
                  <input
                    type="email"
                    value={editEmpForm.email}
                    onChange={(e) => setEditEmpForm({ ...editEmpForm, email: e.target.value })}
                  />
                </div>
                <div className="ent-form-group">
                  <label>Telefone / WhatsApp:</label>
                  <input
                    type="text"
                    value={editEmpForm.phone}
                    onChange={(e) => setEditEmpForm({ ...editEmpForm, phone: e.target.value })}
                  />
                </div>
              </div>

              <div className="ent-form-grid">
                <div className="ent-form-group">
                  <label>Empresa:</label>
                  <select
                    value={editEmpForm.company_id}
                    onChange={(e) => setEditEmpForm({ ...editEmpForm, company_id: e.target.value })}
                  >
                    {companies.map(c => (
                      <option key={c.id} value={c.id}>{c.name}</option>
                    ))}
                  </select>
                </div>
                <div className="ent-form-group">
                  <label>Departamento:</label>
                  <select
                    value={editEmpForm.department_id}
                    onChange={(e) => setEditEmpForm({ ...editEmpForm, department_id: e.target.value })}
                  >
                    {departments.filter(d => d.company_id === editEmpForm.company_id).map(d => (
                      <option key={d.id} value={d.id}>{d.name}</option>
                    ))}
                  </select>
                </div>
              </div>

              <div className="ent-form-grid">
                <div className="ent-form-group">
                  <label>Cargo / Título:</label>
                  <input
                    type="text" required
                    value={editEmpForm.position_title}
                    onChange={(e) => setEditEmpForm({ ...editEmpForm, position_title: e.target.value })}
                  />
                </div>
                <div className="ent-form-group">
                  <label>Nível Hierárquico:</label>
                  <select
                    value={editEmpForm.hierarchy_level}
                    onChange={(e) => setEditEmpForm({ ...editEmpForm, hierarchy_level: parseInt(e.target.value, 10) })}
                  >
                    {isMasterAdmin && <option value={1}>Nível 1 - Master / CEO</option>}
                    {me?.hierarchy_level <= 2 && <option value={2}>Nível 2 - Diretoria de Divisão</option>}
                    {me?.hierarchy_level <= 3 && <option value={3}>Nível 3 - Gerência / Coordenação</option>}
                    <option value={10}>Nível 10 - Colaborador / Especialista</option>
                  </select>
                </div>
              </div>

              <div className="ent-modal-foot">
                <button type="button" className="ent-btn-ghost" onClick={() => setIsEditEmployeeModalOpen(false)}>
                  Cancelar
                </button>
                <button type="submit" className="ent-btn-primary">
                  <i className="fa-solid fa-floppy-disk" /> Salvar Alterações
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* MODAL: ALTERAR SENHA COM RE-AUTH ADMIN */}
      {isPasswdModalOpen && (
        <div className="ent-modal-overlay">
          <div className="ent-modal">
            <div className="ent-modal-head">
              <h3><i className="fa-solid fa-key" /> Alterar Senha do Colaborador</h3>
              <button className="ent-modal-close" onClick={() => setIsPasswdModalOpen(false)}>
                <i className="fa-solid fa-xmark" />
              </button>
            </div>
            <form onSubmit={handleResetPassword} className="ent-modal-form">
              <div className="ent-alert-box">
                <i className="fa-solid fa-shield-halved gold" />
                <span>Por segurança soberana, a alteração de senhas de subordinados exige a <strong>sua senha de administrador</strong> para autorizar a operação.</span>
              </div>

              <div className="ent-form-group">
                <label>Colaborador:</label>
                <input type="text" disabled value={`${passwdForm.full_name} (@${passwdForm.username})`} />
              </div>

              <div className="ent-form-group">
                <label>Nova Senha do Colaborador:</label>
                <input
                  type="password" required placeholder="Digite a nova senha forte"
                  value={passwdForm.new_password}
                  onChange={(e) => setPasswdForm({ ...passwdForm, new_password: e.target.value })}
                />
              </div>

              <div className="ent-form-group">
                <label>Sua Senha de Administrador (Confirmação de Segurança):</label>
                <input
                  type="password" required placeholder="Digite a sua senha atual para autorizar"
                  value={passwdForm.admin_password}
                  onChange={(e) => setPasswdForm({ ...passwdForm, admin_password: e.target.value })}
                />
              </div>

              <div className="ent-modal-foot">
                <button type="button" className="ent-btn-ghost" onClick={() => setIsPasswdModalOpen(false)}>
                  Cancelar
                </button>
                <button type="submit" className="ent-btn-primary gold">
                  <i className="fa-solid fa-lock" /> Confirmar & Revogar Sessões
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* MODAL: CRIAR CARGO (RBAC) */}
      {isRoleModalOpen && (
        <div className="ent-modal-overlay">
          <div className="ent-modal">
            <div className="ent-modal-head">
              <h3><i className="fa-solid fa-shield-plus" /> Criar Novo Cargo & RBAC</h3>
              <button className="ent-modal-close" onClick={() => setIsRoleModalOpen(false)}>
                <i className="fa-solid fa-xmark" />
              </button>
            </div>
            <form onSubmit={handleCreateRole} className="ent-modal-form">
              <div className="ent-form-grid">
                <div className="ent-form-group">
                  <label>Nome do Cargo:</label>
                  <input
                    type="text" required placeholder="Ex: Coordenador de Infraestrutura"
                    value={roleForm.name}
                    onChange={(e) => setRoleForm({ ...roleForm, name: e.target.value })}
                  />
                </div>
                <div className="ent-form-group">
                  <label>Empresa:</label>
                  <select
                    value={roleForm.company_id}
                    onChange={(e) => setRoleForm({ ...roleForm, company_id: e.target.value })}
                  >
                    {companies.map(c => (
                      <option key={c.id} value={c.id}>{c.name}</option>
                    ))}
                  </select>
                </div>
              </div>

              <div className="ent-form-group">
                <label>Nível Hierárquico do Cargo:</label>
                <select
                  value={roleForm.hierarchy_level}
                  onChange={(e) => setRoleForm({ ...roleForm, hierarchy_level: parseInt(e.target.value, 10) })}
                >
                  <option value={2}>Nível 2 - Diretoria Executiva</option>
                  <option value={3}>Nível 3 - Gerência / Coordenação</option>
                  <option value={10}>Nível 10 - Colaborador / Especialista</option>
                </select>
              </div>

              <div className="ent-form-group">
                <label>Matriz de Permissões:</label>
                <div className="ent-perm-grid">
                  {[
                    { id: 'users.view', label: 'Visualizar Quadro de Funcionários' },
                    { id: 'users.create', label: 'Cadastrar Novos Colaboradores' },
                    { id: 'users.edit', label: 'Editar Dados de Subordinados' },
                    { id: 'users.passwd', label: 'Redefinir Senhas de Subordinados' },
                    { id: 'roles.manage', label: 'Criar e Gerenciar Cargos (RBAC)' },
                    { id: 'tasks.all', label: 'Gerenciamento Total de Demandas' },
                    { id: 'chat.channels', label: 'Criar e Gerenciar Canais de Chat' },
                    { id: 'tenant.cross_view', label: 'Visão Cross-Tenant (Holding Master)' }
                  ].map(p => (
                    <label key={p.id} className="ent-checkbox-label">
                      <input
                        type="checkbox"
                        checked={roleForm.permissions.includes(p.id)}
                        onChange={() => togglePermission(p.id)}
                      />
                      <span>{p.label}</span>
                    </label>
                  ))}
                </div>
              </div>

              <div className="ent-modal-foot">
                <button type="button" className="ent-btn-ghost" onClick={() => setIsRoleModalOpen(false)}>
                  Cancelar
                </button>
                <button type="submit" className="ent-btn-primary">
                  <i className="fa-solid fa-check" /> Criar Cargo
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* MODAL: CRIAR CANAL DE CHAT */}
      {isChannelModalOpen && (
        <div className="ent-modal-overlay">
          <div className="ent-modal">
            <div className="ent-modal-head">
              <h3><i className="fa-solid fa-plus" /> Criar Novo Canal Corporativo</h3>
              <button className="ent-modal-close" onClick={() => setIsChannelModalOpen(false)}>
                <i className="fa-solid fa-xmark" />
              </button>
            </div>
            <form onSubmit={handleCreateChannel} className="ent-modal-form">
              <div className="ent-form-group">
                <label>Nome do Canal (sem espaços):</label>
                <input
                  type="text" required placeholder="ex: demanda-anticheat, projetos-2026"
                  value={channelForm.name}
                  onChange={(e) => setChannelForm({ ...channelForm, name: e.target.value.toLowerCase().replace(/\s+/g, '-') })}
                />
              </div>

              <div className="ent-form-group">
                <label>Descrição do Canal / Demanda:</label>
                <input
                  type="text" placeholder="Ex: Acompanhamento da implantação do módulo E2EE"
                  value={channelForm.desc}
                  onChange={(e) => setChannelForm({ ...channelForm, desc: e.target.value })}
                />
              </div>

              <div className="ent-form-group">
                <label>Empresa Vinculada:</label>
                <select
                  value={channelForm.company_id}
                  onChange={(e) => setChannelForm({ ...channelForm, company_id: e.target.value })}
                >
                  {companies.map(c => (
                    <option key={c.id} value={c.id}>{c.name}</option>
                  ))}
                </select>
              </div>

              <div className="ent-modal-foot">
                <button type="button" className="ent-btn-ghost" onClick={() => setIsChannelModalOpen(false)}>
                  Cancelar
                </button>
                <button type="submit" className="ent-btn-primary">
                  <i className="fa-solid fa-check" /> Criar Canal
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* MODAL: NOVA DEMANDA KANBAN */}
      {isTaskModalOpen && (
        <div className="ent-modal-overlay">
          <div className="ent-modal">
            <div className="ent-modal-head">
              <h3><i className="fa-solid fa-list-check" /> Criar Nova Demanda</h3>
              <button className="ent-modal-close" onClick={() => setIsTaskModalOpen(false)}>
                <i className="fa-solid fa-xmark" />
              </button>
            </div>
            <form onSubmit={handleCreateTask} className="ent-modal-form">
              <div className="ent-form-group">
                <label>Título da Demanda:</label>
                <input
                  type="text" required placeholder="Ex: Auditoria de Segurança no Cluster"
                  value={taskForm.title}
                  onChange={(e) => setTaskForm({ ...taskForm, title: e.target.value })}
                />
              </div>

              <div className="ent-form-group">
                <label>Descrição Detalhada:</label>
                <textarea
                  rows="3" placeholder="Detalhes dos requisitos e critérios de aceitação"
                  value={taskForm.description}
                  onChange={(e) => setTaskForm({ ...taskForm, description: e.target.value })}
                />
              </div>

              <div className="ent-form-grid">
                <div className="ent-form-group">
                  <label>Empresa:</label>
                  <select
                    value={taskForm.company_id}
                    onChange={(e) => setTaskForm({ ...taskForm, company_id: e.target.value })}
                  >
                    {companies.map(c => (
                      <option key={c.id} value={c.id}>{c.name}</option>
                    ))}
                  </select>
                </div>
                <div className="ent-form-group">
                  <label>Prioridade:</label>
                  <select
                    value={taskForm.priority}
                    onChange={(e) => setTaskForm({ ...taskForm, priority: e.target.value })}
                  >
                    <option value="low">Baixa</option>
                    <option value="medium">Média</option>
                    <option value="high">Alta</option>
                    <option value="critical">Crítica / Emergencial</option>
                  </select>
                </div>
              </div>

              <div className="ent-form-group">
                <label>Responsável Designado (@username):</label>
                <select
                  value={taskForm.assigned_to}
                  onChange={(e) => setTaskForm({ ...taskForm, assigned_to: e.target.value })}
                >
                  <option value="">Selecione um colaborador</option>
                  {employees.map(emp => (
                    <option key={emp.id} value={emp.username}>
                      {emp.full_name} (@{emp.username}) - {emp.position_title}
                    </option>
                  ))}
                </select>
              </div>

              <div className="ent-modal-foot">
                <button type="button" className="ent-btn-ghost" onClick={() => setIsTaskModalOpen(false)}>
                  Cancelar
                </button>
                <button type="submit" className="ent-btn-primary">
                  <i className="fa-solid fa-plus" /> Criar Demanda
                </button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  )
}
