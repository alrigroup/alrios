/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef AR_RUNTIMES_H
#define AR_RUNTIMES_H

#define AR_RT_MAX_FILES 8

typedef struct {
    const char *name;
    const char *version;
    const char *entry_windows;
    const char *entry_linux;
    const char *files_windows[AR_RT_MAX_FILES];
    int  files_windows_count;
    const char *files_linux[AR_RT_MAX_FILES];
    int  files_linux_count;
    const char *url_windows;
    const char *url_linux;
    int  size_mb;
    int  skip_linux; /* do not install on Linux (use system toolchain) */
} runtime_entry_t;

static const runtime_entry_t available_runtimes[] = {
    {
        .name = "node",
        .version = "22.23.2",
        .entry_windows = "node.exe",
        .entry_linux = "bin/node",
        .files_windows = {"node.exe"},
        .files_windows_count = 1,
        .files_linux = {"bin/node", "lib/"},
        .files_linux_count = 2,
        .url_windows = "https://nodejs.org/dist/v22.23.2/node-v22.23.2-win-x64.zip",
        .url_linux = "https://nodejs.org/dist/v22.23.2/node-v22.23.2-linux-x64.tar.gz",
        .size_mb = 57
    },
    {
        .name = "npm",
        .version = "12.0.2",
        .entry_windows = "bin/npm-cli.js",
        .entry_linux = "bin/npm-cli.js",
        .files_windows = {"bin/", "lib/", "node_modules/", "package.json"},
        .files_windows_count = 4,
        .files_linux = {"bin/", "lib/", "node_modules/", "package.json"},
        .files_linux_count = 4,
        .url_windows = "https://registry.npmjs.org/npm/-/npm-12.0.2.tgz",
        .url_linux = "https://registry.npmjs.org/npm/-/npm-12.0.2.tgz",
        .size_mb = 8
    },
    {
        .name = "python3",
        .version = "3.11.8",
        .entry_windows = "python.exe",
        .entry_linux = "bin/python3",
        .files_windows = {"python.exe", "python3.dll"},
        .files_windows_count = 2,
        .files_linux = {"bin/python3", "lib/"},
        .files_linux_count = 2,
        .url_windows = "https://www.python.org/ftp/python/3.11.5/python-3.11.5-embed-amd64.zip",
        .url_linux = "https://github.com/indygreg/python-build-standalone/releases/download/20240224/cpython-3.11.8%2B20240224-x86_64-unknown-linux-gnu-install_only.tar.gz",
        .size_mb = 25
    },
    {
        .name = "java",
        .version = "17.0.12",
        .entry_windows = "bin/java.exe",
        .entry_linux = "bin/java",
        .files_windows = {"bin/java.exe", "bin/jvm.dll", "lib/"},
        .files_windows_count = 3,
        .files_linux = {"bin/java", "lib/"},
        .files_linux_count = 2,
        .url_windows = "https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.12%2B7/OpenJDK17U-jdk_x64_windows_hotspot_17.0.12_7.zip",
        .url_linux = "https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.12%2B7/OpenJDK17U-jdk_x64_linux_hotspot_17.0.12_7.tar.gz",
        .size_mb = 50
    },
    {
        .name = "lua",
        .version = "5.4.6",
        .entry_windows = "lua54.exe",
        .entry_linux = "bin/lua",
        .files_windows = {"lua54.exe"},
        .files_windows_count = 1,
        .files_linux = {"bin/lua", "lib/"},
        .files_linux_count = 2,
        .url_windows = "https://github.com/rjpcomputing/luaforwindows/releases/download/v5.4.6/LuaForWindows_v5.4.6-x64.zip",
        .url_linux = "https://www.lua.org/ftp/lua-5.4.6.tar.gz",
        .size_mb = 5
    },
    {
        .name = "ruby",
        .version = "3.3.12",
        .entry_windows = "bin/ruby.exe",
        .entry_linux = "bin/ruby",
        .files_windows = {"bin/ruby.exe", "bin/msvcrt-ruby310.dll", "lib/"},
        .files_windows_count = 3,
        .files_linux = {"bin/ruby", "lib/"},
        .files_linux_count = 2,
        .url_windows = "https://github.com/oneclick/rubyinstaller2/releases/download/RubyInstaller-3.1.4-1/rubyinstaller-3.1.4-1-x64.exe",
        .url_linux = "https://github.com/jdx/ruby/releases/download/3.3.12-1/ruby-3.3.12.x86_64_linux.tar.gz",
        .size_mb = 30
    },
    {
        .name = "go",
        .version = "1.21.3",
        .entry_windows = "bin/go.exe",
        .entry_linux = "bin/go",
        .files_windows = {"bin/go.exe", "bin/gofmt.exe", "src/", "pkg/"},
        .files_windows_count = 4,
        .files_linux = {"bin/go", "bin/gofmt", "src/", "pkg/"},
        .files_linux_count = 4,
        .url_windows = "https://go.dev/dl/go1.21.3.windows-amd64.zip",
        .url_linux = "https://redirector.gvt1.com/edgedl/go/go1.21.3.linux-amd64.tar.gz",
        .size_mb = 60
    },
    {
        .name = "c",
        .version = "13.2.0",
        .entry_windows = "bin/gcc.exe",
        .entry_linux = "bin/gcc",
        .files_windows = {"bin/gcc.exe", "bin/", "lib/", "include/", "share/"},
        .files_windows_count = 5,
        .files_linux = {"bin/gcc", "bin/", "lib/", "include/", "share/"},
        .files_linux_count = 5,
        .url_windows = "https://github.com/niXman/mingw-builds-binaries/releases/download/13.2.0-rt_v11-rev1/x86_64-13.2.0-release-win32-seh-ucrt-rt_v11-rev1.7z",
        .url_linux = "https://github.com/niXman/mingw-builds-binaries/releases/download/13.2.0-rt_v11-rev1/x86_64-13.2.0-release-win32-seh-ucrt-rt_v11-rev1.7z",
        .size_mb = 150,
        .skip_linux = 1
    },
    {
        .name = "cpp",
        .version = "13.2.0",
        .entry_windows = "bin/g++.exe",
        .entry_linux = "bin/g++",
        .files_windows = {"bin/g++.exe", "bin/gcc.exe", "bin/", "lib/", "include/", "share/"},
        .files_windows_count = 6,
        .files_linux = {"bin/g++", "bin/gcc", "bin/", "lib/", "include/", "share/"},
        .files_linux_count = 6,
        .url_windows = "https://github.com/niXman/mingw-builds-binaries/releases/download/13.2.0-rt_v11-rev1/x86_64-13.2.0-release-win32-seh-ucrt-rt_v11-rev1.7z",
        .url_linux = "https://github.com/niXman/mingw-builds-binaries/releases/download/13.2.0-rt_v11-rev1/x86_64-13.2.0-release-win32-seh-ucrt-rt_v11-rev1.7z",
        .size_mb = 150,
        .skip_linux = 1
    }
};

#define RUNTIME_COUNT (sizeof(available_runtimes) / sizeof(available_runtimes[0]))

#endif
