/*
 * arwn-bridge.js — Loader/Bridge padrão injetado em todo .arweb.
 * Modelo híbrido: wasm chama helpers ARWN.* (DOM/CSS/eventos) e JS chama
 * exports do wasm (ARWN.modules.<unit>.<fn>). Sem eval/new Function.
 *
 * Parse binário do .arweb (layout idêntico ao arwn_pack.c):
 *   header 48B: magic[16] + ver u16 + flags u16 + count u16 +
 *               header_crc u32 (crc dos primeiros 22 bytes) +
 *               table_off u32 + payload_off u32 + payload_size u32 +
 *               reserved u32
 *   entries 48B cada: name[32] + offset u32 + size u32 + crc u32 +
 *               compressed u8 + reserved[3]
 *   payload: bytes das seções em ordem
 *
 * (C) ALRIGROUP — ARGLR license.
 */
(function (global) {
  'use strict';

  /* --- CRC32 (ISO-HDLC, polinômio 0xEDB88320) — igual ao C --- */
  var CRC_TABLE = (function () {
    var t = new Uint32Array(256);
    for (var i = 0; i < 256; i++) {
      var c = i;
      for (var k = 0; k < 8; k++) {
        c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
      }
      t[i] = c >>> 0;
    }
    return t;
  })();

  function crc32(bytes, off, len) {
    var c = 0xFFFFFFFF;
    for (var i = off; i < off + len; i++) {
      c = (CRC_TABLE[(c ^ bytes[i]) & 0xFF] ^ (c >>> 8)) >>> 0;
    }
    return (c ^ 0xFFFFFFFF) >>> 0;
  }

  var MAGIC = 'ALRIGROUP@ARWEB';
  var HDR = 48, ENTRY = 48, NAME_MAX = 31;
  var u16 = function (dv, o) { return dv.getUint16(o, true); };
  var u32 = function (dv, o) { return dv.getUint32(o, true); };

  /* Parse do .arweb (ArrayBuffer/TypedArray). Lança Error se inválido.
     Devolve { sections: { nome: Uint8Array } }. */
  function parseArweb(buf) {
    var bytes = new Uint8Array(buf);
    var dv = new DataView(buf);
    if (bytes.length < HDR) throw new Error('arweb: too short');
    for (var i = 0; i < 16; i++) {
      if (String.fromCharCode(bytes[i]) !== MAGIC[i]) throw new Error('arweb: bad magic');
    }
    if (u16(dv, 16) !== 1) throw new Error('arweb: unsupported version');
    if (crc32(bytes, 0, 22) !== u32(dv, 22)) throw new Error('arweb: header crc mismatch');

    var count = u16(dv, 20);
    var tableOff = u32(dv, 26);
    var payloadOff = u32(dv, 30);
    if (tableOff !== HDR) throw new Error('arweb: bad table offset');

    var sections = {};
    var prevEnd = payloadOff;
    for (var idx = 0; idx < count; idx++) {
      var e = tableOff + idx * ENTRY;
      var name = '';
      for (var k = 0; k < NAME_MAX && bytes[e + k] !== 0; k++) {
        name += String.fromCharCode(bytes[e + k]);
      }
      var off = u32(dv, e + 32);
      var size = u32(dv, e + 36);
      var crc = u32(dv, e + 40);
      if (off + size > bytes.length) throw new Error('arweb: section out of bounds: ' + name);
      if (off < prevEnd) throw new Error('arweb: section overlap: ' + name);
      prevEnd = off + size;
      if (crc32(bytes, off, size) !== crc) throw new Error('arweb: crc mismatch: ' + name);
      sections[name] = bytes.subarray(off, off + size);
    }
    return { sections: sections };
  }

  /* primeiro módulo mod/*.wasm da tabela */
  function wasmSection(sections) {
    for (var n in sections) {
      if (sections.hasOwnProperty(n) &&
          n.lastIndexOf('mod/', 0) === 0 && n.slice(-5) === '.wasm') {
        return sections[n];
      }
    }
    return null;
  }

  var units = {};   /* name -> { module, instance, exports, imports, sections } */
  var listeners = {};

  var ARWN = {
    version: '0.3.0',
    _parse: parseArweb,
    modules: {},

    /* Busca e valida o .arweb da unit sob demanda (CRC verificado). */
    async load(unit, imports) {
      if (units[unit] && units[unit].module) return units[unit];
      var res = await fetch('/' + unit + '.arweb');
      if (!res.ok) throw new Error('arweb not found: ' + unit);
      var buf = await res.arrayBuffer();
      var parsed = parseArweb(buf);
      var wasm = wasmSection(parsed.sections);
      if (!wasm) throw new Error('no mod/*.wasm section in ' + unit);
      var entry = {
        sections: parsed.sections,
        imports: imports || {},
        module: null,
        instance: null,
        exports: null
      };
      units[unit] = entry;
      if (!ARWN.modules[unit]) ARWN.modules[unit] = {};
      return entry;
    },

    /* Compila/instancia lazy na 1ª call; reusa Module (compila 1x). */
    async _ensure(unit) {
      var e = units[unit];
      if (!e) e = await this.load(unit);
      if (!e.module) {
        var wasm = wasmSection(e.sections);
        e.module = await WebAssembly.compile(wasm);
      }
      if (!e.instance) {
        e.instance = await WebAssembly.instantiate(e.module, e.imports);
        e.exports = e.instance.exports;
        var self = this;
        Object.keys(e.exports).forEach(function (k) {
          if (typeof e.exports[k] === 'function') {
            self.modules[unit][k] = e.exports[k].bind(e.exports);
          }
        });
      }
      return e;
    },

    /* Chama arwn_main da unit (instancia na 1ª chamada). */
    async call(name, payload) {
      var e = await this._ensure(name);
      var fn = e.exports['arwn_main'];
      if (!fn) throw new Error('export arwn_main missing: ' + name);
      return fn(payload);
    },

    /* helpers DOM para o wasm (wasm não toca DOM direto) */
    dom: {
      set: function (sel, value) {
        var el = typeof sel === 'string' ? document.querySelector(sel) : sel;
        if (el) el.textContent = String(value);
      },
      html: function (sel, value) {
        var el = typeof sel === 'string' ? document.querySelector(sel) : sel;
        if (el) el.innerHTML = String(value);
      },
      css: function (sel, prop, value) {
        var el = typeof sel === 'string' ? document.querySelector(sel) : sel;
        if (el) el.style[prop] = value;
      },
      get: function (sel) {
        var el = typeof sel === 'string' ? document.querySelector(sel) : sel;
        return el ? el.textContent : null;
      }
    },

    on: function (event, cb) {
      (listeners[event] = listeners[event] || []).push(cb);
    },

    emit: function (name, data) {
      var cbs = listeners[name] || [];
      for (var i = 0; i < cbs.length; i++) cbs[i](data);
      if (typeof CustomEvent !== 'undefined') {
        var ev = new CustomEvent('arwn:' + name, { detail: data });
        document.dispatchEvent(ev);
      }
    },

    ready: function (cb) {
      if (typeof document !== 'undefined' &&
          document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', function () { cb(ARWN); });
      } else {
        cb(ARWN);
      }
    },

    instantiate: function (name, imports) {
      return ARWN.load(name, imports).then(function (e) { return e; });
    }
  };

  global.ARWN = ARWN;
})(typeof window !== 'undefined' ? window : globalThis);