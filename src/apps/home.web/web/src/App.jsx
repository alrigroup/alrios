/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

import { useEffect, useState } from 'react'
import translations from './i18n.js'

export default function App() {
  const [lang, setLang] = useState(() => localStorage.getItem('alri_lang') || 'en')

  useEffect(() => {
    const t = translations[lang]
    document.querySelectorAll('[data-i18n]').forEach((el) => {
      const key = el.getAttribute('data-i18n')
      if (t && t[key]) el.innerHTML = t[key]
    })
    const langText = document.getElementById('langText')
    if (langText) langText.textContent = t.lang_btn
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
    fadeElements.forEach((el) => observer.observe(el))
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

  return (
    <>
      <nav className="navbar">
        <div className="logo">ALRI<span>GROUP</span></div>
        <div className="nav-links">
          <a href="#about" data-i18n="nav_about">Holding</a>
          <a href="#portfolio" data-i18n="nav_portfolio">Marcas</a>
          <a href="#elite" data-i18n="nav_elite">Tecnologia</a>
        </div>
        <div className="lang-switcher">
          <button onClick={toggleLanguage} id="langBtn" className="mono">
            <i className="fas fa-globe"></i> <span id="langText">PT-BR</span>
          </button>
        </div>
      </nav>

      <main>
        <section id="hero">
          <div className="bg-grid"></div>
          <div className="glow-orb"></div>
          <div className="hero-content fade-in">
            <img src="https://cdn.alrigroup.com/ALRI-SF-W.png" width="220" alt="ALRI Group Logo"
              style={{ marginBottom: '2rem', filter: 'drop-shadow(0 0 10px rgba(255,255,255,0.1))' }} />
            <h1 data-i18n="hero_h1">O ecossistema que conecta<br /><span className="text-gradient">tecnologia, inovação e novos mercados</span></h1>
            <p data-i18n="hero_sub">Nosso DNA é tech. Nossos horizontes são ilimitados. <strong>ALRI Group</strong> &mdash; uma holding multidisciplinar que constrói o futuro em camadas.</p>
            <div className="hero-ctas">
              <a href="#portfolio" className="cta-button" data-i18n="hero_cta1">Conhe&ccedil;a Nossas Marcas</a>
              <a href="#about" className="cta-button cta-outline" data-i18n="hero_cta2">Explore o Ecossistema</a>
            </div>
          </div>
        </section>

        <section id="about" className="fade-in">
          <h2 data-i18n="about_h2">Quem Somos</h2>
          <div className="showcase">
            <div className="showcase-text about-text">
              <p data-i18n="about_p1">O <strong>ALRI Group</strong> nasceu em <strong>2020</strong> da mente de <strong>Alexsander</strong> &mdash; um engenheiro de sistemas com talento para enxergar além do código. O que começou como laboratório de pesquisa em modificações profundas de kernel e segurança ofensiva se transformou em algo maior. Muito maior.</p>
              <p data-i18n="about_p2">Hoje, o ALRI Group é uma <strong>Holding Company</strong> multidisciplinar. Mantemos nossa alma tecnológica viva atrav&eacute;s da <strong>ARD &mdash; ALRI Development</strong> (engenharia de sistemas, sistemas operacionais customizados como o <strong>AROS</strong> e scripts de alto desempenho como o <strong>ARFS</strong>), enquanto rompemos fronteiras com a <strong>RIPB CLOTHES</strong> &mdash; nossa marca global de vestu&aacute;rio premium. E isso é apenas o come&ccedil;o.</p>
              <p data-i18n="about_p3" className="about-highlight">De um sonho tech em 2020 a uma estrutura sólida de gestão de negócios. O grupo cresce, as marcas se multiplicam, o DNA permanece.</p>
            </div>
          </div>
        </section>

        <section id="portfolio" className="fade-in">
          <h2 data-i18n="portfolio_h2">Nosso Portfólio</h2>
          <p data-i18n="portfolio_sub" className="section-sub">Cada marca do grupo representa um pilar estrat&eacute;gico. Juntas, formam um ecossistema completo.</p>

          <div className="portfolio-block">
            <div className="block-header">
              <i className="fas fa-microchip block-icon"></i>
              <h3 data-i18n="portfolio_tech_title">Divisão Tech <span className="badge-pilar">O Pilar</span></h3>
            </div>
            <div className="brand-grid">
              <div className="brand-card stagger-item">
                <div className="brand-card-header">
                  <i className="fas fa-cubes"></i>
                  <h4>ARD</h4>
                </div>
                <p className="brand-sub" data-i18n="portfolio_ard_sub">ALRI Development &mdash; Engenharia de Sistemas</p>
                <p data-i18n="portfolio_ard_p">Engenharia reversa, modificações profundas de kernel e infraestrutura de alto desempenho. A ARD é o motor que move o grupo.</p>
              </div>
              <div className="brand-card stagger-item">
                <div className="brand-card-header">
                  <i className="fas fa-terminal"></i>
                  <h4>AROS</h4>
                </div>
                <p className="brand-sub" data-i18n="portfolio_aros_sub">ALRI Operating System</p>
                <p data-i18n="portfolio_aros_p">Sistemas operacionais customizados &mdash; Windows, Android e Linux modificados para m&aacute;xima performance, seguran&ccedil;a e controle absoluto.</p>
              </div>
              <div className="brand-card stagger-item">
                <div className="brand-card-header">
                  <i className="fas fa-code"></i>
                  <h4>ARFS</h4>
                </div>
                <p className="brand-sub" data-i18n="portfolio_arfs_sub">FiveM Scripts &amp; Protocolos</p>
                <p data-i18n="portfolio_arfs_p">Scripts de elite para FiveM &mdash; incluindo o ecossistema Apex RP e o sistema anticheat ALRI Protect. Performance, estabilidade e inova&ccedil;&atilde;o.</p>
              </div>
            </div>
          </div>

          <div className="portfolio-block">
            <div className="block-header">
              <i className="fas fa-vest block-icon"></i>
              <h3 data-i18n="portfolio_life_title">Divisão Lifestyle &amp; Retail <span className="badge-expansao">A Expansão</span></h3>
            </div>
            <div className="brand-grid">
              <div className="brand-card stagger-item brand-card-wide">
                <div className="brand-card-header">
                  <i className="fas fa-shirt"></i>
                  <h4>RIPB CLOTHES</h4>
                </div>
                <p className="brand-sub" data-i18n="portfolio_ripb_sub">Moda Premium com Visão Global</p>
                <p data-i18n="portfolio_ripb_p">Design contemporâneo, qualidade premium e logística internacional. RIPB CLOTHES nasceu da mesma cultura de excelência que define o ALRI Group &mdash; agora traduzida para o mundo da moda.</p>
                <a href="https://ripb.alrigroup.com" target="_blank" rel="noreferrer" className="brand-link">ripb.alrigroup.com <i className="fas fa-external-link-alt"></i></a>
              </div>
            </div>
          </div>

          <div className="future-vision fade-in">
            <div className="future-content">
              <i className="fas fa-rocket future-icon"></i>
              <h3 data-i18n="portfolio_future_title">Novos Horizontes</h3>
              <p data-i18n="portfolio_future_p">O ALRI Group est&aacute; em constante incubação. Novas marcas, novos setores, novos mercados. O que come&ccedil;a como linha de c&oacute;digo pode se tornar uma indústria inteira. <strong>Fique de olho.</strong></p>
            </div>
          </div>
        </section>

        <section id="elite" className="fade-in">
          <h2 data-i18n="elite_h2">Projetos de Elite &amp; Tecnologia</h2>
          <p data-i18n="elite_sub" className="section-sub">A engenharia que sustenta cada marca do grupo. Certifica&ccedil;&otilde;es, licen&ccedil;as e produtos que definem nosso padrão.</p>

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
              <p data-i18n="wmaros_p">O <strong>Windows Mod ALRI Operating System</strong> é nossa flagship de performance. Um ambiente Windows Professional reconstruído e otimizado para entregar o m&aacute;ximo de FPS e a menor latência possivel para power users e gamers de alto nível.</p>
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
              <p data-i18n="arbemf_p">A espinha dorsal de nossas opera&ccedil;&otilde;es web. Um micro-framework em C nativo, focado em E2EE (End-to-End Encryption) e hot-reloading granular para sistemas que não podem parar. Puro desempenho em nível de kernel.</p>
              <div className="project-footer">
                <span className="status-tag"><i className="fas fa-lock"></i> Alta Seguran&ccedil;a</span>
                <span className="license-tag">ARGLR License</span>
              </div>
            </div>
          </div>
        </section>

        <section id="licensing" className="fade-in">
          <h2 data-i18n="lic_h2">Sistema de Licenciamento ARGL</h2>
          <div className="showcase license-showcase">
            <p data-i18n="lic_p">Nossas licen&ccedil;as (ALRI Group Licenses) garantem o equilíbrio entre inova&ccedil;&atilde;o aberta e seguran&ccedil;a institucional. Cada produto do ecossistema opera sob uma destas licen&ccedil;as.</p>
            <div className="license-grid">
              <div className="license-item">
                <span className="lic-tag permissive">ARGLP</span>
                <h4>Permissive</h4>
                <p data-i18n="lic_arglp">Uso e modifica&ccedil;&atilde;o livres para fins não comerciais (Open Source).</p>
              </div>
              <div className="license-item">
                <span className="lic-tag freeuse">ARGLFU</span>
                <h4>Free Use</h4>
                <p data-i18n="lic_arglfu">Livre para uso e distribui&ccedil;&atilde;o, mas proibido de sofrer modifica&ccedil;&otilde;es.</p>
              </div>
              <div className="license-item">
                <span className="lic-tag reserved">ARGLR</span>
                <h4>Reserved</h4>
                <p data-i18n="lic_arglr">Uso restrito a parceiros e clientes. C&oacute;digo blindado com garantia.</p>
              </div>
            </div>
          </div>
        </section>

        <section id="founder" className="fade-in">
          <h2 data-i18n="founder_h2">Missão &amp; Fundador</h2>
          <div className="showcase">
            <div className="showcase-text">
              <p data-i18n="founder_p1">Nosso prop&oacute;sito é fornecer solu&ccedil;&otilde;es inovadoras nas &aacute;reas mais exigentes &mdash; da engenharia de sistemas ao mercado de moda global. Resolvemos problemas complexos atrav&eacute;s de nossas unidades de neg&oacute;cio:</p>
              <ul className="tech-list mono">
                <li>&gt; <i className="fas fa-shield-halved"></i> <span data-i18n="founder_li1">Engenharia de sistemas e seguran&ccedil;a &mdash; o alicerce.</span></li>
                <li>&gt; <i className="fas fa-cube"></i> <span data-i18n="founder_li2">Desenvolvimento de software e infraestrutura &mdash; a execu&ccedil;&atilde;o.</span></li>
                <li>&gt; <i className="fas fa-vest"></i> <span data-i18n="founder_li3">Inova&ccedil;&atilde;o em lifestyle e retail &mdash; a expansão.</span></li>
              </ul>
              <p data-i18n="founder_p2" style={{ marginTop: '2rem', borderTop: '1px solid var(--border)', paddingTop: '2rem' }}>
                <strong>Fundador:</strong> O ALRI Group foi fundado e é liderado por <strong>Alexsander (@alexsanderalri)</strong>. O nome "ALRI" é um acrônimo de seus sobrenomes &mdash; <strong>Al</strong>meida + <strong>Ri</strong>beiro. Com uma carreira consolidada em seguran&ccedil;a ofensiva, engenharia reversa e modifica&ccedil;&otilde;es profundas de sistema, sua visão segue como o pilar de cada projeto e de cada marca que o grupo abriga.
              </p>
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
        <p className="footer-quote" data-i18n="footer_quote">"Construindo o futuro, uma camada de cada vez."</p>
        <p className="footer-copy">&copy; 2020-2026 ALRI Group. All rights reserved. <span id="easter-egg" title="Restricted Area" onClick={() => { window.location.href = '/manager/login' }}><i className="fas fa-lock"></i></span></p>
      </footer>
    </>
  )
}
