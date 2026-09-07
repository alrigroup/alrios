/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "win32_gui.h"

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#pragma comment(lib, "comctl32.lib")

#define IDC_DEST_EDIT    101
#define IDC_BROWSE_BTN   102
#define IDC_CHK_PATH     103
#define IDC_CHK_GCC      104
#define IDC_CHK_NODE     105
#define IDC_CHK_PYTHON   106
#define IDC_PROGRESS     107
#define IDC_STATUS_LBL   108
#define IDC_LOG_EDIT     109
#define IDC_INSTALL_BTN  110
#define IDC_EXIT_BTN     111

typedef struct {
    installer_config_t *cfg;
    HWND hWnd;
    HWND hDestEdit;
    HWND hChkPath;
    HWND hChkGcc;
    HWND hChkNode;
    HWND hChkPython;
    HWND hProgress;
    HWND hStatusLbl;
    HWND hLogEdit;
    HWND hInstallBtn;
} win32_gui_t;

static win32_gui_t g_win_gui;

static void win_pump_events(void) {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static void win_on_log(const char *msg, void *user_data) {
    (void)user_data;
    if (!g_win_gui.hLogEdit) return;
    int len = GetWindowTextLengthA(g_win_gui.hLogEdit);
    SendMessageA(g_win_gui.hLogEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageA(g_win_gui.hLogEdit, EM_REPLACESEL, FALSE, (LPARAM)msg);
    win_pump_events();
}

static void win_on_progress(int step, int total, int pct, const char *name, void *user_data) {
    (void)user_data;
    if (g_win_gui.hProgress) {
        SendMessage(g_win_gui.hProgress, PBM_SETPOS, (WPARAM)pct, 0);
    }
    if (g_win_gui.hStatusLbl) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[%d/%d] %s (%d%%)", step, total, name, pct);
        SetWindowTextA(g_win_gui.hStatusLbl, buf);
    }
    win_pump_events();
}

static void win_on_complete(int success, const char *msg, void *user_data) {
    (void)user_data;
    if (g_win_gui.hStatusLbl) {
        SetWindowTextA(g_win_gui.hStatusLbl, msg);
    }
    MessageBoxA(g_win_gui.hWnd, msg, success ? "Sucesso" : "Erro", success ? MB_ICONINFORMATION : MB_ICONERROR);
}

static void on_browse(HWND hWnd) {
    BROWSEINFOA bi = {0};
    bi.hwndOwner = hWnd;
    bi.lpszTitle = "Selecione a Pasta de Instalacao do ALRIOS";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        char path[MAX_PATH];
        if (SHGetPathFromIDListA(pidl, path)) {
            SetWindowTextA(g_win_gui.hDestEdit, path);
        }
        CoTaskMemFree(pidl);
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDC_BROWSE_BTN) {
                on_browse(hWnd);
            } else if (wmId == IDC_INSTALL_BTN) {
                EnableWindow(g_win_gui.hInstallBtn, FALSE);

                char dest[MAX_PATH];
                GetWindowTextA(g_win_gui.hDestEdit, dest, sizeof(dest));
                if (dest[0]) {
                    strncpy(g_win_gui.cfg->dest_dir, dest, sizeof(g_win_gui.cfg->dest_dir) - 1);
                }
                g_win_gui.cfg->add_to_path = (SendMessage(g_win_gui.hChkPath, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_win_gui.cfg->install_gcc = (SendMessage(g_win_gui.hChkGcc, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_win_gui.cfg->install_node = (SendMessage(g_win_gui.hChkNode, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_win_gui.cfg->install_python = (SendMessage(g_win_gui.hChkPython, BM_GETCHECK, 0, 0) == BST_CHECKED);

                installer_callbacks_t cb = {
                    .on_log = win_on_log,
                    .on_progress = win_on_progress,
                    .on_complete = win_on_complete
                };

                installer_run(g_win_gui.cfg, &cb, NULL);
                EnableWindow(g_win_gui.hInstallBtn, TRUE);
            } else if (wmId == IDC_EXIT_BTN) {
                PostQuitMessage(0);
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int run_win32_installer(installer_config_t *cfg) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    g_win_gui.cfg = cfg;
    if (!cfg->dest_dir[0]) {
        installer_get_default_paths(cfg->dest_dir, sizeof(cfg->dest_dir), NULL);
    }

    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "ALRIOSInstallerWndClass";
    RegisterClassExA(&wc);

    HWND hWnd = CreateWindowExA(
        0, "ALRIOSInstallerWndClass", "ALRIOS — Instalador Oficial",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 580, 500,
        NULL, NULL, hInstance, NULL
    );
    if (!hWnd) return -1;
    g_win_gui.hWnd = hWnd;

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    /* Banner / Titulo */
    HWND hTitle = CreateWindowA("STATIC", "ALRIOS Sovereign Operating System", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 15, 520, 24, hWnd, NULL, hInstance, NULL);
    SendMessage(hTitle, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hSub = CreateWindowA("STATIC", "Instalador Oficial e Configuracao de Ambiente Soberano", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 38, 520, 20, hWnd, NULL, hInstance, NULL);
    SendMessage(hSub, WM_SETFONT, (WPARAM)hFont, TRUE);

    /* Destino */
    HWND hDestLbl = CreateWindowA("STATIC", "Pasta de Instalacao:", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 70, 520, 18, hWnd, NULL, hInstance, NULL);
    SendMessage(hDestLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

    g_win_gui.hDestEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", cfg->dest_dir, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 20, 90, 420, 24, hWnd, (HMENU)IDC_DEST_EDIT, hInstance, NULL);
    SendMessage(g_win_gui.hDestEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hBrowse = CreateWindowA("BUTTON", "Procurar...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 450, 89, 90, 26, hWnd, (HMENU)IDC_BROWSE_BTN, hInstance, NULL);
    SendMessage(hBrowse, WM_SETFONT, (WPARAM)hFont, TRUE);

    /* Checkboxes */
    g_win_gui.hChkPath = CreateWindowA("BUTTON", "Registrar comandos globais no PATH do Windows (alrios, arpm, arcore, armake)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 130, 520, 22, hWnd, (HMENU)IDC_CHK_PATH, hInstance, NULL);
    SendMessage(g_win_gui.hChkPath, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(g_win_gui.hChkPath, BM_SETCHECK, cfg->add_to_path ? BST_CHECKED : BST_UNCHECKED, 0);

    g_win_gui.hChkGcc = CreateWindowA("BUTTON", "Auto-instalar Compilador GCC C/C++ portatil (w64devkit MinGW)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 155, 520, 22, hWnd, (HMENU)IDC_CHK_GCC, hInstance, NULL);
    SendMessage(g_win_gui.hChkGcc, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(g_win_gui.hChkGcc, BM_SETCHECK, cfg->install_gcc ? BST_CHECKED : BST_UNCHECKED, 0);

    g_win_gui.hChkNode = CreateWindowA("BUTTON", "Auto-instalar Runtime Node.js portatil", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 180, 520, 22, hWnd, (HMENU)IDC_CHK_NODE, hInstance, NULL);
    SendMessage(g_win_gui.hChkNode, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(g_win_gui.hChkNode, BM_SETCHECK, cfg->install_node ? BST_CHECKED : BST_UNCHECKED, 0);

    g_win_gui.hChkPython = CreateWindowA("BUTTON", "Auto-instalar Runtime Python portatil", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 205, 520, 22, hWnd, (HMENU)IDC_CHK_PYTHON, hInstance, NULL);
    SendMessage(g_win_gui.hChkPython, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(g_win_gui.hChkPython, BM_SETCHECK, cfg->install_python ? BST_CHECKED : BST_UNCHECKED, 0);

    /* Status e Progresso */
    g_win_gui.hStatusLbl = CreateWindowA("STATIC", "Pronto para iniciar.", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 240, 520, 18, hWnd, (HMENU)IDC_STATUS_LBL, hInstance, NULL);
    SendMessage(g_win_gui.hStatusLbl, WM_SETFONT, (WPARAM)hFont, TRUE);

    g_win_gui.hProgress = CreateWindowExA(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 20, 260, 520, 20, hWnd, (HMENU)IDC_PROGRESS, hInstance, NULL);
    SendMessage(g_win_gui.hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

    /* Log Box */
    g_win_gui.hLogEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 20, 290, 520, 110, hWnd, (HMENU)IDC_LOG_EDIT, hInstance, NULL);
    SendMessage(g_win_gui.hLogEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    /* Botoes */
    g_win_gui.hInstallBtn = CreateWindowA("BUTTON", "Instalar ALRIOS", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 330, 415, 120, 30, hWnd, (HMENU)IDC_INSTALL_BTN, hInstance, NULL);
    SendMessage(g_win_gui.hInstallBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hExitBtn = CreateWindowA("BUTTON", "Fechar", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 460, 415, 80, 30, hWnd, (HMENU)IDC_EXIT_BTN, hInstance, NULL);
    SendMessage(hExitBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

#else
int run_win32_installer(installer_config_t *cfg) {
    (void)cfg;
    return -1;
}
#endif
