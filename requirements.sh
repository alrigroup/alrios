#!/usr/bin/env bash
# Copyright (c) ALRIGROUP and its affiliates.
#
# This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
# found in the LICENSE file in the root directory of this source tree
# and at: https://github.com/alrigroup/licenses/tree/main
set -e

echo ""
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║     ALRIOS/ARCORE — Instalador de Dependencias            ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

# Detecta arquitetura
ARCH=$(uname -m)
if [ "$ARCH" != "x86_64" ]; then
    echo "Arquitetura: $ARCH (suportado: x86_64)"
fi

install_runtime() {
    echo ">>> Instalando dependencias de RUNTIME..."
    if command -v apt-get &>/dev/null; then
        sudo apt-get update
        sudo apt-get install -y libssl3 libzstd1 zlib1g
    elif command -v pacman &>/dev/null; then
        sudo pacman -Sy --noconfirm openssl zstd zlib
    elif command -v dnf &>/dev/null; then
        sudo dnf install -y openssl libzstd zlib
    else
        echo "Aviso: instale manualmente: OpenSSL, Zstd, Zlib"
    fi
}

install_build() {
    echo ">>> Instalando ferramentas de BUILD..."
    if command -v apt-get &>/dev/null; then
        sudo apt-get install -y cmake gcc make libssl-dev golang-go pkg-config
    elif command -v pacman &>/dev/null; then
        sudo pacman -Sy --noconfirm cmake base-devel openssl go
    elif command -v dnf &>/dev/null; then
        sudo dnf install -y cmake gcc make openssl-devel golang pkgconfig
    else
        echo "Aviso: instale manualmente: cmake, gcc, make, openssl-dev, golang"
    fi
}

install_build_gui() {
    echo ">>> Instalando dependencias opcionais (GTK3 para arinstall GUI)..."
    if command -v apt-get &>/dev/null; then
        sudo apt-get install -y libgtk-3-dev
    elif command -v pacman &>/dev/null; then
        sudo pacman -Sy --noconfirm gtk3
    elif command -v dnf &>/dev/null; then
        sudo dnf install -y gtk3-devel
    else
        echo "Aviso: instale manualmente: GTK3 (opcional)"
    fi
}

# ─── Menu interativo ───
echo "Escolha o que instalar:"
echo "  1) Runtime + Build (completo)"
echo "  2) So runtime (rodar arcore em producao)"
echo "  3) So build (compilar arcore)"
echo "  4) Build + GTK3 (compilar arcore + arinstall GUI)"
echo ""
read -rp "Opcao [1]: " opt
opt="${opt:-1}"

case "$opt" in
    2)
        install_runtime
        ;;
    3)
        install_build
        ;;
    4)
        install_build
        install_build_gui
        ;;
    *)
        install_runtime
        install_build
        ;;
esac

echo ""
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║  Pronto! Para compilar: ./build.sh                       ║"
echo "║  Para rodar: ./run.sh                                    ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""
