#pragma once
#include <Windows.h>
#include <LazyDlls/Lazyimporter.hpp>
#include <xorstr.h>

extern "C" HINSTANCE WINAPI ShellExecuteW(
    HWND    hwnd,
    LPCWSTR lpOperation,
    LPCWSTR lpFile,
    LPCWSTR lpParameters,
    LPCWSTR lpDirectory,
    INT     nShowCmd
);

namespace Url {

    inline void Open(const wchar_t* url) {
        LI_CACHED(LoadLibraryW)(xorstr_w(L"shell32.dll"));
        LI_CACHED(ShellExecuteW)(nullptr, xorstr_w(L"open"), url, nullptr, nullptr, 1);
    }

}
