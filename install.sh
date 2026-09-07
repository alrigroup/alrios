#!/usr/bin/env bash
# ==============================================================================
# ALRIOS — Sovereign Operating System & Microkernel Ecosystem
# Official Automated Installer Bootstrap for Linux (x86_64)
# ==============================================================================
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "========================================================================"
echo "          ALRIOS — Instalador Automatizado para Linux                    "
echo "========================================================================"

# 1. Ajustar permissoes de execucao nos binarios
echo "-> Configurando permissoes de execucao..."
chmod +x "${DIR}/arcore/arinstall" 2>/dev/null || true
chmod +x "${DIR}/arcore/alrios" 2>/dev/null || true
chmod +x "${DIR}/arcore/arcore" 2>/dev/null || true
chmod +x "${DIR}/arcore/armake" 2>/dev/null || true
chmod +x "${DIR}/run.sh" "${DIR}/stop.sh" 2>/dev/null || true

# 2. Garantir links simbolicos
echo "-> Verificando links do ecossistema..."
ln -sf arcore/alrios "${DIR}/alrios" 2>/dev/null || true
ln -sf alrios "${DIR}/arcore/arpm" 2>/dev/null || true
chmod +x "${DIR}/alrios" "${DIR}/arcore/arpm" 2>/dev/null || true

# 3. Executar o arinstall nativo se disponivel
if [ -x "${DIR}/arcore/arinstall" ]; then
    echo "-> Iniciando instalador nativo arinstall..."
    exec "${DIR}/arcore/arinstall" "$@"
else
    echo "✓ ALRIOS pronto para uso!"
    echo "Para inicializar os servicos:"
    echo "   ./alrios power on"
    echo "Para listar pacotes disponiveis:"
    echo "   alrios arpm list"
fi
