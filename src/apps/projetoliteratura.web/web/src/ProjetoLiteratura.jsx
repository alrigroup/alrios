/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

import { useEffect, useRef, useState } from 'react';

import machado1905 from './assets/machado-1905.png';
import machadoAbl from './assets/machado-abl.jpg';

const EPIGRAPH =
  '"Ao verme que primeiro roeu as frias carnes do meu cadáver dedico como saudosa lembrança estas memórias póstumas."';

const TWEETS = [
  { body: 'Não tive filhos, não transmiti a nenhuma criatura o legado da nossa miséria.', cap: 'Cap. CLX — Das Negativas', likes: '372' },
  { body: 'Marcela amou-me durante quinze meses e onze contos de réis.', cap: 'Cap. XVII — Marcela', likes: '1.2K' },
  { body: 'A franqueza é a primeira virtude de um defunto.', cap: 'Cap. I — Óbito do Autor', likes: '891' },
  { body: 'O menino é o pai do homem.', cap: 'Cap. XI — O Menino é Pai do Homem', likes: '2.3K' },
  { body: 'Ao verme que primeiro roeu as frias carnes do meu cadáver dedico como saudosa lembrança estas memórias póstumas.', cap: 'Dedicatória', likes: '5.4K' }
];

function TweetCard({ tweet }) {
  return (
    <div className="tweet-card">
      <div className="tweet-header">
        <div className="tweet-avatar">BC</div>
        <div className="tweet-author">
          <strong>Brás Cubas</strong>
          <span>@defunto_autor</span>
        </div>
        <i className="fas fa-ellipsis tweet-more"></i>
      </div>
      <div className="tweet-body">{tweet.body}</div>
      <div className="tweet-footer">
        <span>{tweet.cap}</span>
        <div className="tweet-actions">
          <span><i className="far fa-heart"></i> {tweet.likes}</span>
        </div>
      </div>
    </div>
  );
}

export default function ProjetoLiteratura() {
  const rootRef = useRef(null);
  const [typed, setTyped] = useState('');
  const [cursorHidden, setCursorHidden] = useState(false);
  const [expanded, setExpanded] = useState(null);
  const [year, setYear] = useState('');

  useEffect(() => {
    setYear(String(new Date().getFullYear()));
  }, []);

  useEffect(() => {
    let index = 0;
    let timer = null;
    const start = setTimeout(() => {
      const type = () => {
        setTyped(EPIGRAPH.slice(0, index));
        index++;
        if (index > EPIGRAPH.length) {
          setCursorHidden(true);
          return;
        }
        const delay = EPIGRAPH[index - 1] === ',' || EPIGRAPH[index - 1] === '.' || EPIGRAPH[index - 1] === '"' ? 80 : 35;
        timer = setTimeout(type, delay);
      };
      type();
    }, 600);
    return () => {
      clearTimeout(start);
      clearTimeout(timer);
    };
  }, []);

  useEffect(() => {
    const root = rootRef.current;
    if (!root) return;

    const els = root.querySelectorAll('.fade-in');
    let observer = null;
    if (typeof IntersectionObserver === 'undefined') {
      els.forEach((el) => el.classList.add('visible'));
    } else {
      observer = new IntersectionObserver(
        (entries, obs) => {
          entries.forEach((entry) => {
            if (entry.isIntersecting) {
              entry.target.classList.add('visible');
              obs.unobserve(entry.target);
            }
          });
        },
        { threshold: 0.15, rootMargin: '0px 0px -40px 0px' }
      );
      els.forEach((el) => observer.observe(el));
    }

    const parallaxEls = root.querySelectorAll('.parallax');
    let rafId = null;
    const updateParallax = () => {
      parallaxEls.forEach((el) => {
        const rect = el.getBoundingClientRect();
        const center = rect.top + rect.height / 2;
        const viewportCenter = window.innerHeight / 2;
        const offset = (center - viewportCenter) * 0.04;
        el.style.transform = 'translateY(' + offset + 'px)';
      });
      rafId = requestAnimationFrame(updateParallax);
    };
    updateParallax();

    const tiltCards = root.querySelectorAll('[data-tilt]');
    const tiltHandlers = [];
    tiltCards.forEach((card) => {
      const move = (e) => {
        const rect = card.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        const centerX = rect.width / 2;
        const centerY = rect.height / 2;
        const rotateY = ((x - centerX) / centerX) * 8;
        const rotateX = ((centerY - y) / centerY) * 8;
        card.style.transform = 'perspective(800px) rotateX(' + rotateX + 'deg) rotateY(' + rotateY + 'deg) translateY(-5px)';
      };
      const leave = () => {
        card.style.transform = 'perspective(800px) rotateX(0deg) rotateY(0deg) translateY(0)';
      };
      card.addEventListener('mousemove', move);
      card.addEventListener('mouseleave', leave);
      tiltHandlers.push([card, move, leave]);
    });

    const bar = root.querySelector('#progress-bar');
    const onScroll = () => {
      if (!bar) return;
      const scrollTop = window.scrollY;
      const docHeight = document.documentElement.scrollHeight - window.innerHeight;
      const progress = docHeight > 0 ? (scrollTop / docHeight) * 100 : 0;
      bar.style.width = progress + '%';
    };
    window.addEventListener('scroll', onScroll);

    return () => {
      if (observer) observer.disconnect();
      cancelAnimationFrame(rafId);
      tiltHandlers.forEach(([card, move, leave]) => {
        card.removeEventListener('mousemove', move);
        card.removeEventListener('mouseleave', leave);
      });
      window.removeEventListener('scroll', onScroll);
    };
  }, []);

  useEffect(() => {
    if (!rootRef.current) return;
    const cursor = rootRef.current.querySelector('#cursor');
    const trail = rootRef.current.querySelector('#cursor-trail');
    if (!cursor || !trail) return;

    let mouseX = 0, mouseY = 0, trailX = 0, trailY = 0;
    const onMove = (e) => {
      mouseX = e.clientX;
      mouseY = e.clientY;
      cursor.style.left = mouseX + 'px';
      cursor.style.top = mouseY + 'px';
    };
    let rafId;
    const animateTrail = () => {
      trailX += (mouseX - trailX) * 0.15;
      trailY += (mouseY - trailY) * 0.15;
      trail.style.left = trailX + 'px';
      trail.style.top = trailY + 'px';
      rafId = requestAnimationFrame(animateTrail);
    };
    animateTrail();
    const onLeave = () => { cursor.style.opacity = '0'; trail.style.opacity = '0'; };
    const onEnter = () => { cursor.style.opacity = '1'; trail.style.opacity = '0.25'; };
    document.addEventListener('mousemove', onMove);
    document.addEventListener('mouseleave', onLeave);
    document.addEventListener('mouseenter', onEnter);
    document.body.classList.add('custom-cursor');
    return () => {
      cancelAnimationFrame(rafId);
      document.removeEventListener('mousemove', onMove);
      document.removeEventListener('mouseleave', onLeave);
      document.removeEventListener('mouseenter', onEnter);
      document.body.classList.remove('custom-cursor');
    };
  }, []);

  const toggleBento = (idx) => setExpanded((prev) => (prev === idx ? null : idx));

  return (
    <div ref={rootRef}>
      <div id="progress-bar"></div>
      <div id="cursor"></div>
      <div id="cursor-trail"></div>
      <div id="particles"></div>

      <nav className="navbar">
        <span className="nav-logo">&dagger; Memórias Póstumas &dagger;</span>
        <ul className="nav-links">
          <li><a href="#hero">Portal</a></li>
          <li><a href="#bento">Machado</a></li>
          <li><a href="#galeria">Personagens</a></li>
          <li><a href="#citacoes">Citações</a></li>
          <li><a href="#realismo">Realismo</a></li>
          <li><a href="#resenha">Resenha</a></li>
        </ul>
      </nav>

      <main>
        <section id="hero" className="hero-section">
          <div className="hero-fog">
            <svg viewBox="0 0 1440 900" preserveAspectRatio="xMidYMid slice">
              <defs>
                <filter id="fog">
                  <feTurbulence type="fractalNoise" baseFrequency="0.012" numOctaves="4" seed="3" />
                  <feColorMatrix type="saturate" values="0" />
                  <feComponentTransfer>
                    <feFuncA type="linear" slope="0.15" />
                  </feComponentTransfer>
                </filter>
              </defs>
              <rect width="100%" height="100%" filter="url(#fog)" />
            </svg>
          </div>
          <div className="hero-content">
            <div className="hero-epigraph-wrapper">
              <p className="hero-epigraph" id="typewriter-text">{typed}</p>
              <span className="hero-cursor" id="typewriter-cursor" style={{ display: cursorHidden ? 'none' : 'inline' }}>|</span>
            </div>
            <a href="#bento" className="hero-btn hero-btn-glow">
              <i className="fas fa-skull"></i> Iniciar Leitura
            </a>
          </div>
          <div className="hero-scroll-indicator">
            <span>Role para explorar</span>
            <i className="fas fa-chevron-down"></i>
          </div>
        </section>

        <section id="bento" className="section bento-section">
          <div className="section-container">
            <h2 className="section-title fade-in"><i className="fas fa-feather-alt"></i> Machado de Assis &amp; a Obra</h2>
            <p className="section-subtitle fade-in">A vida do Bruxo do Cosme Velho e a criação de uma obra-prima</p>
            <div className="bento-grid">
              <div className="bento-item bento-image fade-in">
                <div className="bento-img-inner">
                  <img src={machado1905} alt="Machado de Assis" loading="lazy" />
                  <div className="bento-img-overlay"></div>
                </div>
                <p className="bento-img-caption">Machado na Academia Brasileira de Letras</p>
              </div>

              {[
                { icon: 'fa-seedling', title: 'Origens e Superação', text: 'Nascido no Morro do Livramento (RJ) em 1839, Joaquim Maria Machado de Assis era neto de escravizados alforriados. De origem humilde, mulato, gago e epilético, ele foi autodidata. Aprendeu francês na padaria onde trabalhava e logo começou a publicar seus primeiros poemas, provando que sua genialidade superaria qualquer barreira social.' },
                { icon: 'fa-heart', title: 'O Amor e a Maturidade', text: 'Seu casamento com a portuguesa Carolina Augusta Xavier de Novais foi um divisor de águas. Ela apresentou a ele os grandes clássicos da literatura inglesa, ajudando a refinar seu humor britânico e sua ironia fina, que se tornariam sua marca registrada.' },
                { icon: 'fa-hat-wizard', title: 'O Bruxo do Cosme Velho', text: 'Consagrado em vida, Machado fundou e foi o primeiro presidente da Academia Brasileira de Letras (ABL) em 1897. Seu apelido, "Bruxo do Cosme Velho", veio do bairro onde morava e de sua capacidade mágica de ler a alma e os segredos da sociedade brasileira.', wide: true }
              ].map((card, idx) => (
                <div
                  key={card.title}
                  className={`bento-item bento-card ${card.wide ? 'bento-card-wide ' : ''}${expanded === idx ? 'expanded ' : ''}fade-in`}
                >
                  <button className="bento-card-header" onClick={() => toggleBento(idx)}>
                    <span className="bento-card-icon"><i className={`fas ${card.icon}`}></i></span>
                    <span className="bento-card-title">{card.title}</span>
                    <i className="fas fa-plus bento-card-toggle"></i>
                  </button>
                  <div className="bento-card-body">
                    <div className="bento-card-content">
                      <p>{card.text}</p>
                    </div>
                  </div>
                </div>
              ))}

              <div className="bento-item bento-text fade-in">
                <h3 className="bento-text-title">A Gênese de uma Obra-Prima</h3>
                <p><em>"Memórias Póstumas de Brás Cubas"</em> não nasceu como um livro fechado. A obra foi publicada originalmente em folhetins na <em>Revista Brasileira</em>, entre março e dezembro de 1880. Somente em 1881 foi lançada como livro.</p>
                <div className="bento-text-divider"><i className="fas fa-skull"></i></div>
                <h4 className="bento-text-subtitle">O Choque Literário</h4>
                <p>Imagine a reação dos leitores do século XIX ao abrirem um romance e lerem a dedicatória: <em>"Ao verme que primeiro roeu as frias carnes do meu cadáver"</em>. Machado rompeu com todas as regras: capítulos curtíssimos, conversa irônica com o leitor, e um protagonista cínico e morto. A literatura brasileira mudou para sempre.</p>
              </div>
            </div>
            <div className="bento-images-row">
              <div className="bento-image-wrapper fade-in">
                <div className="bento-image duotone">
                  <img src={machado1905} alt="Machado de Assis, 1905" loading="lazy" />
                </div>
                <p className="bento-caption">Machado na Academia Brasileira de Letras</p>
              </div>
              <div className="bento-image-wrapper parallax fade-in">
                <div className="bento-image duotone">
                  <img src={machadoAbl} alt="Machado de Assis na ABL" loading="lazy" />
                </div>
                <p className="bento-caption">Machado de Assis, 1905 — Arquivo Nacional</p>
              </div>
            </div>
          </div>
        </section>

        <section id="galeria" className="section gallery-section">
          <div className="section-container">
            <h2 className="section-title fade-in"><i className="fas fa-masks-theater"></i> Galeria de Personagens</h2>
            <div className="card-grid">
              {[
                { icon: 'fa-user-tie', name: 'Brás Cubas', tagline: 'O defunto autor', desc: 'Homem rico, medíocre, que narra seus fracassos do além-túmulo com ironia mordaz.' },
                { icon: 'fa-crown', name: 'Virgília', tagline: 'Grande amor', desc: 'Ambiciosa, casa-se com Lobo Neves por status social, mas mantém um caso com Brás Cubas.' },
                { icon: 'fa-coins', name: 'Marcela', tagline: 'Amor de juventude', desc: '"Marcela amou-me durante quinze meses e onze contos de réis." A cortesã que marcou Brás.' },
                { icon: 'fa-brain', name: 'Quincas Borba', tagline: 'Filósofo louco', desc: 'Criador do "Humanitismo" — filosofia que justifica a crueldade como lei natural.' }
              ].map((c) => (
                <article className="character-card fade-in" data-tilt key={c.name}>
                  <div className="card-img">
                    <i className={`fas ${c.icon}`}></i>
                  </div>
                  <h3 className="card-name">{c.name}</h3>
                  <p className="card-tagline">{c.tagline}</p>
                  <p className="card-desc">{c.desc}</p>
                </article>
              ))}
            </div>
          </div>
        </section>

        <section id="citacoes" className="section citacoes-section">
          <div className="marquee-overlay"></div>
          <div className="section-container marquee-section-container">
            <h2 className="section-title fade-in"><i className="fas fa-quote-right"></i> Citações Marcantes</h2>
            <p className="section-subtitle fade-in">Trechos que definem o espírito da obra</p>
          </div>
          <div className="marquee-wrapper fade-in">
            <div className="marquee-track">
              <div className="marquee-group">
                {TWEETS.map((tw, i) => <TweetCard tweet={tw} key={i} />)}
              </div>
              <div className="marquee-group" aria-hidden="true">
                {TWEETS.map((tw, i) => <TweetCard tweet={tw} key={i} />)}
              </div>
            </div>
          </div>
        </section>

        <section id="realismo" className="section realismo-section">
          <div className="section-container">
            <h2 className="section-title fade-in"><i className="fas fa-landmark"></i> O Realismo na Literatura</h2>
            <p className="section-subtitle fade-in">Contexto escolar: as características do movimento literário</p>
            <div className="realismo-grid">
              {[
                { icon: 'fa-search', title: 'Análise Psicológica', desc: 'Foco no interior dos personagens, revelando suas hipocrisias e verdadeiros interesses.' },
                { icon: 'fa-mask', title: 'Fim das Ilusões', desc: 'O casamento não é por amor, é por status. O Romantismo morre para dar lugar à dura realidade.' },
                { icon: 'fa-scale-balanced', title: 'Crítica Social', desc: 'Retrato irônico da burguesia e da elite carioca do Segundo Reinado, focada em aparências e poder.' }
              ].map((r) => (
                <div className="realismo-card fade-in" key={r.title}>
                  <div className="realismo-icon"><i className={`fas ${r.icon}`}></i></div>
                  <h3>{r.title}</h3>
                  <p>{r.desc}</p>
                </div>
              ))}
            </div>
          </div>
        </section>

        <section id="resenha" className="section review-section">
          <div className="section-container">
            <h2 className="section-title fade-in"><i className="fas fa-book-open"></i> Análise Crítica</h2>
            <div className="review-content fade-in">
              <h3>A Radiografia de uma Sociedade Hipócrita</h3>
              <p>Publicado em 1881, <em>Memórias Póstumas de Brás Cubas</em> marca o início do Realismo no Brasil. Machado de Assis rompe com o Romantismo ao apresentar um anti-herói cínico, um defunto que narra sua própria história sem concessões ao sentimentalismo.</p>
              <p>A obra é um mosaico de inovações narrativas: digressões filosóficas, capítulos curtos, interlocução direta com o leitor e a famosa ideia de que a vida é um "pândego" equilíbrio de egoísmos. Machado dissecou a elite carioca com bisturi de gelo, expondo o racismo, o compadrio e a hipocrisia que sustentavam o Império.</p>
              <p>O <em>Humanitismo</em> de Quincas Borba — "ao vencedor, as batatas" — é uma sátira feroz ao darwinismo social e ao liberalismo selvagem do século XIX. Mais de 140 anos depois, o livro segue assombrosamente atual.</p>
              <div className="review-footer">
                <span><i className="fas fa-star"></i> Clássico Ouro</span>
                <span><i className="fas fa-calendar"></i> 1881</span>
                <span><i className="fas fa-flag"></i> Realismo</span>
              </div>
            </div>
          </div>
        </section>

        <section id="creditos" className="section creditos-section">
          <div className="section-container">
            <h2 className="section-title fade-in"><i className="fas fa-users"></i> Créditos</h2>
            <p className="section-subtitle fade-in">Equipe responsável por este projeto literário</p>
            <div className="creditos-grid">
              {['Alexsander AR', 'Cauê Santiago', 'Davi Alves', 'Bia M.', 'Luka Calabrio', 'Luiz Fernando'].map((n) => (
                <div className="credito-card fade-in" key={n}>
                  <div className="credito-avatar"><i className="fas fa-user-graduate"></i></div>
                  <h3>{n}</h3>
                </div>
              ))}
            </div>
            <div className="creditos-institution fade-in">
              <p className="creditos-turma">Turma 3 Vestibular</p>
              <p className="creditos-curso">Curso ZeroHum — Maricá</p>
            </div>
          </div>
        </section>
      </main>

      <footer className="footer">
        <p>&copy; {year} — O Blog do Além. Nenhum direito reservado, pois estou morto.</p>
      </footer>
    </div>
  );
}
