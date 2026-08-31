import React, { useState, useEffect, useRef } from 'react'

export default function App() {
  const [session, setSession] = useState(null)
  const [loading, setLoading] = useState(true)

  const [tickets, setTickets] = useState([])
  const [activeTicket, setActiveTicket] = useState(null)
  const [messages, setMessages] = useState([])
  const [replyText, setReplyText] = useState('')
  const msgEndRef = useRef(null)

  const getApiUrl = (endpoint, port = 9675) => {
    const host = window.location.hostname
    const currentPort = window.location.port
    if (currentPort === '3014' || currentPort === '5173' || host === 'localhost' || host === '127.0.0.1') {
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
            fetchTickets()
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

  const fetchTickets = async () => {
    try {
      const res = await fetch(getApiUrl('/arapi/chat/tickets', 9675), { credentials: 'include' })
      if (res.ok) {
        const data = await res.json()
        setTickets(data)
        if (data.length > 0 && !activeTicket) {
          setActiveTicket(data[0])
          fetchMessages(data[0].id)
        }
      }
    } catch (e) {}
  }

  const fetchMessages = async (tktId) => {
    try {
      const res = await fetch(getApiUrl(`/arapi/chat/messages?ticket_id=${tktId}`, 9675), { credentials: 'include' })
      if (res.ok) setMessages(await res.json())
    } catch (e) {}
  }

  useEffect(() => {
    if (activeTicket) {
      fetchMessages(activeTicket.id)
      const interval = setInterval(() => fetchMessages(activeTicket.id), 3000)
      return () => clearInterval(interval)
    }
  }, [activeTicket])

  useEffect(() => {
    msgEndRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [messages])

  const handleSendReply = async (e) => {
    e.preventDefault()
    if (!replyText.trim() || !activeTicket) return
    const txt = replyText.trim()
    setReplyText('')

    try {
      await fetch(getApiUrl('/arapi/chat/tickets/reply', 9675), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'include',
        body: JSON.stringify({ ticket_id: activeTicket.id, content: txt })
      })
      fetchMessages(activeTicket.id)
    } catch (err) {
      console.error(err)
    }
  }

  const handleResolveTicket = async () => {
    if (!activeTicket) return
    try {
      await fetch(getApiUrl('/arapi/chat/tickets/close', 9675), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        credentials: 'include',
        body: JSON.stringify({ ticket_id: activeTicket.id })
      })
      fetchTickets()
      setActiveTicket({ ...activeTicket, status: 'resolved' })
    } catch (err) {
      console.error(err)
    }
  }

  if (loading) {
    return (
      <div style={{ display: 'flex', height: '100vh', alignItems: 'center', justifyContent: 'center' }}>
        <div style={{ color: 'var(--text-muted)' }}>Carregando ALRI-Chat...</div>
      </div>
    )
  }

  return (
    <div className="chat-layout">
      {/* Left Inbox */}
      <aside className="chat-inbox">
        <div className="inbox-header">
          <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
            <span style={{ fontSize: '1.4rem' }}>🎧</span>
            <div>
              <div style={{ fontWeight: 800, fontSize: '1.1rem' }}>ALRI-Chat</div>
              <div style={{ fontSize: '0.72rem', color: 'var(--accent-cyan)' }}>Tickets & Atendimento</div>
            </div>
          </div>
          <span style={{ fontSize: '0.8rem', color: 'var(--text-dark)' }}>{tickets.length} chamados</span>
        </div>

        <div className="ticket-list">
          {tickets.map((tkt) => (
            <div
              key={tkt.id}
              className={`ticket-item ${activeTicket?.id === tkt.id ? 'active' : ''}`}
              onClick={() => { setActiveTicket(tkt); fetchMessages(tkt.id); }}
            >
              <div className="ticket-top">
                <span style={{ fontWeight: 700, fontSize: '0.9rem' }}>{tkt.customer_name}</span>
                <span className={`status-badge ${tkt.status === 'resolved' ? 'status-resolved' : 'status-open'}`}>
                  {tkt.status === 'resolved' ? 'Resolvido' : 'Aberto'}
                </span>
              </div>
              <div style={{ fontSize: '0.84rem', fontWeight: 500, color: 'var(--text-main)', marginBottom: '4px' }}>
                {tkt.subject}
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.74rem', color: 'var(--text-dark)' }}>
                <span>Empresa: {tkt.company_id.toUpperCase()}</span>
                <span>{tkt.created_at}</span>
              </div>
            </div>
          ))}
        </div>
      </aside>

      {/* Right Conversation Thread */}
      <main className="chat-thread-container">
        {activeTicket ? (
          <>
            <header className="thread-header">
              <div>
                <h3 style={{ fontSize: '1.15rem', fontWeight: 800 }}>{activeTicket.subject}</h3>
                <span style={{ fontSize: '0.8rem', color: 'var(--text-muted)' }}>
                  Cliente: <strong>{activeTicket.customer_name}</strong> ({activeTicket.customer_email}) &bull; {activeTicket.company_id.toUpperCase()}
                </span>
              </div>

              <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
                {activeTicket.status !== 'resolved' && (
                  <button className="btn-resolve" onClick={handleResolveTicket}>
                    ✓ Resolver Chamado
                  </button>
                )}
                <span className={`status-badge ${activeTicket.status === 'resolved' ? 'status-resolved' : 'status-open'}`}>
                  {activeTicket.status === 'resolved' ? 'Resolvido' : 'Em Atendimento'}
                </span>
              </div>
            </header>

            <div className="thread-messages">
              {messages.map((m) => (
                <div key={m.id} className={`thread-msg-item ${m.sender_type === 'staff' ? 'msg-staff' : 'msg-customer'}`}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '0.75rem', color: 'var(--text-dark)', marginBottom: '4px' }}>
                    <strong>{m.sender_name} {m.sender_type === 'staff' ? '(Staff ALRIGROUP)' : '(Cliente)'}</strong>
                    <span>{m.timestamp}</span>
                  </div>
                  <div>{m.content}</div>
                </div>
              ))}
              <div ref={msgEndRef} />
            </div>

            <div className="thread-reply-box">
              <form className="reply-form" onSubmit={handleSendReply}>
                <input
                  type="text"
                  className="reply-input"
                  placeholder="Digitar resposta oficial de suporte ao cliente..."
                  value={replyText}
                  onChange={e => setReplyText(e.target.value)}
                />
                <button
                  type="submit"
                  style={{ padding: '0 24px', background: 'var(--primary)', border: 'none', borderRadius: 'var(--radius-sm)', color: '#fff', fontWeight: 600, cursor: 'pointer' }}
                >
                  Responder
                </button>
              </form>
            </div>
          </>
        ) : (
          <div style={{ display: 'flex', height: '100%', alignItems: 'center', justifyContent: 'center', color: 'var(--text-dark)' }}>
            Selecione um chamado na caixa de entrada para iniciar o atendimento.
          </div>
        )}
      </main>
    </div>
  )
}
