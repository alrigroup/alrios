/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef HOME_WEB_DATA_H
#define HOME_WEB_DATA_H

static const char *home_html = R"homeweb(
<!DOCTYPE html>
<html lang="pt-BR">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ALRI Group | Holding Company &mdash; Technology &amp; Lifestyle</title>
    <link rel="icon" type="image/png" sizes="32x32" href="https://cdn.alrigroup.com/favicon-32x32.png">
    <link rel="icon" type="image/png" sizes="16x16" href="https://cdn.alrigroup.com/favicon-16x16.png">
    <link rel="icon" type="image/svg+xml" href="https://cdn.alrigroup.com/favicon.svg">
    <link rel="apple-touch-icon" sizes="180x180" href="https://cdn.alrigroup.com/apple-touch-icon.png">
    <link rel="manifest" href="https://cdn.alrigroup.com/site.webmanifest">
    <link rel="stylesheet" href="/home/style.css">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.2/css/all.min.css"
        integrity="sha512-SnH5WK+bZxgPHs44uWIX+LLJAJ9/2PkPKZ5QiAj6Ta86w+fsb2TkcmfRyVX3pBnMFcV7oQPJkl9QevSCWr3W6A=="
        crossorigin="anonymous">
</head>

<body>
    <nav class="navbar">
        <div class="logo">ALRI<span>GROUP</span></div>
        <div class="nav-links">
            <a href="#about" data-i18n="nav_about">Holding</a>
            <a href="#portfolio" data-i18n="nav_portfolio">Marcas</a>
            <a href="#elite" data-i18n="nav_elite">Tecnologia</a>
        </div>
        <div class="lang-switcher">
            <button onclick="toggleLanguage()" id="langBtn" class="mono">
                <i class="fas fa-globe"></i> <span id="langText">PT-BR</span>
            </button>
        </div>
    </nav>

    <main>
        <section id="hero">
            <div class="bg-grid"></div>
            <div class="glow-orb"></div>
            <div class="hero-content fade-in">
                <img src="https://cdn.alrigroup.com/ALRI-SF-W.png" width="220" alt="ALRI Group Logo"
                    style="margin-bottom: 2rem; filter: drop-shadow(0 0 10px rgba(255,255,255,0.1));">
                <h1 data-i18n="hero_h1">O ecossistema que conecta<br><span class="text-gradient">tecnologia, inovação e novos mercados</span></h1>
                <p data-i18n="hero_sub">Nosso DNA é tech. Nossos horizontes são ilimitados. <strong>ALRI Group</strong> &mdash; uma holding multidisciplinar que constrói o futuro em camadas.</p>
                <div class="hero-ctas">
                    <a href="#portfolio" class="cta-button" data-i18n="hero_cta1">Conhe&ccedil;a Nossas Marcas</a>
                    <a href="#about" class="cta-button cta-outline" data-i18n="hero_cta2">Explore o Ecossistema</a>
                </div>
            </div>
        </section>

        <section id="about" class="fade-in">
            <h2 data-i18n="about_h2">Quem Somos</h2>
            <div class="showcase">
                <div class="showcase-text about-text">
                    <p data-i18n="about_p1">O <strong>ALRI Group</strong> nasceu em <strong>2020</strong> da mente de <strong>Alexsander</strong> &mdash; um engenheiro de sistemas com talento para enxergar além do código. O que começou como laboratório de pesquisa em modificações profundas de kernel e segurança ofensiva se transformou em algo maior. Muito maior.</p>
                    <p data-i18n="about_p2">Hoje, o ALRI Group é uma <strong>Holding Company</strong> multidisciplinar. Mantemos nossa alma tecnológica viva atrav&eacute;s da <strong>ARD &mdash; ALRI Development</strong> (engenharia de sistemas, sistemas operacionais customizados como o <strong>AROS</strong> e scripts de alto desempenho como o <strong>ARFS</strong>), enquanto rompemos fronteiras com a <strong>RIPB CLOTHES</strong> &mdash; nossa marca global de vestu&aacute;rio premium. E isso é apenas o come&ccedil;o.</p>
                    <p data-i18n="about_p3" class="about-highlight">De um sonho tech em 2020 a uma estrutura sólida de gestão de negócios. O grupo cresce, as marcas se multiplicam, o DNA permanece.</p>
                </div>
            </div>
        </section>

        <section id="portfolio" class="fade-in">
            <h2 data-i18n="portfolio_h2">Nosso Portfólio</h2>
            <p data-i18n="portfolio_sub" class="section-sub">Cada marca do grupo representa um pilar estrat&eacute;gico. Juntas, formam um ecossistema completo.</p>

            <div class="portfolio-block">
                <div class="block-header">
                    <i class="fas fa-microchip block-icon"></i>
                    <h3 data-i18n="portfolio_tech_title">Divisão Tech <span class="badge-pilar">O Pilar</span></h3>
                </div>
                <div class="brand-grid">
                    <div class="brand-card stagger-item">
                        <div class="brand-card-header">
                            <i class="fas fa-cubes"></i>
                            <h4>ARD</h4>
                        </div>
                        <p class="brand-sub" data-i18n="portfolio_ard_sub">ALRI Development &mdash; Engenharia de Sistemas</p>
                        <p data-i18n="portfolio_ard_p">Engenharia reversa, modificações profundas de kernel e infraestrutura de alto desempenho. A ARD é o motor que move o grupo.</p>
                    </div>
                    <div class="brand-card stagger-item">
                        <div class="brand-card-header">
                            <i class="fas fa-terminal"></i>
                            <h4>AROS</h4>
                        </div>
                        <p class="brand-sub" data-i18n="portfolio_aros_sub">ALRI Operating System</p>
                        <p data-i18n="portfolio_aros_p">Sistemas operacionais customizados &mdash; Windows, Android e Linux modificados para m&aacute;xima performance, seguran&ccedil;a e controle absoluto.</p>
                    </div>
                    <div class="brand-card stagger-item">
                        <div class="brand-card-header">
                            <i class="fas fa-code"></i>
                            <h4>ARFS</h4>
                        </div>
                        <p class="brand-sub" data-i18n="portfolio_arfs_sub">FiveM Scripts &amp; Protocolos</p>
                        <p data-i18n="portfolio_arfs_p">Scripts de elite para FiveM &mdash; incluindo o ecossistema Apex RP e o sistema anticheat ALRI Protect. Performance, estabilidade e inova&ccedil;&atilde;o.</p>
                    </div>
                </div>
            </div>

            <div class="portfolio-block">
                <div class="block-header">
                    <i class="fas fa-vest block-icon"></i>
                    <h3 data-i18n="portfolio_life_title">Divisão Lifestyle &amp; Retail <span class="badge-expansao">A Expansão</span></h3>
                </div>
                <div class="brand-grid">
                    <div class="brand-card stagger-item brand-card-wide">
                        <div class="brand-card-header">
                            <i class="fas fa-shirt"></i>
                            <h4>RIPB CLOTHES</h4>
                        </div>
                        <p class="brand-sub" data-i18n="portfolio_ripb_sub">Moda Premium com Visão Global</p>
                        <p data-i18n="portfolio_ripb_p">Design contemporâneo, qualidade premium e logística internacional. RIPB CLOTHES nasceu da mesma cultura de excelência que define o ALRI Group &mdash; agora traduzida para o mundo da moda.</p>
                        <a href="https://ripb.alrigroup.com" target="_blank" class="brand-link">ripb.alrigroup.com <i class="fas fa-external-link-alt"></i></a>
                    </div>
                </div>
            </div>

            <div class="future-vision fade-in">
                <div class="future-content">
                    <i class="fas fa-rocket future-icon"></i>
                    <h3 data-i18n="portfolio_future_title">Novos Horizontes</h3>
                    <p data-i18n="portfolio_future_p">O ALRI Group est&aacute; em constante incubação. Novas marcas, novos setores, novos mercados. O que come&ccedil;a como linha de c&oacute;digo pode se tornar uma indústria inteira. <strong>Fique de olho.</strong></p>
                </div>
            </div>
        </section>

        <section id="elite" class="fade-in">
            <h2 data-i18n="elite_h2">Projetos de Elite &amp; Tecnologia</h2>
            <p data-i18n="elite_sub" class="section-sub">A engenharia que sustenta cada marca do grupo. Certifica&ccedil;&otilde;es, licen&ccedil;as e produtos que definem nosso padrão.</p>

            <div class="project-showcase">
                <div class="project-card">
                    <div class="project-header">
                        <div class="project-icon-wrapper">
                            <i class="fas fa-bolt"></i>
                        </div>
                        <div>
                            <h3 class="neon-text">WMAROS</h3>
                            <span class="badge">Engineered by AROS</span>
                        </div>
                    </div>
                    <p data-i18n="wmaros_p">O <strong>Windows Mod ALRI Operating System</strong> é nossa flagship de performance. Um ambiente Windows Professional reconstruído e otimizado para entregar o m&aacute;ximo de FPS e a menor latência possivel para power users e gamers de alto nível.</p>
                    <div class="project-footer">
                        <span class="status-tag"><i class="fas fa-bolt"></i> Ultra Performance</span>
                        <span class="license-tag">ARGLFU License</span>
                    </div>
                </div>

                <div class="project-card">
                    <div class="project-header">
                        <div class="project-icon-wrapper">
                            <i class="fas fa-microchip"></i>
                        </div>
                        <div>
                            <h3 class="neon-text">AR-BEMF</h3>
                            <span class="badge">Engineered by ARD</span>
                        </div>
                    </div>
                    <p data-i18n="arbemf_p">A espinha dorsal de nossas opera&ccedil;&otilde;es web. Um micro-framework em C nativo, focado em E2EE (End-to-End Encryption) e hot-reloading granular para sistemas que não podem parar. Puro desempenho em nível de kernel.</p>
                    <div class="project-footer">
                        <span class="status-tag"><i class="fas fa-lock"></i> Alta Seguran&ccedil;a</span>
                        <span class="license-tag">ARGLR License</span>
                    </div>
                </div>
            </div>
        </section>

        <section id="licensing" class="fade-in">
            <h2 data-i18n="lic_h2">Sistema de Licenciamento ARGL</h2>
            <div class="showcase license-showcase">
                <p data-i18n="lic_p">Nossas licen&ccedil;as (ALRI Group Licenses) garantem o equilíbrio entre inova&ccedil;&atilde;o aberta e seguran&ccedil;a institucional. Cada produto do ecossistema opera sob uma destas licen&ccedil;as.</p>
                <div class="license-grid">
                    <div class="license-item">
                        <span class="lic-tag permissive">ARGLP</span>
                        <h4>Permissive</h4>
                        <p data-i18n="lic_arglp">Uso e modifica&ccedil;&atilde;o livres para fins não comerciais (Open Source).</p>
                    </div>
                    <div class="license-item">
                        <span class="lic-tag freeuse">ARGLFU</span>
                        <h4>Free Use</h4>
                        <p data-i18n="lic_arglfu">Livre para uso e distribui&ccedil;&atilde;o, mas proibido de sofrer modifica&ccedil;&otilde;es.</p>
                    </div>
                    <div class="license-item">
                        <span class="lic-tag reserved">ARGLR</span>
                        <h4>Reserved</h4>
                        <p data-i18n="lic_arglr">Uso restrito a parceiros e clientes. C&oacute;digo blindado com garantia.</p>
                    </div>
                </div>
            </div>
        </section>

        <section id="founder" class="fade-in">
            <h2 data-i18n="founder_h2">Missão &amp; Fundador</h2>
            <div class="showcase">
                <div class="showcase-text">
                    <p data-i18n="founder_p1">Nosso prop&oacute;sito é fornecer solu&ccedil;&otilde;es inovadoras nas &aacute;reas mais exigentes &mdash; da engenharia de sistemas ao mercado de moda global. Resolvemos problemas complexos atrav&eacute;s de nossas unidades de neg&oacute;cio:</p>
                    <ul class="tech-list mono">
                        <li>> <i class="fas fa-shield-halved"></i> <span data-i18n="founder_li1">Engenharia de sistemas e seguran&ccedil;a &mdash; o alicerce.</span></li>
                        <li>> <i class="fas fa-cube"></i> <span data-i18n="founder_li2">Desenvolvimento de software e infraestrutura &mdash; a execu&ccedil;&atilde;o.</span></li>
                        <li>> <i class="fas fa-vest"></i> <span data-i18n="founder_li3">Inova&ccedil;&atilde;o em lifestyle e retail &mdash; a expansão.</span></li>
                    </ul>
                    <p data-i18n="founder_p2" style="margin-top: 2rem; border-top: 1px solid var(--border); padding-top: 2rem;">
                        <strong>Fundador:</strong> O ALRI Group foi fundado e é liderado por <strong>Alexsander (@alexsanderalri)</strong>. O nome "ALRI" é um acrônimo de seus sobrenomes &mdash; <strong>Al</strong>meida + <strong>Ri</strong>beiro. Com uma carreira consolidada em seguran&ccedil;a ofensiva, engenharia reversa e modifica&ccedil;&otilde;es profundas de sistema, sua visão segue como o pilar de cada projeto e de cada marca que o grupo abriga.
                    </p>
                </div>
            </div>
        </section>
    </main>

    <footer>
        <div class="footer-socials">
            <a href="https://www.instagram.com/alrigroup" target="_blank" title="Instagram">
                <i class="fab fa-instagram"></i>
            </a>
            <a href="https://dsc.gg/alrigroup" target="_blank" title="Discord">
                <i class="fab fa-discord"></i>
            </a>
            <a href="https://github.com/alrigroup" target="_blank" title="GitHub">
                <i class="fab fa-github"></i>
            </a>
        </div>
        <p class="footer-quote" data-i18n="footer_quote">"Construindo o futuro, uma camada de cada vez."</p>
        <p class="footer-copy">&copy; 2020-2026 ALRI Group. All rights reserved. <span id="easter-egg" title="Restricted Area"><i class="fas fa-lock"></i></span></p>
    </footer>

    <script src="/home/script.js"></script>
</body>

</html>
)homeweb";

static const char *home_style_css = R"homecss(
:root {
    --bg: #020202;
    --surface: rgba(17, 17, 17, 0.4);
    --border: rgba(255, 255, 255, 0.08);
    --text-main: #ffffff;
    --text-muted: #a0a0a0;
    --neon-red: #ff003c;
    --neon-red-glow: #ff003c80;
    --neon-blue: #0088ff;
    --neon-blue-glow: #0088ff80;
    --font-sans: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    --font-mono: 'Courier New', Courier, monospace;
}

* { margin: 0; padding: 0; box-sizing: border-box; }

body {
    background-color: var(--bg);
    color: var(--text-main);
    font-family: var(--font-sans);
    line-height: 1.6;
    overflow-x: hidden;
}

.mono { font-family: var(--font-mono); }

.neon-text {
    color: var(--neon-red);
    text-shadow: 0 0 10px rgba(255, 0, 60, 0.3);
}

.text-gradient {
    background: linear-gradient(90deg, var(--text-main) 0%, var(--neon-red) 100%);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    text-shadow: 0 0 20px var(--neon-red-glow);
}

.navbar {
    padding: 1.5rem 5%;
    display: flex;
    justify-content: space-between;
    align-items: center;
    border-bottom: 1px solid var(--border);
    background: rgba(2, 2, 2, 0.6);
    position: sticky;
    top: 0;
    z-index: 100;
    backdrop-filter: blur(12px);
    -webkit-backdrop-filter: blur(12px);
}

.nav-links { display: flex; gap: 2rem; }

.nav-links a {
    color: var(--text-muted);
    text-decoration: none;
    font-size: 0.85rem;
    transition: color 0.3s ease;
    letter-spacing: 1px;
}

.nav-links a:hover { color: var(--neon-red); }

.lang-switcher button {
    background: transparent;
    border: 1px solid var(--border);
    color: var(--text-main);
    padding: 8px 15px;
    cursor: pointer;
    font-size: 0.8rem;
    transition: all 0.3s ease;
}

.lang-switcher button:hover {
    border-color: var(--neon-red);
    color: var(--neon-red);
    box-shadow: 0 0 10px var(--neon-red-glow);
}

.logo { font-size: 1.5rem; font-weight: 800; letter-spacing: 2px; }
.logo span { color: var(--text-muted); font-weight: 400; }

.hero-ctas { display: flex; gap: 1rem; justify-content: center; flex-wrap: wrap; }

.cta-outline { background: transparent; border-color: var(--text-muted); color: var(--text-muted); }
.cta-outline:hover { border-color: var(--text-main); color: var(--text-main); box-shadow: 0 0 20px rgba(255,255,255,0.1); }

.about-highlight {
    margin-top: 2rem; padding: 1.5rem 2rem;
    border-left: 3px solid var(--neon-red);
    background: rgba(255, 0, 60, 0.03);
    font-style: italic; font-size: 1.05rem; color: var(--text-main);
}

.showcase { max-width: 850px; margin: 0 auto; }
.showcase-text p { margin-bottom: 1.2rem; color: var(--text-main); font-size: 1.05rem; line-height: 1.7; }

.tech-list { list-style: none; margin: 1.5rem 0; }
.tech-list li { margin-bottom: 0.8rem; color: var(--text-muted); font-size: 0.9rem; }
.tech-list li i { margin-right: 6px; color: var(--neon-red); }

.section-sub { text-align: center; color: var(--text-muted); margin-bottom: 3rem; max-width: 700px; margin-left: auto; margin-right: auto; }

.portfolio-block { margin-bottom: 3.5rem; }

.block-header { display: flex; align-items: center; gap: 1rem; margin-bottom: 1.5rem; padding-bottom: 1rem; border-bottom: 1px solid var(--border); }
.block-header h3 { font-size: 1.3rem; font-weight: 700; }
.block-icon { font-size: 1.5rem; color: var(--neon-red); }

.badge-pilar { display: inline-block; font-size: 0.65rem; font-family: var(--font-mono); background: rgba(0,136,255,0.15); color: var(--neon-blue); padding: 2px 10px; margin-left: 8px; text-transform: uppercase; letter-spacing: 1px; vertical-align: middle; }
.badge-expansao { display: inline-block; font-size: 0.65rem; font-family: var(--font-mono); background: rgba(255,0,60,0.15); color: var(--neon-red); padding: 2px 10px; margin-left: 8px; text-transform: uppercase; letter-spacing: 1px; vertical-align: middle; }

.brand-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 1.5rem; }

.brand-card {
    background: var(--surface); backdrop-filter: blur(10px); -webkit-backdrop-filter: blur(10px);
    padding: 2rem; border: 1px solid var(--border); border-top: 1px solid rgba(0,162,255,0.5);
    box-shadow: 0 8px 32px 0 rgba(0,0,0,0.2); transition: border-color 0.3s ease, box-shadow 0.3s ease;
    transform-style: preserve-3d; will-change: transform;
}

.brand-card:hover { box-shadow: 0 15px 40px rgba(0,162,255,0.15); border-top-color: var(--neon-blue); }
.brand-card-wide { grid-column: 1 / -1; }

.brand-card-header { display: flex; align-items: center; gap: 0.8rem; margin-bottom: 0.8rem; }
.brand-card-header i { font-size: 1.8rem; color: var(--neon-red); }
.brand-card-header h4 { font-size: 1.4rem; font-weight: 800; letter-spacing: 1px; }

.brand-sub { font-size: 0.8rem; font-family: var(--font-mono); color: var(--text-muted); margin-bottom: 1rem; }
.brand-card p { color: var(--text-main); font-size: 0.95rem; line-height: 1.6; }

.brand-link { display: inline-block; margin-top: 1rem; color: var(--neon-blue); text-decoration: none; font-size: 0.85rem; font-family: var(--font-mono); transition: color 0.3s ease; }
.brand-link:hover { color: var(--text-main); }

.future-vision { margin-top: 2rem; padding: 3rem; background: rgba(255,0,60,0.02); border: 1px solid rgba(255,0,60,0.15); text-align: center; }
.future-content { max-width: 650px; margin: 0 auto; }
.future-icon { font-size: 2.5rem; color: var(--neon-red); margin-bottom: 1rem; }
.future-content h3 { font-size: 1.4rem; margin-bottom: 0.8rem; letter-spacing: 1px; }
.future-content p { color: var(--text-muted); font-size: 1rem; line-height: 1.7; }

.project-icon-wrapper { width: 60px; height: 60px; display: flex; align-items: center; justify-content: center; background: rgba(255,0,60,0.05); border: 1px solid rgba(255,0,60,0.15); flex-shrink: 0; }
.project-icon-wrapper i { font-size: 1.8rem; color: var(--neon-red); }

main { padding: 0 5%; max-width: 1200px; margin: 0 auto; }
section { padding: 8vh 0; border-bottom: 1px solid var(--border); position: relative; }
section:last-child { border-bottom: none; }

#hero { position: relative; min-height: 80vh; display: flex; align-items: center; justify-content: center; border-bottom: 1px solid var(--border); }

.bg-grid {
    position: absolute; top: 0; left: 0; right: 0; bottom: 0;
    background-size: 50px 50px;
    background-image: linear-gradient(to right, rgba(255,255,255,0.03) 1px, transparent 1px),
        linear-gradient(to bottom, rgba(255,255,255,0.03) 1px, transparent 1px);
    mask-image: linear-gradient(to bottom, rgba(0,0,0,1) 0%, rgba(0,0,0,0) 100%);
    -webkit-mask-image: linear-gradient(to bottom, rgba(0,0,0,1) 0%, rgba(0,0,0,0) 100%);
    z-index: -2;
}

.glow-orb {
    position: absolute; width: 40vw; height: 40vw; max-width: 600px; max-height: 600px;
    background: radial-gradient(circle, rgba(255,0,60,0.15) 0%, rgba(0,0,0,0) 70%);
    top: 50%; left: 50%; transform: translate(-50%, -50%);
    z-index: -1; animation: floatOrb 10s ease-in-out infinite alternate;
}

@keyframes floatOrb {
    0% { transform: translate(-50%,-50%) scale(1); }
    100% { transform: translate(-50%,-55%) scale(1.1); }
}

.hero-content { text-align: center; position: relative; z-index: 1; }

#hero h1 { font-size: clamp(2.5rem,5vw,4.5rem); line-height: 1.1; margin-bottom: 1rem; }
#hero p { color: var(--text-muted); font-size: clamp(1rem,2vw,1.25rem); max-width: 700px; margin: 0 auto 2.5rem auto; }

.cta-button {
    display: inline-block; background: rgba(255,0,60,0.05); color: var(--neon-red);
    text-decoration: none; padding: 1.2rem 2.5rem; border: 1px solid var(--neon-red);
    font-family: var(--font-mono); text-transform: uppercase; font-weight: bold;
    transition: all 0.3s ease; box-shadow: 0 0 15px rgba(255,0,60,0.1);
    position: relative; overflow: hidden; backdrop-filter: blur(4px);
}

.cta-button:hover { background: rgba(255,0,60,0.15); box-shadow: 0 0 30px rgba(255,0,60,0.3); transform: translateY(-2px); }

.cta-button::after {
    content: ''; position: absolute; top: -50%; left: -50%; width: 200%; height: 200%;
    background: linear-gradient(transparent, rgba(255,0,60,0.3), transparent);
    transform: rotate(45deg); animation: sweep 3s infinite linear;
}

@keyframes sweep {
    0% { top: -100%; left: -100%; }
    100% { top: 100%; left: 100%; }
}

h2 { font-size: 2rem; margin-bottom: 3rem; text-align: center; letter-spacing: 1px; }

.grid-cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 2rem; }

.card {
    background: var(--surface); backdrop-filter: blur(10px); -webkit-backdrop-filter: blur(10px);
    padding: 2.5rem; border: 1px solid var(--border); border-top: 1px solid rgba(0,162,255,0.5);
    box-shadow: 0 8px 32px 0 rgba(0,0,0,0.2); transition: border-color 0.3s ease, box-shadow 0.3s ease;
    transform-style: preserve-3d; will-change: transform;
}

.card:hover { box-shadow: 0 15px 40px rgba(0,162,255,0.15); border-top-color: var(--neon-blue); }
.card h3 { color: var(--neon-blue); margin-bottom: 1rem; font-size: 1.2rem; }
.card p { color: var(--text-main); font-size: 1rem; margin-bottom: 1.5rem; }

.sub-list { list-style: none; font-size: 0.85rem; border-left: 1px solid var(--border); padding-left: 1rem; }
.sub-list li { margin-bottom: 0.5rem; color: var(--text-muted); }
.sub-list li span { color: var(--neon-red); font-weight: bold; margin-right: 5px; }

.project-showcase { display: grid; grid-template-columns: repeat(auto-fit, minmax(350px, 1fr)); gap: 3rem; }

.project-card { background: var(--surface); border: 1px solid var(--border); padding: 2.5rem; position: relative; transition: transform 0.4s ease; }
.project-card:hover { transform: translateY(-10px); border-color: var(--neon-red-glow); }

.project-header { display: flex; align-items: center; gap: 1.5rem; margin-bottom: 1.5rem; }
.project-header img { width: 60px; height: 60px; object-fit: contain; }
.project-icon { font-size: 2.5rem; color: var(--neon-red); }

.badge { display: block; font-size: 0.7rem; font-family: var(--font-mono); color: var(--text-muted); text-transform: uppercase; letter-spacing: 1px; }

.project-footer { margin-top: 2rem; display: flex; justify-content: space-between; align-items: center; }
.status-tag { font-size: 0.75rem; color: #4ade80; font-weight: bold; }
.license-tag { font-size: 0.7rem; font-family: var(--font-mono); background: rgba(255,255,255,0.05); padding: 4px 10px; border: 1px solid var(--border); }

.license-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 2rem; margin-top: 3rem; }
.license-item { padding: 2rem; background: rgba(255,255,255,0.02); border: 1px solid var(--border); text-align: center; }

.lic-tag { display: inline-block; padding: 5px 15px; font-weight: bold; font-family: var(--font-mono); margin-bottom: 1rem; border-radius: 4px; }
.permissive { background: #3b82f6; color: white; }
.freeuse { background: #10b981; color: white; }
.reserved { background: #ef4444; color: white; }
.license-item h4 { margin-bottom: 1rem; font-size: 1.2rem; }
.license-item p { font-size: 0.9rem; color: var(--text-muted); }

.footer-socials { display: flex; justify-content: center; gap: 2rem; margin-bottom: 2rem; }
.footer-socials a { color: var(--text-main); font-size: 1.5rem; transition: all 0.3s ease; }
.footer-socials a:hover { color: var(--neon-red); transform: scale(1.2); text-shadow: 0 0 15px var(--neon-red); }
.footer-quote { font-style: italic; color: var(--text-main); margin-bottom: 0.5rem; }
.footer-copy { color: var(--text-muted); }

footer { text-align: center; padding: 3rem 0; border-top: 1px solid var(--border); color: var(--text-muted); font-size: 0.9rem; }

#easter-egg { cursor: default; opacity: 0.3; transition: opacity 0.3s; margin-left: 5px; user-select: none; }
#easter-egg:hover { opacity: 1; cursor: pointer; }

@media (max-width: 768px) {
    .navbar { padding: 0.8rem 4%; gap: 0.5rem; }
    .logo { font-size: 1rem; }
    .nav-links { gap: 0.4rem; }
    .nav-links a { font-size: 0.65rem; padding: 0.3rem 0.4rem; letter-spacing: 0.5px; }
    .lang-switcher button { padding: 5px 8px; font-size: 0.65rem; }
    #hero { min-height: 70vh; padding-bottom: 5rem; }
    #hero h1 { font-size: clamp(1.6rem,6vw,2.2rem); }
    #hero p { font-size: clamp(0.85rem,2.5vw,1rem); max-width: 90%; }
    .hero-content img { width: 160px; }
    .cta-button { padding: 0.9rem 1.5rem; font-size: 0.75rem; }
    .cta-button::after { display: none; }
    main { padding: 0 4%; }
    section { padding: 4vh 0; }
    h2 { font-size: 1.4rem; margin-bottom: 2rem; }
    .section-sub { font-size: 0.85rem; margin-bottom: 2rem; }
    .showcase-text p { font-size: 0.95rem; }
    .about-highlight { padding: 1rem 1.2rem; font-size: 0.95rem; }
    .portfolio-block { margin-bottom: 2.5rem; }
    .block-header { gap: 0.6rem; }
    .block-header h3 { font-size: 1rem; line-height: 1.4; }
    .block-icon { font-size: 1.2rem; }
    .badge-pilar, .badge-expansao { font-size: 0.55rem; padding: 1px 7px; margin-left: 4px; }
    .brand-grid { grid-template-columns: 1fr; gap: 1rem; }
    .brand-card { padding: 1.2rem; }
    .brand-card-header i { font-size: 1.4rem; }
    .brand-card-header h4 { font-size: 1.1rem; }
    .brand-sub { font-size: 0.7rem; }
    .brand-card p { font-size: 0.85rem; }
    .future-vision { padding: 2rem 1.2rem; }
    .future-icon { font-size: 1.8rem; }
    .future-content h3 { font-size: 1.1rem; }
    .future-content p { font-size: 0.85rem; }
    .project-showcase { grid-template-columns: 1fr; gap: 1.5rem; }
    .project-card { padding: 1.5rem; }
    .project-header { gap: 1rem; }
    .project-icon-wrapper { width: 48px; height: 48px; }
    .project-icon-wrapper i { font-size: 1.4rem; }
    .project-footer { flex-direction: column; align-items: flex-start; gap: 0.5rem; }
    .license-grid { grid-template-columns: 1fr; gap: 1rem; margin-top: 2rem; }
    .license-item { padding: 1.5rem; }
    .license-item h4 { font-size: 1rem; }
    .license-item p { font-size: 0.8rem; }
    .tech-list li { font-size: 0.8rem; }
    footer { padding: 2rem 4%; }
    .footer-socials { gap: 1.5rem; }
    .footer-socials a { font-size: 1.3rem; }
}

@media (max-width: 480px) {
    .navbar { padding: 0.6rem 4%; }
    .logo { font-size: 0.85rem; }
    .nav-links { gap: 0.2rem; }
    .nav-links a { font-size: 0.55rem; padding: 0.2rem 0.3rem; }
    .lang-switcher button { padding: 3px 6px; font-size: 0.55rem; }
    #hero { min-height: 55vh; }
    #hero h1 { font-size: clamp(1.2rem,5vw,1.4rem); }
    #hero p { font-size: clamp(0.75rem,2.5vw,0.85rem); }
    .hero-content img { width: 110px; margin-bottom: 1.2rem !important; }
    .hero-ctas { flex-direction: column; align-items: center; gap: 0.6rem; }
    .cta-button { padding: 0.6rem 1rem; font-size: 0.6rem; width: 100%; max-width: 240px; }
    section { padding: 2.5vh 0; }
    h2 { font-size: 1.1rem; margin-bottom: 1.2rem; }
    .brand-card { padding: 1rem; }
    .project-card { padding: 1.2rem; }
    .future-vision { padding: 1.5rem 1rem; }
}

.fade-in { opacity: 0; transform: translateY(30px); transition: opacity 0.8s cubic-bezier(0.16,1,0.3,1), transform 0.8s cubic-bezier(0.16,1,0.3,1); will-change: opacity, transform; }
.fade-in.visible { opacity: 1; transform: translateY(0); }
)homecss";

static const char *home_script_js = R"homejs(
const translations = {
    en: {
        lang_btn: "PT-BR",
        nav_about: "Holding",
        nav_portfolio: "Brands",
        nav_elite: "Technology",
        hero_h1: 'The ecosystem that connects<br><span class="text-gradient">technology, innovation and new markets</span>',
        hero_sub: 'Our DNA is tech. Our horizons are limitless. <strong>ALRI Group</strong> &mdash; a multidisciplinary holding building the future in layers.',
        hero_cta1: "Meet Our Brands",
        hero_cta2: "Explore the Ecosystem",
        about_h2: "Who We Are",
        about_p1: 'The <strong>ALRI Group</strong> was born in <strong>2020</strong> from the mind of <strong>Alexsander</strong> &mdash; a systems engineer with a talent for seeing beyond the code. What started as a research lab in deep kernel modifications and offensive security became something bigger. Much bigger.',
        about_p2: 'Today, ALRI Group is a multidisciplinary <strong>Holding Company</strong>. We keep our tech soul alive through <strong>ARD &mdash; ALRI Development</strong> (systems engineering, custom operating systems like <strong>AROS</strong>, and high-performance scripts like <strong>ARFS</strong>), while breaking new ground with <strong>RIPB CLOTHES</strong> &mdash; our global premium apparel brand. And this is just the beginning.',
        about_p3: 'From a tech dream in 2020 to a solid business management structure. The group grows, the brands multiply, the DNA remains.',
        portfolio_h2: "Our Portfolio",
        portfolio_sub: "Each brand in the group represents a strategic pillar. Together, they form a complete ecosystem.",
        portfolio_tech_title: 'Tech Division <span class="badge-pilar">The Pillar</span>',
        portfolio_ard_sub: "ALRI Development &mdash; Systems Engineering",
        portfolio_ard_p: "Reverse engineering, deep kernel modifications and high-performance infrastructure. ARD is the engine that drives the group.",
        portfolio_aros_sub: "ALRI Operating System",
        portfolio_aros_p: "Custom operating systems &mdash; Windows, Android and Linux modified for maximum performance, security and absolute control.",
        portfolio_arfs_sub: "FiveM Scripts &amp; Protocols",
        portfolio_arfs_p: "Elite scripts for FiveM &mdash; including the Apex RP ecosystem and the ALRI Protect anti-cheat system. Performance, stability and innovation.",
        portfolio_life_title: 'Lifestyle &amp; Retail Division <span class="badge-expansao">The Expansion</span>',
        portfolio_ripb_sub: "Premium Fashion with Global Vision",
        portfolio_ripb_p: "Contemporary design, premium quality and international logistics. RIPB CLOTHES was born from the same culture of excellence that defines ALRI Group &mdash; now translated into the world of fashion.",
        portfolio_future_title: "New Horizons",
        portfolio_future_p: 'ALRI Group is constantly incubating. New brands, new sectors, new markets. What starts as a line of code can become an entire industry. <strong>Watch this space.</strong>',
        elite_h2: "Elite Projects &amp; Technology",
        elite_sub: "The engineering that underpins every brand in the group. Certifications, licenses and products that define our standard.",
        wmaros_p: 'The <strong>Windows Mod ALRI Operating System</strong> is our performance flagship. A Windows Professional environment rebuilt and optimized to deliver maximum FPS and the lowest possible latency for power users and elite gamers.',
        arbemf_p: 'The backbone of our web operations. A native C micro-framework focused on E2EE (End-to-End Encryption) and granular hot-reloading for systems that cannot stop. Pure kernel-level performance.',
        lic_h2: "ARGL Licensing System",
        lic_p: "Our licenses (ALRI Group Licenses) ensure the balance between open innovation and institutional security. Every product in the ecosystem operates under one of these licenses.",
        lic_arglp: "Free usage and modification for non-commercial purposes (Open Source).",
        lic_arglfu: "Free to use and distribute, but prohibited from undergoing modifications.",
        lic_arglr: "Restricted use for partners and clients. Hardened code with warranty.",
        founder_h2: "Mission &amp; Founder",
        founder_p1: "Our purpose is to deliver innovative solutions in the most demanding fields &mdash; from systems engineering to the global fashion market. We solve complex problems through our business units:",
        founder_li1: "Systems engineering and security &mdash; the foundation.",
        founder_li2: "Software development and infrastructure &mdash; the execution.",
        founder_li3: "Innovation in lifestyle and retail &mdash; the expansion.",
        founder_p2: '<strong>Founder:</strong> ALRI Group was founded and is led by <strong>Alexsander (@alexsanderalri)</strong>. The name "ALRI" is an acronym of his surnames &mdash; <strong>Al</strong>meida + <strong>Ri</strong>beiro. With a consolidated career in offensive security, reverse engineering and deep system modifications, his vision remains the pillar of every project and every brand the group houses.',
        footer_quote: '"Building the future, one layer at a time."'
    },
    pt: {
        lang_btn: "EN-US",
        nav_about: "Holding",
        nav_portfolio: "Marcas",
        nav_elite: "Tecnologia",
        hero_h1: 'O ecossistema que conecta<br><span class="text-gradient">tecnologia, inovação e novos mercados</span>',
        hero_sub: 'Nosso DNA é tech. Nossos horizontes são ilimitados. <strong>ALRI Group</strong> &mdash; uma holding multidisciplinar que constrói o futuro em camadas.',
        hero_cta1: "Conhe&ccedil;a Nossas Marcas",
        hero_cta2: "Explore o Ecossistema",
        about_h2: "Quem Somos",
        about_p1: 'O <strong>ALRI Group</strong> nasceu em <strong>2020</strong> da mente de <strong>Alexsander</strong> &mdash; um engenheiro de sistemas com talento para enxergar além do código. O que começou como laboratório de pesquisa em modificações profundas de kernel e segurança ofensiva se transformou em algo maior. Muito maior.',
        about_p2: 'Hoje, o ALRI Group é uma <strong>Holding Company</strong> multidisciplinar. Mantemos nossa alma tecnológica viva atrav&eacute;s da <strong>ARD &mdash; ALRI Development</strong> (engenharia de sistemas, sistemas operacionais customizados como o <strong>AROS</strong> e scripts de alto desempenho como o <strong>ARFS</strong>), enquanto rompemos fronteiras com a <strong>RIPB CLOTHES</strong> &mdash; nossa marca global de vestu&aacute;rio premium. E isso é apenas o come&ccedil;o.',
        about_p3: 'De um sonho tech em 2020 a uma estrutura sólida de gestão de negócios. O grupo cresce, as marcas se multiplicam, o DNA permanece.',
        portfolio_h2: "Nosso Portfólio",
        portfolio_sub: "Cada marca do grupo representa um pilar estrat&eacute;gico. Juntas, formam um ecossistema completo.",
        portfolio_tech_title: 'Divisão Tech <span class="badge-pilar">O Pilar</span>',
        portfolio_ard_sub: "ALRI Development &mdash; Engenharia de Sistemas",
        portfolio_ard_p: "Engenharia reversa, modificações profundas de kernel e infraestrutura de alto desempenho. A ARD é o motor que move o grupo.",
        portfolio_aros_sub: "ALRI Operating System",
        portfolio_aros_p: "Sistemas operacionais customizados &mdash; Windows, Android e Linux modificados para m&aacute;xima performance, seguran&ccedil;a e controle absoluto.",
        portfolio_arfs_sub: "FiveM Scripts &amp; Protocolos",
        portfolio_arfs_p: "Scripts de elite para FiveM &mdash; incluindo o ecossistema Apex RP e o sistema anticheat ALRI Protect. Performance, estabilidade e inova&ccedil;&atilde;o.",
        portfolio_life_title: 'Divisão Lifestyle &amp; Retail <span class="badge-expansao">A Expansão</span>',
        portfolio_ripb_sub: "Moda Premium com Visão Global",
        portfolio_ripb_p: "Design contemporâneo, qualidade premium e logística internacional. RIPB CLOTHES nasceu da mesma cultura de excelência que define o ALRI Group &mdash; agora traduzida para o mundo da moda.",
        portfolio_future_title: "Novos Horizontes",
        portfolio_future_p: 'O ALRI Group est&aacute; em constante incubação. Novas marcas, novos setores, novos mercados. O que come&ccedil;a como linha de c&oacute;digo pode se tornar uma indústria inteira. <strong>Fique de olho.</strong>',
        elite_h2: "Projetos de Elite &amp; Tecnologia",
        elite_sub: "A engenharia que sustenta cada marca do grupo. Certifica&ccedil;&otilde;es, licen&ccedil;as e produtos que definem nosso padrão.",
        wmaros_p: 'O <strong>Windows Mod ALRI Operating System</strong> é nossa flagship de performance. Um ambiente Windows Professional reconstruído e otimizado para entregar o m&aacute;ximo de FPS e a menor latência possivel para power users e gamers de alto nível.',
        arbemf_p: 'A espinha dorsal de nossas opera&ccedil;&otilde;es web. Um micro-framework em C nativo, focado em E2EE (End-to-End Encryption) e hot-reloading granular para sistemas que não podem parar. Puro desempenho em nível de kernel.',
        lic_h2: "Sistema de Licenciamento ARGL",
        lic_p: "Nossas licen&ccedil;as (ALRI Group Licenses) garantem o equilíbrio entre inova&ccedil;&atilde;o aberta e seguran&ccedil;a institucional. Cada produto do ecossistema opera sob uma destas licen&ccedil;as.",
        lic_arglp: "Uso e modifica&ccedil;&atilde;o livres para fins não comerciais (Open Source).",
        lic_arglfu: "Livre para uso e distribui&ccedil;&atilde;o, mas proibido de sofrer modifica&ccedil;&otilde;es.",
        lic_arglr: "Uso restrito a parceiros e clientes. C&oacute;digo blindado com garantia.",
        founder_h2: "Missão &amp; Fundador",
        founder_p1: "Nosso prop&oacute;sito é fornecer solu&ccedil;&otilde;es inovadoras nas &aacute;reas mais exigentes &mdash; da engenharia de sistemas ao mercado de moda global. Resolvemos problemas complexos atrav&eacute;s de nossas unidades de neg&oacute;cio:",
        founder_li1: "Engenharia de sistemas e seguran&ccedil;a &mdash; o alicerce.",
        founder_li2: "Desenvolvimento de software e infraestrutura &mdash; a execu&ccedil;&atilde;o.",
        founder_li3: "Inova&ccedil;&atilde;o em lifestyle e retail &mdash; a expansão.",
        founder_p2: '<strong>Fundador:</strong> O ALRI Group foi fundado e é liderado por <strong>Alexsander (@alexsanderalri)</strong>. O nome "ALRI" é um acrônimo de seus sobrenomes &mdash; <strong>Al</strong>meida + <strong>Ri</strong>beiro. Com uma carreira consolidada em seguran&ccedil;a ofensiva, engenharia reversa e modifica&ccedil;&otilde;es profundas de sistema, sua visão segue como o pilar de cada projeto e de cada marca que o grupo abriga.',
        footer_quote: '"Construindo o futuro, uma camada de cada vez."'
    }
};

let currentLang = localStorage.getItem('alri_lang') || 'en';

function updateUI() {
    const langData = translations[currentLang];
    document.querySelectorAll('[data-i18n]').forEach(el => {
        const key = el.getAttribute('data-i18n');
        if (langData[key]) { el.innerHTML = langData[key]; }
    });
    document.getElementById('langText').innerText = langData.lang_btn;
    document.documentElement.lang = currentLang === 'en' ? 'en' : 'pt-BR';
}

function toggleLanguage() {
    currentLang = currentLang === 'en' ? 'pt' : 'en';
    localStorage.setItem('alri_lang', currentLang);
    updateUI();
}

document.addEventListener('DOMContentLoaded', () => {
    updateUI();
    const easterEgg = document.getElementById('easter-egg');
    if (easterEgg) {
        easterEgg.addEventListener('click', () => { window.location.href = '/manager/login'; });
    }
    const fadeElements = document.querySelectorAll('.fade-in');
    const observer = new IntersectionObserver((entries, observer) => {
        let delay = 0;
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                if (entry.target.classList.contains('stagger-item')) {
                    setTimeout(() => { entry.target.classList.add('visible'); }, delay);
                    delay += 150;
                } else { entry.target.classList.add('visible'); }
                observer.unobserve(entry.target);
            }
        });
    }, { threshold: 0.10, rootMargin: "0px 0px -50px 0px" });
    fadeElements.forEach(el => observer.observe(el));
    const cards = document.querySelectorAll('.brand-card');
    cards.forEach(card => {
        card.addEventListener('mousemove', (e) => {
            const rect = card.getBoundingClientRect();
            const x = e.clientX - rect.left;
            const y = e.clientY - rect.top;
            const centerX = rect.width / 2;
            const centerY = rect.height / 2;
            const rotateX = ((y - centerY) / centerY) * -5;
            const rotateY = ((x - centerX) / centerX) * 5;
            card.style.transform = `perspective(1000px) rotateX(${rotateX}deg) rotateY(${rotateY}deg) scale3d(1.02, 1.02, 1.02)`;
        });
        card.addEventListener('mouseleave', () => {
            card.style.transform = 'perspective(1000px) rotateX(0deg) rotateY(0deg) scale3d(1, 1, 1)';
            card.style.transition = 'transform 0.5s ease';
        });
        card.addEventListener('mouseenter', () => { card.style.transition = 'none'; });
    });
});
)homejs";

#endif
