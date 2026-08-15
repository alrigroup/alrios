/**
 * ALRIOS ARWN - Multi-Language Benchmark Suite (React 18 Dark Theme)
 * Algoritmo com Manipulação de Memória Linear (1MB Buffer) & Pointers
 * Faz o download e desempacotamento de cada contêiner .arweb sob demanda:
 * - c_engine.arweb (C Nativo WASM com Direct Pointers)
 * - cpp_engine.arweb (C++20 Clang WASM Vector Buffer)
 * - go_engine.arweb (Go WASM Slice Memory)
 * - rust_engine.arweb (Rust wasm32 Zero-Cost Pointer Memory)
 * - JavaScript JIT Engine (Uint32Array com Bounds Check & GC Overhead)
 */

// Desempacotador binário nativo do contêiner .arweb (layout ARWN)
function unpackArwebWasm(arrayBuffer) {
  const bytes = new Uint8Array(arrayBuffer);
  const dv = new DataView(arrayBuffer);
  
  if (bytes.length < 48) throw new Error('Contêiner .arweb inválido (tamanho insuficiente)');
  
  const count = dv.getUint16(20, true);
  const tableOff = dv.getUint32(26, true);
  
  for (let i = 0; i < count; i++) {
    const entryOff = tableOff + i * 48;
    let name = '';
    for (let k = 0; k < 31 && bytes[entryOff + k] !== 0; k++) {
      name += String.fromCharCode(bytes[entryOff + k]);
    }
    const off = dv.getUint32(entryOff + 32, true);
    const size = dv.getUint32(entryOff + 36, true);
    
    if (name.startsWith('mod/') && name.endsWith('.wasm')) {
      return bytes.subarray(off, off + size);
    }
  }
  throw new Error('Nenhuma seção mod/*.wasm encontrada no contêiner .arweb');
}

// Definições de cada Linguagem e sua respectiva Unidade ARWN (.arweb)
const LANG_DEFINITIONS = {
  c: {
    id: 'c',
    name: 'C (Native WASM)',
    unit: 'c_engine',
    badge: 'ARWN C Container (.arweb)',
    color: '#0284c7',
    icon: '⚡',
    type: 'arweb',
    desc: 'Operações diretas em memória linear com ponteiros brutos e i32.load/store sem bounds checking.',
    code: `// C Language Implementation (1MB Memory Array + Pointers)
#include <stdint.h>
static uint32_t memory_buffer[262144]; // 1MB Memory Array

uint32_t compute(uint32_t iters) {
    uint32_t acc = 0x12345678;
    for (uint32_t i = 0; i < iters; i++) {
        uint32_t idx = i & 0x3FFFF;
        uint32_t val = memory_buffer[idx];
        acc = ((acc ^ (i + 0x9E3779B9) ^ val) * 1664525) + 1013904223;
        memory_buffer[idx] = acc;
    }
    return acc;
}`
  },
  cpp: {
    id: 'cpp',
    name: 'C++ (Clang WASM)',
    unit: 'cpp_engine',
    badge: 'ARWN C++ Container (.arweb)',
    color: '#3b82f6',
    icon: '⚙️',
    type: 'arweb',
    desc: 'Buffer contíguo C++20 com acesso vetorial e sem overhead de abstrações.',
    code: `// C++20 Implementation (std::array 1MB Buffer)
#include <cstdint>
#include <array>
static std::array<uint32_t, 262144> memory_buffer{};

extern "C" uint32_t compute(uint32_t iters) noexcept {
    uint32_t acc = 0x12345678U;
    for (uint32_t i = 0; i < iters; ++i) {
        uint32_t idx = i & 0x3FFFFU;
        uint32_t val = memory_buffer[idx];
        acc = ((acc ^ (i + 0x9E3779B9U) ^ val) * 1664525U) + 1013904223U;
        memory_buffer[idx] = acc;
    }
    return acc;
}`
  },
  go: {
    id: 'go',
    name: 'Go (GOOS=js GOARCH=wasm)',
    unit: 'go_engine',
    badge: 'ARWN Go Container (.arweb)',
    color: '#06b6d4',
    icon: '🐹',
    type: 'arweb',
    desc: 'Contêiner binário go_engine.arweb com slice de 1MB e acesso a ponteiros.',
    code: `// Go Implementation (1MB Memory Slice)
package main
var memoryBuffer = make([]uint32, 262144)

func compute(iters uint32) uint32 {
    var acc uint32 = 0x12345678
    for i := uint32(0); i < iters; i++ {
        idx := i & 0x3FFFF
        val := memoryBuffer[idx]
        acc = ((acc ^ (i + 0x9E3779B9) ^ val) * 1664525) + 1013904223
        memoryBuffer[idx] = acc
    }
    return acc
}`
  },
  rust: {
    id: 'rust',
    name: 'Rust (wasm32-unknown-unknown)',
    unit: 'rust_engine',
    badge: 'ARWN Rust Container (.arweb)',
    color: '#f97316',
    icon: '🦀',
    type: 'arweb',
    desc: 'Rust com get_unchecked_mut e aritmética wrapping em memória linear nativa.',
    code: `// Rust Implementation (Zero-Cost Memory Array)
static mut MEMORY_BUFFER: [u32; 262144] = [0; 262144];

#[no_mangle]
pub extern "C" fn compute(iters: u32) -> u32 {
    let mut acc: u32 = 0x12345678;
    unsafe {
        for i in 0..iters {
            let idx = (i & 0x3FFFF) as usize;
            let val = *MEMORY_BUFFER.get_unchecked(idx);
            let term = (acc ^ i.wrapping_add(0x9E3779B9) ^ val).wrapping_mul(1664525);
            acc = term.wrapping_add(1013904223);
            *MEMORY_BUFFER.get_unchecked_mut(idx) = acc;
        }
    }
    acc
}`
  },
  javascript: {
    id: 'javascript',
    name: 'JavaScript (V8 / SpiderMonkey)',
    unit: null,
    badge: 'Uint32Array TypedArray',
    color: '#eab308',
    icon: '📜',
    type: 'js',
    desc: 'JavaScript com Uint32Array (1MB), sujeito ao overhead de GC e checagem de limites.',
    code: `// JavaScript Implementation (Uint32Array Buffer)
const memoryBuffer = new Uint32Array(262144);

function computeJS(iters) {
    let acc = 0x12345678;
    for (let i = 0; i < iters; i++) {
        const idx = i & 0x3FFFF;
        const val = memoryBuffer[idx];
        acc = (Math.imul((acc ^ ((i + 0x9E3779B9) | 0) ^ val), 1664525) + 1013904223) | 0;
        memoryBuffer[idx] = acc;
    }
    return acc >>> 0;
}`
  }
};

const ITERATIONS_DEFAULT = 250000000; // 250M iterações com Array de 1MB (~1.0s)

// Buffer de memória global para execução do JavaScript
const jsMemoryBuffer = new Uint32Array(262144);

// Cache de módulos WASM carregados dos respectivos .arweb
const unitInstances = {};

async function getUnitInstance(unitName) {
  if (unitInstances[unitName]) return unitInstances[unitName];
  
  // Faz o fetch do arquivo .arweb
  const resp = await fetch('/' + unitName + '.arweb');
  if (!resp.ok) throw new Error('Falha ao baixar /' + unitName + '.arweb: HTTP ' + resp.status);
  const arwebBuf = await resp.arrayBuffer();
  
  // Extrai a seção mod/*.wasm
  const wasmBytes = unpackArwebWasm(arwebBuf);
  
  // Instancia o módulo WebAssembly
  const mod = await WebAssembly.instantiate(wasmBytes);
  unitInstances[unitName] = mod.instance;
  return mod.instance;
}

// Componente Principal React
function BenchmarkApp() {
  const [selectedLang, setSelectedLang] = React.useState('c');
  const [iterations, setIterations] = React.useState(ITERATIONS_DEFAULT);
  const [isRunning, setIsRunning] = React.useState(false);
  const [results, setResults] = React.useState({});
  const [currentResult, setCurrentResult] = React.useState(null);

  const runBenchmark = async (langKey = selectedLang) => {
    setIsRunning(true);
    setCurrentResult(null);

    // Permitir atualização do DOM antes de travar no loop matemático
    await new Promise(r => setTimeout(r, 60));

    try {
      const iters = Number(iterations);
      const lang = LANG_DEFINITIONS[langKey];
      let checksum = 0;
      let elapsedMs = 0;

      if (lang.type === 'arweb') {
        // Carrega o contêiner .arweb correspondente à linguagem
        const inst = await getUnitInstance(lang.unit);
        const computeFn = inst.exports.compute || inst.exports._arwn_main || inst.exports._compute;
        if (typeof computeFn !== 'function') {
          throw new Error('Função compute não encontrada no contêiner ' + lang.unit + '.arweb');
        }

        const t0 = performance.now();
        checksum = computeFn(iters) >>> 0;
        const t1 = performance.now();
        elapsedMs = t1 - t0;
      } else {
        // JavaScript com Uint32Array Buffer
        const buf = jsMemoryBuffer;
        buf.fill(0);
        const t0 = performance.now();
        let acc = 0x12345678;
        for (let i = 0; i < iters; i++) {
          const idx = i & 0x3FFFF;
          const val = buf[idx];
          acc = (Math.imul((acc ^ ((i + 0x9E3779B9) | 0) ^ val), 1664525) + 1013904223) | 0;
          buf[idx] = acc;
        }
        const t1 = performance.now();
        checksum = acc >>> 0;
        elapsedMs = t1 - t0;
      }

      const elapsedSec = (elapsedMs / 1000).toFixed(3);
      const opsPerSec = ((iters / (elapsedMs / 1000)) / 1000000).toFixed(1);

      const resObj = {
        lang: langKey,
        langName: lang.name,
        unit: lang.unit ? lang.unit + '.arweb' : 'JIT TypedArray',
        timeMs: elapsedMs.toFixed(2),
        timeSec: elapsedSec,
        opsPerSec: opsPerSec,
        checksum: '0x' + checksum.toString(16).toUpperCase(),
        timestamp: new Date().toLocaleTimeString()
      };

      setCurrentResult(resObj);
      setResults(prev => ({ ...prev, [langKey]: resObj }));

      if (window.ARWN && ARWN.emit) {
        ARWN.emit('benchmark_complete', resObj);
      }
    } catch (err) {
      console.error(err);
      alert('Erro durante o benchmark: ' + err.message);
    } finally {
      setIsRunning(false);
    }
  };

  const runAll = async () => {
    for (const key of Object.keys(LANG_DEFINITIONS)) {
      setSelectedLang(key);
      await runBenchmark(key);
      await new Promise(r => setTimeout(r, 120));
    }
  };

  const currentDef = LANG_DEFINITIONS[selectedLang];

  return React.createElement('div', { style: { display: 'flex', flexDirection: 'column', gap: '2rem' } },
    // Header
    React.createElement('div', {
      className: 'glass-card',
      style: { display: 'flex', justifyContent: 'space-between', alignItems: 'center', flexWrap: 'wrap', gap: '1rem' }
    },
      React.createElement('div', null,
        React.createElement('div', { style: { display: 'flex', alignItems: 'center', gap: '10px' } },
          React.createElement('h1', { style: { fontSize: '1.75rem', fontWeight: '800', background: 'linear-gradient(135deg, #38bdf8, #818cf8)', WebkitBackgroundClip: 'text', WebkitTextFillColor: 'transparent' } }, 'ALRIOS ARWN'),
          React.createElement('span', { className: 'badge badge-primary' }, 'Linear Memory (.arweb) Benchmark')
        ),
        React.createElement('p', { style: { color: 'var(--text-muted)', marginTop: '0.4rem', fontSize: '0.95rem' } },
          'Benchmark intensivo de 1MB Linear Buffer (Ponteiros Diretos em WASM vs TypedArray em JS) com contêineres .arweb individuais.'
        )
      ),
      React.createElement('div', { style: { display: 'flex', gap: '0.75rem' } },
        React.createElement('span', { className: 'badge badge-success' },
          React.createElement('span', { className: 'badge-dot' }),
          'ARWS Gateway Online'
        )
      )
    ),

    // Painel Central de Controle e Seleção
    React.createElement('div', {
      style: { display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(320px, 1fr))', gap: '1.5rem' }
    },
      // Card da Linguagem Selecionada
      React.createElement('div', { className: 'glass-card', style: { display: 'flex', flexDirection: 'column', gap: '1.25rem' } },
        React.createElement('div', { style: { display: 'flex', justifyContent: 'space-between', alignItems: 'center' } },
          React.createElement('h2', { style: { fontSize: '1.2rem', fontWeight: '600' } }, '1. Selecione a Linguagem / .arweb'),
          React.createElement('span', {
            className: 'badge',
            style: { background: currentDef.color + '22', color: currentDef.color, border: '1px solid ' + currentDef.color + '44' }
          }, currentDef.badge)
        ),

        // Select Box estilizado
        React.createElement('select', {
          value: selectedLang,
          onChange: (e) => setSelectedLang(e.target.value),
          disabled: isRunning,
          style: {
            padding: '0.85rem 1rem',
            borderRadius: '12px',
            background: '#090d16',
            color: '#f8fafc',
            border: '1px solid var(--card-border-hover)',
            fontSize: '1.05rem',
            fontWeight: '600',
            cursor: 'pointer'
          }
        },
          Object.values(LANG_DEFINITIONS).map(l =>
            React.createElement('option', { key: l.id, value: l.id }, `${l.icon}  ${l.name} [${l.unit ? l.unit + '.arweb' : 'JS TypedArray'}]`)
          )
        ),

        React.createElement('p', { style: { color: 'var(--text-muted)', fontSize: '0.9rem', lineHeight: '1.4' } },
          currentDef.desc
        ),

        // Controle de Iterações
        React.createElement('div', { style: { marginTop: '0.5rem' } },
          React.createElement('div', { style: { display: 'flex', justifyContent: 'space-between', marginBottom: '0.4rem', fontSize: '0.85rem', color: 'var(--text-muted)' } },
            React.createElement('span', null, 'Operações de Memória (Iterações)'),
            React.createElement('span', { style: { fontWeight: '600', color: 'var(--primary)' } }, (iterations / 1000000) + ' Milhões')
          ),
          React.createElement('input', {
            type: 'range',
            min: '50000000',
            max: '500000000',
            step: '25000000',
            value: iterations,
            disabled: isRunning,
            onChange: (e) => setIterations(Number(e.target.value)),
            style: { width: '100%', accentColor: 'var(--primary)', cursor: 'pointer' }
          })
        ),

        // Botão de Executar
        React.createElement('div', { style: { display: 'flex', gap: '0.75rem', marginTop: '0.5rem' } },
          React.createElement('button', {
            onClick: () => runBenchmark(selectedLang),
            disabled: isRunning,
            className: isRunning ? 'pulsing' : '',
            style: {
              flex: 1,
              padding: '1rem',
              borderRadius: '12px',
              border: 'none',
              background: 'linear-gradient(135deg, #0284c7, #0369a1)',
              color: '#ffffff',
              fontSize: '1rem',
              fontWeight: '700',
              cursor: isRunning ? 'not-allowed' : 'pointer',
              boxShadow: '0 10px 20px -5px rgba(2, 132, 199, 0.5)'
            }
          }, isRunning ? '⚡ Executando em Memória Linear...' : `▶ Executar em ${currentDef.name.split(' ')[0]}`),

          React.createElement('button', {
            onClick: runAll,
            disabled: isRunning,
            style: {
              padding: '1rem 1.25rem',
              borderRadius: '12px',
              border: '1px solid var(--card-border)',
              background: '#1e293b',
              color: '#f8fafc',
              fontSize: '0.9rem',
              fontWeight: '600',
              cursor: isRunning ? 'not-allowed' : 'pointer'
            }
          }, 'Testar Todas')
        )
      ),

      // Card de Resultado em Tempo Real
      React.createElement('div', { className: 'glass-card', style: { display: 'flex', flexDirection: 'column', gap: '1rem' } },
        React.createElement('h2', { style: { fontSize: '1.2rem', fontWeight: '600' } }, '2. Resultado da Execução'),

        currentResult ? React.createElement('div', {
          style: {
            background: '#020617',
            border: '1px solid var(--card-border)',
            borderRadius: '14px',
            padding: '1.5rem',
            display: 'flex',
            flexDirection: 'column',
            gap: '1rem'
          }
        },
          React.createElement('div', { style: { display: 'flex', justifyContent: 'space-between', alignItems: 'baseline' } },
            React.createElement('span', { style: { color: 'var(--text-muted)', fontSize: '0.9rem' } }, 'Tempo de Execução:'),
            React.createElement('span', {
              style: {
                fontSize: '2rem',
                fontWeight: '800',
                color: Number(currentResult.timeSec) < 1.0 ? '#10b981' : '#38bdf8'
              }
            }, `${currentResult.timeSec}s`,
              React.createElement('span', { style: { fontSize: '0.9rem', color: 'var(--text-muted)', marginLeft: '6px' } }, `(${currentResult.timeMs} ms)`)
            )
          ),
          React.createElement('div', { style: { display: 'flex', justifyContent: 'space-between', borderTop: '1px solid #1e293b', paddingTop: '0.75rem' } },
            React.createElement('span', { style: { color: 'var(--text-muted)', fontSize: '0.85rem' } }, 'Contêiner ARWN:'),
            React.createElement('span', { style: { fontWeight: '600', color: 'var(--secondary)', fontSize: '0.9rem', fontFamily: 'monospace' } }, currentResult.unit)
          ),
          React.createElement('div', { style: { display: 'flex', justifyContent: 'space-between' } },
            React.createElement('span', { style: { color: 'var(--text-muted)', fontSize: '0.85rem' } }, 'Taxa de Throughput:'),
            React.createElement('span', { style: { fontWeight: '600', color: '#f8fafc', fontSize: '0.9rem' } }, `${currentResult.opsPerSec} M op/s`)
          ),
          React.createElement('div', { style: { display: 'flex', justifyContent: 'space-between' } },
            React.createElement('span', { style: { color: 'var(--text-muted)', fontSize: '0.85rem' } }, 'Checksum de Memória:'),
            React.createElement('span', { style: { fontFamily: 'monospace', color: '#38bdf8', fontSize: '0.85rem', fontWeight: '600' } }, currentResult.checksum)
          )
        ) : React.createElement('div', {
          style: {
            background: '#020617',
            border: '1px dashed var(--card-border)',
            borderRadius: '14px',
            padding: '2.5rem 1.5rem',
            textAlign: 'center',
            color: 'var(--text-muted)',
            fontSize: '0.95rem'
          }
        }, isRunning ? 'Processando buffer de memória...' : 'Clique em "Executar" para iniciar a medição.')
      )
    ),

    // Tabela Comparativa de Todas as Linguagens
    React.createElement('div', { className: 'glass-card' },
      React.createElement('h2', { style: { fontSize: '1.2rem', fontWeight: '600', marginBottom: '1.25rem' } }, '3. Tabela Comparativa de Performance'),
      React.createElement('div', { style: { overflowX: 'auto' } },
        React.createElement('table', { style: { width: '100%', borderCollapse: 'collapse', textAlign: 'left' } },
          React.createElement('thead', null,
            React.createElement('tr', { style: { borderBottom: '1px solid var(--card-border)', color: 'var(--text-muted)', fontSize: '0.85rem' } },
              React.createElement('th', { style: { padding: '0.75rem 1rem' } }, 'LINGUAGEM'),
              React.createElement('th', { style: { padding: '0.75rem 1rem' } }, 'CONTÊINER ARWN'),
              React.createElement('th', { style: { padding: '0.75rem 1rem' } }, 'TEMPO (S)'),
              React.createElement('th', { style: { padding: '0.75rem 1rem' } }, 'VELOCIDADE (M OP/S)'),
              React.createElement('th', { style: { padding: '0.75rem 1rem' } }, 'CHECKSUM'),
              React.createElement('th', { style: { padding: '0.75rem 1rem' } }, 'STATUS')
            )
          ),
          React.createElement('tbody', null,
            Object.values(LANG_DEFINITIONS).map(l => {
              const res = results[l.id];
              return React.createElement('tr', {
                key: l.id,
                style: {
                  borderBottom: '1px solid rgba(255, 255, 255, 0.04)',
                  background: selectedLang === l.id ? 'rgba(56, 189, 248, 0.04)' : 'transparent'
                }
              },
                React.createElement('td', { style: { padding: '1rem', fontWeight: '600', color: l.color } }, `${l.icon} ${l.name}`),
                React.createElement('td', { style: { padding: '1rem', color: 'var(--text-muted)', fontSize: '0.85rem', fontFamily: 'monospace' } }, l.unit ? l.unit + '.arweb' : 'app.js TypedArray'),
                React.createElement('td', { style: { padding: '1rem', fontWeight: '700', fontSize: '1.05rem', color: res ? '#10b981' : 'var(--text-muted)' } },
                  res ? `${res.timeSec}s` : '-'
                ),
                React.createElement('td', { style: { padding: '1rem', color: 'var(--text-muted)' } },
                  res ? `${res.opsPerSec} M/s` : '-'
                ),
                React.createElement('td', { style: { padding: '1rem', fontFamily: 'monospace', fontSize: '0.85rem', color: '#38bdf8' } },
                  res ? res.checksum : '-'
                ),
                React.createElement('td', { style: { padding: '1rem' } },
                  res ? React.createElement('span', { className: 'badge badge-success' }, 'Concluído')
                      : React.createElement('span', { className: 'badge', style: { background: '#1e293b', color: 'var(--text-muted)' } }, 'Aguardando')
                )
              );
            })
          )
        )
      )
    ),

    // Código Fonte do Algoritmo na Linguagem Ativa
    React.createElement('div', { className: 'glass-card' },
      React.createElement('div', { style: { display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '1rem' } },
        React.createElement('h2', { style: { fontSize: '1.2rem', fontWeight: '600' } }, `4. Código Fonte com Memória Linear (${currentDef.name})`),
        React.createElement('span', { style: { fontSize: '0.85rem', color: 'var(--text-muted)' } }, currentDef.unit ? `${currentDef.unit}.arweb` : 'Inline TypedArray')
      ),
      React.createElement('pre', {
        style: {
          background: '#020617',
          border: '1px solid var(--card-border)',
          borderRadius: '12px',
          padding: '1.25rem',
          color: '#38bdf8',
          fontSize: '0.9rem',
          lineHeight: '1.5',
          overflowX: 'auto'
        }
      },
        React.createElement('code', null, currentDef.code)
      )
    )
  );
}

// Inicialização com React 18 e ARWN Bridge
function initApp() {
  const rootEl = document.getElementById('root');
  if (window.ReactDOM && rootEl) {
    const root = ReactDOM.createRoot(rootEl);
    root.render(React.createElement(BenchmarkApp));
    console.log('[ARWN] React 18 Multi-Unit Benchmark App montado com sucesso!');
  } else {
    setTimeout(initApp, 50);
  }
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', initApp);
} else {
  initApp();
}
