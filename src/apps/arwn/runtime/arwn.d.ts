/**
 * ARWN (Alri Real-Time Web Node) Bridge TypeScript Definitions
 * (C) ALRIGROUP — ARGLR license.
 */

export interface AwrnDom {
  set(selector: string | Element, value: any): void;
  html(selector: string | Element, value: any): void;
  css(selector: string | Element, property: string, value: string): void;
  get(selector: string | Element): string | null;
}

export interface AwrnUnitEntry {
  sections: Record<string, Uint8Array>;
  imports: WebAssembly.Imports;
  module: WebAssembly.Module | null;
  instance: WebAssembly.Instance | null;
  exports: Record<string, any> | null;
}

export interface AwrnBridge {
  version: string;
  modules: Record<string, Record<string, (...args: any[]) => any>>;
  dom: AwrnDom;

  load(unit: string, imports?: WebAssembly.Imports): Promise<AwrnUnitEntry>;
  _ensure(unit: string): Promise<AwrnUnitEntry>;
  call(name: string, payload?: any): Promise<any>;
  on(event: string, callback: (data: any) => void): void;
  emit(name: string, data?: any): void;
  ready(callback: (bridge: AwrnBridge) => void): void;
  instantiate(name: string, imports?: WebAssembly.Imports): Promise<AwrnUnitEntry>;
}

declare global {
  interface Window {
    ARWN: AwrnBridge;
  }
  const ARWN: AwrnBridge;
}

export default ARWN;
