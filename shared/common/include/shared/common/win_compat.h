#pragma once

// Windows compatibility header: ensures Winsock2 is included before Windows.h
// and defines NOMINMAX to avoid min/max macro conflicts. Must be included
// before any header that might transitively include <windows.h> or <asio>.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
// Prevent std::byte vs Windows byte (rpcndr.h) ambiguity when using
// `using namespace std;` in translation units that ultimately include
// Windows headers. MSVC defines std::byte when _HAS_STD_BYTE is 1.
#ifndef _HAS_STD_BYTE
#define _HAS_STD_BYTE 0
#endif
// asio (via restinio) requires Winsock2 before Windows.h
#include <winsock2.h>
#include <BaseTsd.h>
#include <windows.h>
// Windows headers define DrawText as DrawTextA/W via macro, which collides
// with rgb_matrix::DrawText. Remove the macro so the C++ API is visible.
#ifdef DrawText
#undef DrawText
#endif
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
#ifdef CreateWindow
#undef CreateWindow
#endif
#ifdef GetObject
#undef GetObject
#endif
#endif
