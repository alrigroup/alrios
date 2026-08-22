/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

import { useEffect, useState, useRef } from 'react'
import translations from './i18n.js'

export default function App() {
  const [lang, setLang] = useState(() => localStorage.getItem('alri_lang') || 'en')
  const [authSession, setAuthSession] = useState({
    checked: false,
    authenticated: false,
    user: '',
    tenant: '',
    role: '',
    sessionToken: ''
  })
  const [dropdownOpen, setDropdownOpen] = useState(false)
  const [loginModalOpen, setLoginModalOpen] = useState(false)
  const [usernameInput, setUsernameInput] = useState('')
  const [passwordInput, setPasswordInput] = useState('')
  const [totpInput, setTotpInput] = useState('')
  const [authError, setAuthError] = useState('')
  const [authSuccess, setAuthSuccess] = useState('')
  const [isSubmitting, setIsSubmitting] = useState(false)
  const sessionMenuRef = useRef(null)
  const [showTotp, setShowTotp] = useState(false)
  const getAuthBaseUrl = () => {
    // If running directly on ARWN frontend port 3001 without reverse proxy, route to arapiauth port 9650
    if (typeof window !== 'undefined' && (window.location.port === '3001' || window.location.port === '5173')) {
      return `http://${window.location.hostname}:9650`
    }
    return ''
  }

  // Verify active session against ARAUTH backend (/arapi/auth/me)
  const verifySession = async () => {
    try {
      const storedToken = localStorage.getItem('ar_session_token') || ''
      const headers = { 'Accept': 'application/json' }
      if (storedToken) {
        headers['Authorization'] = `Bearer ${storedToken}`
      }
      const baseUrl = getAuthBaseUrl()
      const res = await fetch(`${baseUrl}/arapi/auth/me`, {
        method: 'GET',
        headers,
        credentials: 'include'
      })
      if (res.ok) {
        const data = await res.json()
        if (data && data.authenticated && data.user) {
          setAuthSession({
            checked: true,
            authenticated: true,
            user: data.user,
            tenant: data.tenant || 'holding_alri',
            role: data.role || 'user',
            sessionToken: storedToken
          })
          return
        }
      }
    } catch (err) {
      console.warn('[ARAUTH] Session verification offline or pending gateway:', err)
    }
    setAuthSession({
      checked: true,
      authenticated: false,
      user: '',
      tenant: '',
      role: '',
      sessionToken: ''
    })
  }

  useEffect(() => {
    verifySession()
  }, [])

  // Close dropdown on click outside
  useEffect(() => {
    const handleClickOutside = (e) => {
      if (sessionMenuRef.current && !sessionMenuRef.current.contains(e.target)) {
        setDropdownOpen(false)
      }
    }
    document.addEventListener('mousedown', handleClickOutside)
    return () => document.removeEventListener('mousedown', handleClickOutside)
  }, [])

  useEffect(() => {
    document.documentElement.lang = lang === 'en' ? 'en' : 'pt-BR'
  }, [lang])

  useEffect(() => {
    const fadeElements = document.querySelectorAll('.fade-in')
    const observer = new IntersectionObserver(
      (entries, obs) => {
        let delay = 0
        entries.forEach((entry) => {
          if (entry.isIntersecting) {
            if (entry.target.classList.contains('stagger-item')) {
              setTimeout(() => entry.target.classList.add('visible'), delay)
              delay += 150
            } else {
              entry.target.classList.add('visible')
            }
            obs.unobserve(entry.target)
          }
        })
      },
      { threshold: 0.1, rootMargin: '0px 0px -50px 0px' }
    )
    fadeElements.forEach((el) => {
      el.classList.add('visible')
      observer.observe(el)
    })
    return () => observer.disconnect()
  }, [])

  useEffect(() => {
    const cards = document.querySelectorAll('.brand-card')
    const onMove = (card) => (e) => {
      const rect = card.getBoundingClientRect()
      const x = e.clientX - rect.left
      const y = e.clientY - rect.top
      const cx = rect.width / 2
      const cy = rect.height / 2
      const rx = ((y - cy) / cy) * -5
      const ry = ((x - cx) / cx) * 5
      card.style.transform = `perspective(1000px) rotateX(${rx}deg) rotateY(${ry}deg) scale3d(1.02, 1.02, 1.02)`
    }
    const onLeave = (card) => () => {
      card.style.transform = 'perspective(1000px) rotateX(0deg) rotateY(0deg) scale3d(1, 1, 1)'
      card.style.transition = 'transform 0.5s ease'
    }
    const onEnter = (card) => () => {
      card.style.transition = 'none'
    }
    cards.forEach((card) => {
      card.addEventListener('mousemove', onMove(card))
      card.addEventListener('mouseleave', onLeave(card))
      card.addEventListener('mouseenter', onEnter(card))
    })
    return () => {
      cards.forEach((card) => {
        card.removeEventListener('mousemove', onMove(card))
        card.removeEventListener('mouseleave', onLeave(card))
        card.removeEventListener('mouseenter', onEnter(card))
      })
    }
  }, [])

  const toggleLanguage = () => {
    const next = lang === 'en' ? 'pt' : 'en'
    localStorage.setItem('alri_lang', next)
    setLang(next)
  }

  // Handle Login submission
  const handleLoginSubmit = async (e) => {
    e.preventDefault()
    setAuthError('')
    setAuthSuccess('')
    if (!usernameInput.trim() || !passwordInput.trim()) {
      setAuthError(lang === 'en' ? 'Please fill in both username and password.' : 'Por favor, preencha o usuário e a senha.')
      return
    }

    setIsSubmitting(true)
    try {
      const payload = {
        username: usernameInput.trim(),
        password: passwordInput,
        totp_code: totpInput.trim() || undefined
      }

      const baseUrl = getAuthBaseUrl()
      const res = await fetch(`${baseUrl}/arapi/auth/login`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
        credentials: 'include'
      })

      const data = await res.json()
      if (res.ok && data.status === 'success') {
        if (data.session_token) {
          localStorage.setItem('ar_session_token', data.session_token)
        }
        if (data.refresh_token) {
          localStorage.setItem('ar_refresh_token', data.refresh_token)
        }
        setAuthSession({
          checked: true,
          authenticated: true,
          user: data.user || usernameInput.trim(),
          tenant: data.tenant || 'holding_alri',
          role: data.role || 'user',
          sessionToken: data.session_token || ''
        })
        setAuthSuccess(t.modal_redirecting || 'Authenticating sovereignly...')
        setTimeout(() => {
          setLoginModalOpen(false)
          setUsernameInput('')
          setPasswordInput('')
          setTotpInput('')
          setAuthSuccess('')
          window.location.href = '/restrict-area'
        }, 800)
      } else {
        if (data.status === 'need_2fa') {
          setShowTotp(true);
          setAuthError(lang === 'en' ? 'Two‑factor authentication required.' : 'Autenticação de dois fatores necessária.');
        } else {
          setAuthError(data.error || (lang === 'en' ? 'Invalid credentials or access rejected.' : 'Credenciais inválidas ou acesso recusado.'));
        }
      }
    } catch (err) {
      setAuthError(lang === 'en' ? 'Connection error with ARAUTH Gateway.' : 'Erro de conexão com o ARAUTH Gateway.')
    } finally {
      setIsSubmitting(false)
    }
  }

  // Handle Logout
  const handleLogout = async () => {
    try {
      const storedToken = localStorage.getItem('ar_session_token') || ''
      const baseUrl = getAuthBaseUrl()
      await fetch(`${baseUrl}/arapi/auth/logout`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${storedToken}`
        },
        credentials: 'include'
      })
    } catch (err) {
      console.warn('[ARAUTH] Logout call completed with local fallback')
    }
    localStorage.removeItem('ar_session_token')
    localStorage.removeItem('ar_refresh_token')
    setAuthSession({
      checked: true,
      authenticated: false,
      user: '',
      tenant: '',
      role: '',
      sessionToken: ''
    })
    setDropdownOpen(false)
  }

  const t = translations[lang] || translations.pt || translations.en

  return (
    <>
      <nav className="navbar">
        <div className="logo">ALRI<span>GROUP</span></div>
        <div className="nav-links">
          <a href="#about" data-i18n="nav_about">{t.nav_about}</a>
          <a href="#portfolio" data-i18n="nav_portfolio">{t.nav_portfolio}</a>
          <a href="#elite" data-i18n="nav_elite">{t.nav_elite}</a>
        </div>
        <div className="nav-actions">
          {/* User Session Menu & Avatar */}
          <div className="user-session-wrapper" ref={sessionMenuRef}>
            <button
              className={`user-avatar-btn ${authSession.authenticated ? 'is-authenticated' : ''}`}
              onClick={() => setDropdownOpen(!dropdownOpen)}
              title={authSession.authenticated ? `Sessão: ${authSession.user}` : 'Identidade & Acesso'}
              aria-label="User Session"
            >
              {authSession.authenticated ? (
                <img
                  src={`https://ui-avatars.com/api/?name=${encodeURIComponent(authSession.user)}&background=0088ff&color=ffffff&bold=true`}
                  alt={authSession.user}
                  className="avatar-img"
                />
              ) : (
                <i className="fas fa-user-shield avatar-icon-anon"></i>
              )}
              <span className={`session-status-dot ${authSession.authenticated ? 'active' : 'idle'}`}></span>
            </button>

            {/* Dropdown Popover */}
            {dropdownOpen && (
              <div className="user-dropdown">
                <div className="user-dropdown-header">
                  <div className="dropdown-avatar-large">
                    {authSession.authenticated ? (
                      <img
                        src={`https://ui-avatars.com/api/?name=${encodeURIComponent(authSession.user)}&background=0088ff&color=ffffff&bold=true`}
                        alt={authSession.user}
                        className="avatar-img"
                      />
                    ) : (
                      <i className="fas fa-user-secret"></i>
                    )}
                  </div>
                  <div className="dropdown-user-info">
                    <h5>{authSession.authenticated ? authSession.user : t.auth_anonymous}</h5>
                    <span>
                      {authSession.authenticated
                        ? (authSession.role === 'admin'
                            ? t.auth_role_admin
                            : authSession.role === 'operator'
                              ? t.auth_role_operator
                              : t.auth_role_user)
                        : (t.auth_anonymous_sub || 'Faça login para continuar')}
                    </span>
                    {authSession.authenticated && (
                      <span className="dropdown-tenant">Tenant: {authSession.tenant}</span>
                    )}
                  </div>
                </div>

                <div className="dropdown-shield-badge">
                  <i className="fas fa-shield-halved"></i>
                  <span>{t.auth_status_online}</span>
                </div>

                {authSession.authenticated ? (
                  <>
                    <button
                      className="dropdown-btn dropdown-btn-private"
                      onClick={() => {
                        setDropdownOpen(false)
                        window.location.href = '/restrict-area'
                      }}
                    >
                      <i className="fas fa-key"></i>
                      <span>{t.auth_private_area}</span>
                    </button>
                    <button className="dropdown-btn dropdown-btn-logout" onClick={handleLogout}>
                      <i className="fas fa-arrow-right-from-bracket"></i>
                      <span>{t.auth_logout}</span>
                    </button>
                  </>
                ) : (
                  <button
                    className="dropdown-btn dropdown-btn-primary"
                    onClick={() => {
                      setDropdownOpen(false)
                      setShowTotp(false)
                      setLoginModalOpen(true)
                    }}
                  >
                    <i className="fas fa-fingerprint"></i>
                    <span>{t.auth_login}</span>
                  </button>
                )}
              </div>
            )}
          </div>

          {/* Language Switcher */}
          <div className="lang-switcher">
            <button onClick={toggleLanguage} id="langBtn" className="mono">
              <i className="fas fa-globe"></i> <span id="langText">{t.lang_btn}</span>
            </button>
          </div>
        </div>
      </nav>

      {/* Sovereign Login Modal */}
      {loginModalOpen && (
        <div className="auth-modal-overlay" onClick={() => setLoginModalOpen(false)}>
          <div className="auth-modal" onClick={(e) => e.stopPropagation()}>
            <button
              className="auth-modal-close"
              onClick={() => setLoginModalOpen(false)}
              aria-label="Close modal"
            >
              <i className="fas fa-times"></i>
            </button>

            <div className="auth-modal-header">
              <div className="auth-modal-icon">
                <i className="fas fa-shield-halved"></i>
              </div>
              <h3>{t.modal_title || 'Login na Área Restrita'}</h3>
            </div>

            {authError && (
              <div className="auth-error-banner">
                <i className="fas fa-triangle-exclamation"></i>
                <span>{authError}</span>
              </div>
            )}

            {authSuccess && (
              <div className="auth-success-banner">
                <i className="fas fa-circle-check"></i>
                <span>{authSuccess}</span>
              </div>
            )}

            <form onSubmit={handleLoginSubmit}>
              <div className="auth-form-group">
                <label>{t.modal_user_label || 'Usuário'}</label>
                <div className="auth-input-wrapper">
                  <input
                    type="text"
                    className="auth-input mono"
                    placeholder={t.modal_user_placeholder || 'alex / alri_admin'}
                    value={usernameInput}
                    onChange={(e) => setUsernameInput(e.target.value)}
                    required
                    autoFocus
                    autoComplete="username"
                  />
                  <i className="fas fa-user"></i>
                </div>
              </div>

              <div className="auth-form-group">
                <label>{t.modal_pass_label || 'Senha'}</label>
                <div className="auth-input-wrapper">
                  <input
                    type="password"
                    className="auth-input mono"
                    placeholder={t.modal_pass_placeholder || '••••••••••••'}
                    value={passwordInput}
                    onChange={(e) => setPasswordInput(e.target.value)}
                    required
                    autoComplete="current-password"
                  />
                  <i className="fas fa-lock"></i>
                </div>
              </div>

              {showTotp && (
                <div className="auth-form-group">
                  <label>{t.modal_totp_label || 'Código 2FA'}</label>
                  <div className="auth-input-wrapper">
                    <input
                      type="text"
                      className="auth-input mono"
                      placeholder="6-digit code (e.g. 123456)"
                      maxLength={6}
                      value={totpInput}
                      onChange={(e) => setTotpInput(e.target.value)}
                      autoComplete="one-time-code"
                    />
                    <i className="fas fa-shield-keyhole"></i>
                  </div>
                </div>
              )}

              <button
                type="submit"
                className="auth-submit-btn"
                disabled={isSubmitting}
              >
                {isSubmitting ? (
                  <>
                    <i className="fas fa-spinner fa-spin"></i>
                    <span>{t.modal_btn_authenticating || 'Autenticando...'}</span>
                  </>
                ) : (
                  <>
                    <i className="fas fa-fingerprint"></i>
                    <span>{t.modal_btn_submit || 'Autenticar'}</span>
                  </>
                )}
              </button>
            </form>

            <div className="auth-modal-footer">
              <i className="fas fa-lock"></i> {t.modal_footer || 'Login protegido por ARAUTH'}
            </div>
          </div>
        </div>
      )}

      <main>
        <section id="hero">
          <div className="bg-grid"></div>
          <div className="glow-orb"></div>
          <div className="hero-content fade-in">
            <img src="https://cdn.alrigroup.com/ALRI-SF-W.png" width="220" alt="ALRI Group Logo"
              style={{ marginBottom: '2rem', filter: 'drop-shadow(0 0 10px rgba(255,255,255,0.1))' }} />
            <h1 data-i18n="hero_h1" dangerouslySetInnerHTML={{ __html: t.hero_h1 }} />
            <p data-i18n="hero_sub" dangerouslySetInnerHTML={{ __html: t.hero_sub }} />
            <div className="hero-ctas">
              <a href="#portfolio" className="cta-button" data-i18n="hero_cta1">{t.hero_cta1}</a>
              <a href="#about" className="cta-button cta-outline" data-i18n="hero_cta2">{t.hero_cta2}</a>
            </div>
          </div>
        </section>

        <section id="about" className="fade-in">
          <h2 data-i18n="about_h2">{t.about_h2}</h2>
          <div className="showcase">
            <div className="showcase-text about-text">
              <p data-i18n="about_p1" dangerouslySetInnerHTML={{ __html: t.about_p1 }} />
              <p data-i18n="about_p2" dangerouslySetInnerHTML={{ __html: t.about_p2 }} />
              <p data-i18n="about_p3" className="about-highlight" dangerouslySetInnerHTML={{ __html: t.about_p3 }} />
            </div>
          </div>
        </section>

        <section id="portfolio" className="fade-in">
          <h2 data-i18n="portfolio_h2">{t.portfolio_h2}</h2>
          <p data-i18n="portfolio_sub" className="section-sub">{t.portfolio_sub}</p>

          <div className="portfolio-block">
            <div className="block-header">
              <i className="fas fa-microchip block-icon"></i>
              <h3 data-i18n="portfolio_tech_title" dangerouslySetInnerHTML={{ __html: t.portfolio_tech_title }} />
            </div>
            <div className="brand-grid">
              <div className="brand-card stagger-item">
                <div className="brand-card-header">
                  <i className="fas fa-cubes"></i>
                  <h4>ARD</h4>
                </div>
                <p className="brand-sub" data-i18n="portfolio_ard_sub" dangerouslySetInnerHTML={{ __html: t.portfolio_ard_sub }} />
                <p data-i18n="portfolio_ard_p">{t.portfolio_ard_p}</p>
              </div>
              <div className="brand-card stagger-item">
                <div className="brand-card-header">
                  <i className="fas fa-terminal"></i>
                  <h4>AROS</h4>
                </div>
                <p className="brand-sub" data-i18n="portfolio_aros_sub" dangerouslySetInnerHTML={{ __html: t.portfolio_aros_sub }} />
                <p data-i18n="portfolio_aros_p">{t.portfolio_aros_p}</p>
              </div>
              <div className="brand-card stagger-item">
                <div className="brand-card-header">
                  <i className="fas fa-code"></i>
                  <h4>ARFS</h4>
                </div>
                <p className="brand-sub" data-i18n="portfolio_arfs_sub" dangerouslySetInnerHTML={{ __html: t.portfolio_arfs_sub }} />
                <p data-i18n="portfolio_arfs_p">{t.portfolio_arfs_p}</p>
              </div>
            </div>
          </div>

          <div className="portfolio-block">
            <div className="block-header">
              <i className="fas fa-vest block-icon"></i>
              <h3 data-i18n="portfolio_life_title" dangerouslySetInnerHTML={{ __html: t.portfolio_life_title }} />
            </div>
            <div className="brand-grid">
              <div className="brand-card stagger-item brand-card-wide">
                <div className="brand-card-header">
                  <i className="fas fa-shirt"></i>
                  <h4>RIPB CLOTHES</h4>
                </div>
                <p className="brand-sub" data-i18n="portfolio_ripb_sub">{t.portfolio_ripb_sub}</p>
                <p data-i18n="portfolio_ripb_p">{t.portfolio_ripb_p}</p>
                <a href="https://ripb.alrigroup.com" target="_blank" rel="noreferrer" className="brand-link">ripb.alrigroup.com <i className="fas fa-external-link-alt"></i></a>
              </div>
            </div>
          </div>

          <div className="future-vision fade-in">
            <div className="future-content">
              <i className="fas fa-rocket future-icon"></i>
              <h3 data-i18n="portfolio_future_title">{t.portfolio_future_title}</h3>
              <p data-i18n="portfolio_future_p" dangerouslySetInnerHTML={{ __html: t.portfolio_future_p }} />
            </div>
          </div>
        </section>

        <section id="elite" className="fade-in">
          <h2 data-i18n="elite_h2" dangerouslySetInnerHTML={{ __html: t.elite_h2 }} />
          <p data-i18n="elite_sub" className="section-sub">{t.elite_sub}</p>

          <div className="project-showcase">
            <div className="project-card">
              <div className="project-header">
                <div className="project-icon-wrapper">
                  <i className="fas fa-bolt"></i>
                </div>
                <div>
                  <h3 className="neon-text">WMAROS</h3>
                  <span className="badge">Engineered by AROS</span>
                </div>
              </div>
              <p data-i18n="wmaros_p" dangerouslySetInnerHTML={{ __html: t.wmaros_p }} />
              <div className="project-footer">
                <span className="status-tag"><i className="fas fa-bolt"></i> Ultra Performance</span>
                <span className="license-tag">ARGLFU License</span>
              </div>
            </div>

            <div className="project-card">
              <div className="project-header">
                <div className="project-icon-wrapper">
                  <i className="fas fa-microchip"></i>
                </div>
                <div>
                  <h3 className="neon-text">AR-BEMF</h3>
                  <span className="badge">Engineered by ARD</span>
                </div>
              </div>
              <p data-i18n="arbemf_p" dangerouslySetInnerHTML={{ __html: t.arbemf_p }} />
              <div className="project-footer">
                <span className="status-tag"><i className="fas fa-lock"></i> Alta Segurança</span>
                <span className="license-tag">ARGLR License</span>
              </div>
            </div>
          </div>
        </section>

        <section id="licensing" className="fade-in">
          <h2 data-i18n="lic_h2">{t.lic_h2}</h2>
          <div className="showcase license-showcase">
            <p data-i18n="lic_p">{t.lic_p}</p>
            <div className="license-grid">
              <div className="license-item">
                <span className="lic-tag permissive">ARGLP</span>
                <h4>Permissive</h4>
                <p data-i18n="lic_arglp">{t.lic_arglp}</p>
              </div>
              <div className="license-item">
                <span className="lic-tag freeuse">ARGLFU</span>
                <h4>Free Use</h4>
                <p data-i18n="lic_arglfu">{t.lic_arglfu}</p>
              </div>
              <div className="license-item">
                <span className="lic-tag reserved">ARGLR</span>
                <h4>Reserved</h4>
                <p data-i18n="lic_arglr">{t.lic_arglr}</p>
              </div>
            </div>
          </div>
        </section>

        <section id="founder" className="fade-in">
          <h2 data-i18n="founder_h2" dangerouslySetInnerHTML={{ __html: t.founder_h2 }} />
          <div className="showcase">
            <div className="showcase-text">
              <p data-i18n="founder_p1" dangerouslySetInnerHTML={{ __html: t.founder_p1 }} />
              <ul className="tech-list mono">
                <li>&gt; <i className="fas fa-shield-halved"></i> <span data-i18n="founder_li1">{t.founder_li1}</span></li>
                <li>&gt; <i className="fas fa-cube"></i> <span data-i18n="founder_li2">{t.founder_li2}</span></li>
                <li>&gt; <i className="fas fa-vest"></i> <span data-i18n="founder_li3">{t.founder_li3}</span></li>
              </ul>
              <p data-i18n="founder_p2" style={{ marginTop: '2rem', borderTop: '1px solid var(--border)', paddingTop: '2rem' }} dangerouslySetInnerHTML={{ __html: t.founder_p2 }} />
            </div>
          </div>
        </section>
      </main>

      <footer>
        <div className="footer-socials">
          <a href="https://www.instagram.com/alrigroup" target="_blank" rel="noreferrer" title="Instagram">
            <i className="fab fa-instagram"></i>
          </a>
          <a href="https://dsc.gg/alrigroup" target="_blank" rel="noreferrer" title="Discord">
            <i className="fab fa-discord"></i>
          </a>
          <a href="https://github.com/alrigroup" target="_blank" rel="noreferrer" title="GitHub">
            <i className="fab fa-github"></i>
          </a>
        </div>
        <p className="footer-quote" data-i18n="footer_quote">{t.footer_quote}</p>
        <p className="footer-copy">&copy; 2020-2026 ALRI Group. All rights reserved. <span id="easter-egg" title="Restricted Area" onClick={() => { window.location.href = '/restrict-area' }}><i className="fas fa-lock"></i></span></p>
      </footer>
    </>
  )
}
