#!/usr/bin/env bash
# ==============================================================================
# ALRIOS — Sovereign Operating System & Microkernel Ecosystem
# Official Automated Installer & Environment Setup for Linux (x86_64)
# ==============================================================================
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "========================================================================"
echo "          ALRIOS — Instalador Automatizado para Linux                    "
echo "========================================================================"

# 1. Ajustar permissões de execução nos binários
echo "-> [1/4] Configurando permissões de execução..."
chmod +x "${DIR}/arcore/arinstall" 2>/dev/null || true
chmod +x "${DIR}/arcore/alrios" 2>/dev/null || true
chmod +x "${DIR}/arcore/arcore" 2>/dev/null || true
chmod +x "${DIR}/arcore/armake" 2>/dev/null || true
chmod +x "${DIR}/run.sh" "${DIR}/stop.sh" 2>/dev/null || true

# 2. Garantir links locais do ecossistema
echo "-> [2/4] Verificando links internos do ecossistema..."
ln -sf arcore/alrios "${DIR}/alrios" 2>/dev/null || true
ln -sf alrios "${DIR}/arcore/arpm" 2>/dev/null || true
chmod +x "${DIR}/alrios" "${DIR}/arcore/arpm" 2>/dev/null || true

# 3. Registrar no PATH do Linux (/usr/local/bin ou ~/.local/bin)
echo "-> [3/4] Configurando comandos no PATH do sistema..."
INSTALLED_BIN=""

# Tentar /usr/local/bin se formos root ou se for gravável
if [ "$(id -u)" -eq 0 ] || [ -w "/usr/local/bin" ]; then
    ln -sf "${DIR}/arcore/alrios" "/usr/local/bin/alrios"
    ln -sf "${DIR}/arcore/alrios" "/usr/local/bin/arpm"
    ln -sf "${DIR}/arcore/arcore" "/usr/local/bin/arcore"
    ln -sf "${DIR}/arcore/armake" "/usr/local/bin/armake"
    INSTALLED_BIN="/usr/local/bin"
elif command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
    sudo ln -sf "${DIR}/arcore/alrios" "/usr/local/bin/alrios"
    sudo ln -sf "${DIR}/arcore/alrios" "/usr/local/bin/arpm"
    sudo ln -sf "${DIR}/arcore/arcore" "/usr/local/bin/arcore"
    sudo ln -sf "${DIR}/arcore/armake" "/usr/local/bin/armake"
    INSTALLED_BIN="/usr/local/bin"
else
    # Fallback para ~/.local/bin
    mkdir -p "${HOME}/.local/bin"
    ln -sf "${DIR}/arcore/alrios" "${HOME}/.local/bin/alrios"
    ln -sf "${DIR}/arcore/alrios" "${HOME}/.local/bin/arpm"
    ln -sf "${DIR}/arcore/arcore" "${HOME}/.local/bin/arcore"
    ln -sf "${DIR}/arcore/armake" "${HOME}/.local/bin/armake"
    INSTALLED_BIN="${HOME}/.local/bin"

    # Garantir que ~/.local/bin está no PATH do usuário
    for rc in "${HOME}/.bashrc" "${HOME}/.zshrc" "${HOME}/.profile"; do
        if [ -f "$rc" ] && ! grep -q '\.local/bin' "$rc"; then
            echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$rc"
        fi
    done
fi

echo "   ✓ Comandos registrados em ${INSTALLED_BIN}:"
echo "     - alrios (CLI e orquestrador)"
echo "     - arpm   (Gerenciador de pacotes soberano)"
echo "     - arcore (Kernel do ALRIOS)"
echo "     - armake (Empacotador .arapp)"

# 4. Finalização
echo "-> [4/4] Finalizando instalacao..."
if [ "$1" = "--runtime" ] || [ "$1" = "all" ] && [ -x "${DIR}/arcore/arinstall" ]; then
    echo "-> Iniciando instalador de runtimes arinstall..."
    exec "${DIR}/arcore/arinstall" "$@"
fi

echo ""
echo "========================================================================"
echo " ✓ ALRIOS instalado com sucesso no Linux!"
echo "   Voce ja pode executar de qualquer terminal:"
echo "   $ alrios power on"
echo "   $ arpm list"
echo "   $ arcore --help"
echo "========================================================================"
