import React, { useState, useEffect } from 'react'

export default function App() {
  const [session, setSession] = useState(null)
  const [loading, setLoading] = useState(true)
  const [activeTab, setActiveTab] = useState('employees') // 'employees', 'departments', 'companies', 'hierarchy'

  const [companies, setCompanies] = useState([])
  const [departments, setDepartments] = useState([])
  const [employees, setEmployees] = useState([])
  const [selectedCompany, setSelectedCompany] = useState('all')

  const [modalType, setModalType] = useState(null) // 'employee', 'department', 'company'
  const [formErr, setFormErr] = useState('')
  const [formSuccess, setFormSuccess] = useState('')

  // Form states
  const [empUsername, setEmpUsername] = useState('')
  const [empName, setEmpName] = useState('')
  const [empEmail, setEmpEmail] = useState('')
  const [empRole, setEmpRole] = useState('')
  const [empLevel, setEmpLevel] = useState(4)
  const [empDept, setEmpDept] = useState('')
  const [empComp, setEmpComp] = useState('')

  const [deptName, setDeptName] = useState('')
  const [deptCode, setDeptCode] = useState('')
  const [deptLeader, setDeptLeader] = useState('')
  const [deptDesc, setDeptDesc] = useState('')
  const [deptComp, setDeptComp] = useState('')

  const [compName, setCompName] = useState('')
  const [compSlug, setCompSlug] = useState('')
  const [compDomain, setCompDomain] = useState('')
  const [compCnpj, setCompCnpj] = useState('')

  const getApiUrl = (endpoint, port = 9670) => {
    const host = window.location.hostname
    const currentPort = window.location.port
    if (currentPort === '3013' || currentPort === '5173' || host === 'localhost' || host === '127.0.0.1') {
      return `http://${host}:${port}${endpoint}`
    }
    return endpoint
  }

  // 1. Check SSO Session
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
            fetchData()
          }
        }
      } catch (err) {
        console.warn('Auth error:', err)
      } finally {
        setLoading(false)
      }
    }
    init()
  }, [])

  const fetchData = async () => {
    try {
      const [compRes, deptRes, empRes] = await Promise.all([
        fetch(getApiUrl('/arapi/bus/companies', 9670), { credentials: 'include' }),
        fetch(getApiUrl('/arapi/bus/departments', 9670), { credentials: 'include' }),
        fetch(getApiUrl('/arapi/bus/employees', 9670), { credentials: 'include' })
      ])
      if (compRes.ok) setCompanies(await compRes.json())
      if (deptRes.ok) setDepartments(await deptRes.json())
      if (empRes.ok) setEmployees(await empRes.json())
    } catch (e) {
      console.warn('Error fetching corporate data:', e)
    }
  }

  const handleCreateEmployee = async (e) => {
    e.preventDefault()
    setFormErr('')
    setFormSuccess('')
    try {
      const res = await fetch(getApiUrl('/arapi/bus/employees/create', 9670), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'include',
        body: JSON.stringify({
          company_id: empComp || session.tenant,
          department_id: empDept || 'DIR',
          username: empUsername.trim(),
          name: empName.trim(),
          email: empEmail.trim(),
          role: empRole.trim(),
          hierarchy_level: parseInt(empLevel)
        })
      })
      const data = await res.json()
      if (res.ok) {
        setFormSuccess('Colaborador cadastrado com sucesso!')
        setEmpUsername(''); setEmpName(''); setEmpEmail(''); setEmpRole('');
        fetchData()
        setTimeout(() => setModalType(null), 1000)
      } else {
        setFormErr(data.error || 'Erro ao cadastrar colaborador')
      }
    } catch (err) {
      setFormErr('Falha de conexão com a API ARBUS')
    }
  }

  const handleCreateDepartment = async (e) => {
    e.preventDefault()
    setFormErr('')
    setFormSuccess('')
    try {
      const res = await fetch(getApiUrl('/arapi/bus/departments/create', 9670), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'include',
        body: JSON.stringify({
          company_id: deptComp || session.tenant,
          name: deptName.trim(),
          code: deptCode.trim().toUpperCase(),
          leader: deptLeader.trim(),
          description: deptDesc.trim()
        })
      })
      const data = await res.json()
      if (res.ok) {
        setFormSuccess('Departamento criado com sucesso!')
        setDeptName(''); setDeptCode(''); setDeptLeader(''); setDeptDesc('');
        fetchData()
        setTimeout(() => setModalType(null), 1000)
      } else {
        setFormErr(data.error || 'Erro ao criar departamento')
      }
    } catch (err) {
      setFormErr('Falha de conexão com a API ARBUS')
    }
  }

  const handleCreateCompany = async (e) => {
    e.preventDefault()
    setFormErr('')
    setFormSuccess('')
    try {
      const res = await fetch(getApiUrl('/arapi/bus/companies/create', 9670), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'include',
        body: JSON.stringify({
          id: compSlug.trim().toLowerCase(),
          name: compName.trim(),
          slug: compSlug.trim().toLowerCase(),
          domain: compDomain.trim(),
          cnpj: compCnpj.trim()
        })
      })
      const data = await res.json()
      if (res.ok) {
        setFormSuccess('Empresa cadastrada com sucesso!')
        setCompName(''); setCompSlug(''); setCompDomain(''); setCompCnpj('');
        fetchData()
        setTimeout(() => setModalType(null), 1000)
      } else {
        setFormErr(data.error || 'Erro ao cadastrar empresa')
      }
    } catch (err) {
      setFormErr('Falha de conexão com a API ARBUS')
    }
  }

  const getLevelBadge = (level) => {
    switch (level) {
      case 1: return <span className="level-badge level-1">👑 Nível 1 &bull; Holding Master</span>
      case 2: return <span className="level-badge level-2">🏛️ Nível 2 &bull; Diretoria / CEO</span>
      case 3: return <span className="level-badge level-3">💼 Nível 3 &bull; Gerência / Coord</span>
      case 4: return <span className="level-badge level-4">💻 Nível 4 &bull; Operador / Staff</span>
      case 5: return <span className="level-badge level-5">🌱 Nível 5 &bull; Estagiário</span>
      default: return <span className="level-badge level-4">Nível {level}</span>
    }
  }

  if (loading) {
    return (
      <div style={{ display: 'flex', height: '100vh', alignItems: 'center', justifyContent: 'center' }}>
        <div style={{ color: 'var(--text-muted)' }}>Carregando ALRI-Business...</div>
      </div>
    )
  }

  return (
    <div className="bus-layout">
      {/* Header */}
      <header className="bus-header">
        <div className="bus-brand">
          <div className="bus-logo">🏢</div>
          <div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <span className="bus-brand-title">ALRI-Business</span>
              <span className="bus-badge">RH & Corporativo</span>
            </div>
          </div>
        </div>

        {/* Tab Navigation */}
        <nav className="bus-tabs">
          <button
            className={`bus-tab-btn ${activeTab === 'employees' ? 'active' : ''}`}
            onClick={() => setActiveTab('employees')}
          >
            👥 Colaboradores ({employees.length})
          </button>
          <button
            className={`bus-tab-btn ${activeTab === 'departments' ? 'active' : ''}`}
            onClick={() => setActiveTab('departments')}
          >
            📂 Departamentos ({departments.length})
          </button>
          <button
            className={`bus-tab-btn ${activeTab === 'companies' ? 'active' : ''}`}
            onClick={() => setActiveTab('companies')}
          >
            🏛️ Empresas ({companies.length})
          </button>
          <button
            className={`bus-tab-btn ${activeTab === 'hierarchy' ? 'active' : ''}`}
            onClick={() => setActiveTab('hierarchy')}
          >
            🛡️ Hierarquia & Níveis
          </button>
        </nav>

        {/* User Pill */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px', fontSize: '0.86rem' }}>
          <span style={{ fontWeight: 600 }}>{session?.user}</span>
          <span style={{ color: '#a5b4fc' }}>({session?.tenant})</span>
        </div>
      </header>

      {/* Main Container */}
      <main className="bus-container">
        {/* TAB 1: EMPLOYEES DIRECTORY */}
        {activeTab === 'employees' && (
          <div>
            <div className="bus-toolbar">
              <h2 className="bus-toolbar-title">Quadro Geral de Colaboradores</h2>
              <div className="bus-actions">
                <select
                  className="input-field"
                  style={{ width: '200px', marginBottom: 0 }}
                  value={selectedCompany}
                  onChange={(e) => setSelectedCompany(e.target.value)}
                >
                  <option value="all">Todas as Empresas</option>
                  {companies.map(c => <option key={c.id} value={c.id}>{c.name}</option>)}
                </select>

                <button className="btn-add" onClick={() => { setModalType('employee'); setFormErr(''); setFormSuccess(''); }}>
                  <span>+</span> <span>Admitir Colaborador</span>
                </button>
              </div>
            </div>

            <div className="data-table-card">
              <div className="table-row table-header-row">
                <span>Colaborador</span>
                <span>Empresa</span>
                <span>Departamento / Cargo</span>
                <span>Nível de Autoridade</span>
                <span style={{ textAlign: 'right' }}>Ações</span>
              </div>

              {employees
                .filter(e => selectedCompany === 'all' || e.company_id === selectedCompany)
                .map(emp => (
                  <div key={emp.id} className="table-row">
                    <div>
                      <div style={{ fontWeight: 700 }}>{emp.name}</div>
                      <div style={{ color: 'var(--text-muted)', fontSize: '0.78rem' }}>@{emp.username} &bull; {emp.email}</div>
                    </div>

                    <div style={{ color: '#c084fc', fontWeight: 600 }}>
                      {emp.company_id ? emp.company_id.toUpperCase() : 'ALRIGROUP'}
                    </div>

                    <div>
                      <div style={{ fontWeight: 500 }}>{emp.role}</div>
                      <div style={{ color: 'var(--text-dark)', fontSize: '0.78rem' }}>Dept: {emp.department_id}</div>
                    </div>

                    <div>
                      {getLevelBadge(emp.hierarchy_level)}
                    </div>

                    <div style={{ textAlign: 'right', color: 'var(--text-dark)' }}>
                      <span style={{ cursor: 'pointer' }}>⚙️</span>
                    </div>
                  </div>
                ))}
            </div>
          </div>
        )}

        {/* TAB 2: DEPARTMENTS */}
        {activeTab === 'departments' && (
          <div>
            <div className="bus-toolbar">
              <h2 className="bus-toolbar-title">Estrutura de Departamentos</h2>
              <button className="btn-add" onClick={() => { setModalType('department'); setFormErr(''); setFormSuccess(''); }}>
                <span>+</span> <span>Novo Departamento</span>
              </button>
            </div>

            <div className="cards-grid">
              {departments.map(dept => (
                <div key={dept.id} className="card-item">
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '12px' }}>
                    <span style={{ fontSize: '0.78rem', fontWeight: 700, padding: '2px 8px', borderRadius: '4px', background: 'rgba(99, 102, 241, 0.15)', color: '#a5b4fc' }}>
                      {dept.code}
                    </span>
                    <span style={{ color: '#c084fc', fontSize: '0.8rem', fontWeight: 600 }}>
                      {dept.company_id.toUpperCase()}
                    </span>
                  </div>

                  <h3 style={{ fontSize: '1.2rem', fontWeight: 700, marginBottom: '8px' }}>{dept.name}</h3>
                  <p style={{ color: 'var(--text-muted)', fontSize: '0.88rem', lineHeight: 1.5, marginBottom: '16px' }}>
                    {dept.description}
                  </p>

                  <div style={{ fontSize: '0.82rem', color: 'var(--text-dark)', borderTop: '1px solid rgba(255, 255, 255, 0.05)', paddingTop: '12px' }}>
                    Liderança: <strong style={{ color: 'var(--text-main)' }}>@{dept.leader || 'alexsanderalri'}</strong>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* TAB 3: COMPANIES */}
        {activeTab === 'companies' && (
          <div>
            <div className="bus-toolbar">
              <h2 className="bus-toolbar-title">Empresas e Subsidiárias do Grupo</h2>
              {session?.is_master && (
                <button className="btn-add" onClick={() => { setModalType('company'); setFormErr(''); setFormSuccess(''); }}>
                  <span>+</span> <span>Cadastrar Subsidiária</span>
                </button>
              )}
            </div>

            <div className="cards-grid">
              {companies.map(comp => (
                <div key={comp.id} className="card-item" style={{ borderLeft: comp.is_holding ? '4px solid var(--accent-purple)' : '4px solid var(--accent-cyan)' }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '12px' }}>
                    <span style={{ fontSize: '0.75rem', fontWeight: 700, color: comp.is_holding ? '#c084fc' : '#67e8f9' }}>
                      {comp.is_holding ? '👑 HOLDING PRINCIPAL' : '🏢 SUBSIDIÁRIA'}
                    </span>
                    <span style={{ fontSize: '0.78rem', color: 'var(--text-dark)' }}>{comp.cnpj}</span>
                  </div>

                  <h3 style={{ fontSize: '1.3rem', fontWeight: 700, marginBottom: '6px' }}>{comp.name}</h3>
                  <div style={{ color: 'var(--text-muted)', fontSize: '0.86rem', marginBottom: '16px' }}>
                    ID: <code>{comp.id}</code> &bull; Domínio: {comp.domain || `${comp.id}.alrigroup.com`}
                  </div>

                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', borderTop: '1px solid rgba(255, 255, 255, 0.05)', paddingTop: '14px', fontSize: '0.82rem' }}>
                    <span style={{ color: 'var(--accent-emerald)' }}>● Status Ativo</span>
                    <span style={{ color: 'var(--primary)', cursor: 'pointer' }}>Gerenciar &rarr;</span>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* TAB 4: HIERARCHY GUIDE */}
        {activeTab === 'hierarchy' && (
          <div style={{ maxWidth: '800px', margin: '0 auto' }}>
            <h2 className="bus-toolbar-title" style={{ textAlign: 'center', marginBottom: '24px' }}>
              Matriz Soberana de Autoridade (Zero-Trust RBAC)
            </h2>

            <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
              <div className="card-item" style={{ borderLeft: '4px solid #c084fc' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
                  <strong style={{ color: '#c084fc' }}>Nível 1 &bull; Holding Master Admin</strong>
                  <span className="level-badge level-1">Acesso Global Irrestrito</span>
                </div>
                <p style={{ color: 'var(--text-muted)', fontSize: '0.88rem' }}>
                  Super-administradores da holding (ex: Alex Sander Alri). Podem criar e excluir subsidiárias, gerenciar infraestrutura no ARCTRL e auditar todo o ecossistema.
                </p>
              </div>

              <div className="card-item" style={{ borderLeft: '4px solid #67e8f9' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
                  <strong style={{ color: '#67e8f9' }}>Nível 2 &bull; Diretoria / CEO da Subsidiária</strong>
                  <span className="level-badge level-2">Escopo da Empresa</span>
                </div>
                <p style={{ color: 'var(--text-muted)', fontSize: '0.88rem' }}>
                  Diretores executivos de cada empresa (ex: CEO Detroit GG). Gestão completa dentro da sua empresa, bloqueados de alterar a holding ou subsidiárias irmãs.
                </p>
              </div>

              <div className="card-item" style={{ borderLeft: '4px solid #6ee7b7' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
                  <strong style={{ color: '#6ee7b7' }}>Nível 3 &bull; Gerência e Coordenação</strong>
                  <span className="level-badge level-3">Gestão de Equipe</span>
                </div>
                <p style={{ color: 'var(--text-muted)', fontSize: '0.88rem' }}>
                  Gerentes departamentais. Podem criar tarefas, gerenciar colaboradores de nível 4 e 5 no seu departamento e criar canais de equipe.
                </p>
              </div>

              <div className="card-item" style={{ borderLeft: '4px solid #cbd5e1' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
                  <strong style={{ color: '#cbd5e1' }}>Nível 4 &bull; Operador / Staff / Especialista</strong>
                  <span className="level-badge level-4">Operação Padrão</span>
                </div>
                <p style={{ color: 'var(--text-muted)', fontSize: '0.88rem' }}>
                  Colaboradores ativos. Interagem em canais, movem suas tarefas designadas e atendem chamados de clientes.
                </p>
              </div>

              <div className="card-item" style={{ borderLeft: '4px solid #fde68a' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
                  <strong style={{ color: '#fde68a' }}>Nível 5 &bull; Estagiário / Trainee</strong>
                  <span className="level-badge level-5">Acesso Restrito</span>
                </div>
                <p style={{ color: 'var(--text-muted)', fontSize: '0.88rem' }}>
                  Membros em treinamento. Permissões estritamente limitadas para visualização e execução de tarefas pontuais.
                </p>
              </div>
            </div>
          </div>
        )}
      </main>

      {/* MODAL: ADMITIR COLABORADOR */}
      {modalType === 'employee' && (
        <div className="modal-overlay" onClick={() => setModalType(null)}>
          <div className="modal-card" onClick={e => e.stopPropagation()}>
            <h3 className="modal-title">Admitir Novo Colaborador</h3>
            {formErr && <div style={{ color: 'var(--accent-rose)', marginBottom: '12px', fontSize: '0.86rem' }}>⚠️ {formErr}</div>}
            {formSuccess && <div style={{ color: 'var(--accent-emerald)', marginBottom: '12px', fontSize: '0.86rem' }}>✅ {formSuccess}</div>}

            <form onSubmit={handleCreateEmployee}>
              <input className="input-field" placeholder="Username (ex: josesilva)" value={empUsername} onChange={e => setEmpUsername(e.target.value)} required />
              <input className="input-field" placeholder="Nome Completo" value={empName} onChange={e => setEmpName(e.target.value)} required />
              <input className="input-field" type="email" placeholder="E-mail corporativo" value={empEmail} onChange={e => setEmpEmail(e.target.value)} />
              <input className="input-field" placeholder="Cargo (ex: Desenvolvedor Senior)" value={empRole} onChange={e => setEmpRole(e.target.value)} required />

              <div style={{ display: 'flex', gap: '10px', marginBottom: '14px' }}>
                <select className="input-field" value={empComp} onChange={e => setEmpComp(e.target.value)} style={{ marginBottom: 0 }}>
                  <option value="">Selecione a Empresa</option>
                  {companies.map(c => <option key={c.id} value={c.id}>{c.name}</option>)}
                </select>

                <select className="input-field" value={empLevel} onChange={e => setEmpLevel(e.target.value)} style={{ marginBottom: 0 }}>
                  <option value={2}>Nível 2 (Diretoria)</option>
                  <option value={3}>Nível 3 (Gerência)</option>
                  <option value={4}>Nível 4 (Operador)</option>
                  <option value={5}>Nível 5 (Estagiário)</option>
                </select>
              </div>

              <div style={{ display: 'flex', gap: '10px', marginTop: '20px' }}>
                <button type="button" className="bus-tab-btn" onClick={() => setModalType(null)} style={{ flex: 1 }}>Cancelar</button>
                <button type="submit" className="btn-add" style={{ flex: 1, justifyContent: 'center' }}>Salvar</button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* MODAL: NOVO DEPARTAMENTO */}
      {modalType === 'department' && (
        <div className="modal-overlay" onClick={() => setModalType(null)}>
          <div className="modal-card" onClick={e => e.stopPropagation()}>
            <h3 className="modal-title">Cadastrar Novo Departamento</h3>
            {formErr && <div style={{ color: 'var(--accent-rose)', marginBottom: '12px', fontSize: '0.86rem' }}>⚠️ {formErr}</div>}
            {formSuccess && <div style={{ color: 'var(--accent-emerald)', marginBottom: '12px', fontSize: '0.86rem' }}>✅ {formSuccess}</div>}

            <form onSubmit={handleCreateDepartment}>
              <input className="input-field" placeholder="Nome do Departamento (ex: Financeiro)" value={deptName} onChange={e => setDeptName(e.target.value)} required />
              <input className="input-field" placeholder="Código (ex: FIN, DEV, RH)" value={deptCode} onChange={e => setDeptCode(e.target.value)} required />
              <input className="input-field" placeholder="Líder / Coordenador" value={deptLeader} onChange={e => setDeptLeader(e.target.value)} />
              <input className="input-field" placeholder="Descrição das Atividades" value={deptDesc} onChange={e => setDeptDesc(e.target.value)} />

              <select className="input-field" value={deptComp} onChange={e => setDeptComp(e.target.value)}>
                <option value="">Empresa de Alocação</option>
                {companies.map(c => <option key={c.id} value={c.id}>{c.name}</option>)}
              </select>

              <div style={{ display: 'flex', gap: '10px', marginTop: '20px' }}>
                <button type="button" className="bus-tab-btn" onClick={() => setModalType(null)} style={{ flex: 1 }}>Cancelar</button>
                <button type="submit" className="btn-add" style={{ flex: 1, justifyContent: 'center' }}>Criar</button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* MODAL: NOVA EMPRESA */}
      {modalType === 'company' && (
        <div className="modal-overlay" onClick={() => setModalType(null)}>
          <div className="modal-card" onClick={e => e.stopPropagation()}>
            <h3 className="modal-title">Cadastrar Nova Subsidiária</h3>
            {formErr && <div style={{ color: 'var(--accent-rose)', marginBottom: '12px', fontSize: '0.86rem' }}>⚠️ {formErr}</div>}
            {formSuccess && <div style={{ color: 'var(--accent-emerald)', marginBottom: '12px', fontSize: '0.86rem' }}>✅ {formSuccess}</div>}

            <form onSubmit={handleCreateCompany}>
              <input className="input-field" placeholder="Razão Social / Nome da Empresa" value={compName} onChange={e => setCompName(e.target.value)} required />
              <input className="input-field" placeholder="ID Único / Slug (ex: detroitgg, alripay)" value={compSlug} onChange={e => setCompSlug(e.target.value)} required />
              <input className="input-field" placeholder="Domínio Oficial (ex: detroitgg.alrigroup.com)" value={compDomain} onChange={e => setCompDomain(e.target.value)} />
              <input className="input-field" placeholder="CNPJ" value={compCnpj} onChange={e => setCompCnpj(e.target.value)} />

              <div style={{ display: 'flex', gap: '10px', marginTop: '20px' }}>
                <button type="button" className="bus-tab-btn" onClick={() => setModalType(null)} style={{ flex: 1 }}>Cancelar</button>
                <button type="submit" className="btn-add" style={{ flex: 1, justifyContent: 'center' }}>Cadastrar</button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  )
}
