set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(OPENSSL_ROOT_DIR /opt/mingw64-openssl)
set(OPENSSL_INCLUDE_DIR /opt/mingw64-openssl/include)
set(OPENSSL_CRYPTO_LIBRARY /opt/mingw64-openssl/lib64/libcrypto.a)
set(OPENSSL_SSL_LIBRARY /opt/mingw64-openssl/lib64/libssl.a)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32 /opt/mingw64-openssl)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)

set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -static-libgcc")
