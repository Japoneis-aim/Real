#pragma once
#include <Windows.h>
#include <winternl.h>
#include <LazyDlls/Lazyimporter.hpp>
#include <LazyDlls/LazyNtdll.hpp>
#include <xorstr.h>

namespace Credentials {

    inline constexpr ULONG kKeyValuePartialInformation = 2;

    inline const unsigned char kKey[16] = {
        0x5A, 0xC3, 0x91, 0x47, 0xE2, 0x18, 0x76, 0xBE,
        0x0D, 0xA9, 0x52, 0x3F, 0x84, 0xCB, 0x67, 0x1E
    };

    inline void XorBuf(unsigned char* data, ULONG len) {
        for (ULONG i = 0; i < len; ++i)
            data[i] ^= kKey[i & 0x0F];
    }

    inline void InitUString(UNICODE_STRING* us, const wchar_t* src) {
        USHORT n = 0;
        while (src[n]) ++n;
        us->Length = static_cast<USHORT>(n * sizeof(wchar_t));
        us->MaximumLength = static_cast<USHORT>((n + 1) * sizeof(wchar_t));
        us->Buffer = const_cast<PWSTR>(src);
    }

    inline HANDLE OpenAppKey(ACCESS_MASK access, bool createIfMissing) {
        HANDLE hUser = nullptr;
        NTSTATUS s = LI_CACHED(RtlOpenCurrentUser)(MAXIMUM_ALLOWED, &hUser);
        if (s < 0 || !hUser) return nullptr;

        HANDLE hKey = nullptr;
        {
            auto regPath = xorstr_w(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Streams\\Cache");
            UNICODE_STRING usName;
            InitUString(&usName, regPath);

            OBJECT_ATTRIBUTES oa;
            InitializeObjectAttributes(&oa, &usName, OBJ_CASE_INSENSITIVE, hUser, nullptr);

            if (createIfMissing) {
                ULONG disp = 0;
                s = LI_CACHED(NtCreateKey)(&hKey, access, &oa, 0, nullptr, REG_OPTION_NON_VOLATILE, &disp);
            } else {
                s = LI_CACHED(NtOpenKey)(&hKey, access, &oa);
            }
        }

        LI_CACHED(NtClose)(hUser);
        return (s >= 0) ? hKey : nullptr;
    }

    inline bool Load(char* user, int userSize, char* pass, int passSize, bool* remember) {
        if (remember) *remember = false;
        if (user && userSize > 0) user[0] = '\0';
        if (pass && passSize > 0) pass[0] = '\0';

        HANDLE hKey = OpenAppKey(KEY_QUERY_VALUE, false);
        if (!hKey) return false;

        BYTE buf[sizeof(LZN_KEY_VALUE_PARTIAL_INFORMATION) + 64];
        UNICODE_STRING us;
        ULONG ret = 0;

        if (remember) {
            auto sName = xorstr_w(L"NodeSlot");
            InitUString(&us, sName);
            if (LI_CACHED(NtQueryValueKey)(hKey, &us, kKeyValuePartialInformation, buf, sizeof(buf), &ret) >= 0) {
                auto* kvi = reinterpret_cast<LZN_KEY_VALUE_PARTIAL_INFORMATION*>(buf);
                if (kvi->Type == REG_DWORD && kvi->DataLength == sizeof(DWORD)) {
                    DWORD r = *reinterpret_cast<DWORD*>(kvi->Data);
                    *remember = (r != 0);
                }
            }
        }

        if (user && userSize > 0) {
            auto sName = xorstr_w(L"ItemIDList");
            InitUString(&us, sName);
            if (LI_CACHED(NtQueryValueKey)(hKey, &us, kKeyValuePartialInformation, buf, sizeof(buf), &ret) >= 0) {
                auto* kvi = reinterpret_cast<LZN_KEY_VALUE_PARTIAL_INFORMATION*>(buf);
                if (kvi->Type == REG_BINARY && kvi->DataLength > 0) {
                    XorBuf(kvi->Data, kvi->DataLength);
                    int copy = static_cast<int>(kvi->DataLength) < userSize - 1
                             ? static_cast<int>(kvi->DataLength) : userSize - 1;
                    for (int i = 0; i < copy; ++i) user[i] = static_cast<char>(kvi->Data[i]);
                    user[copy] = '\0';
                }
            }
        }

        if (pass && passSize > 0) {
            auto sName = xorstr_w(L"MRUListEx");
            InitUString(&us, sName);
            if (LI_CACHED(NtQueryValueKey)(hKey, &us, kKeyValuePartialInformation, buf, sizeof(buf), &ret) >= 0) {
                auto* kvi = reinterpret_cast<LZN_KEY_VALUE_PARTIAL_INFORMATION*>(buf);
                if (kvi->Type == REG_BINARY && kvi->DataLength > 0) {
                    XorBuf(kvi->Data, kvi->DataLength);
                    int copy = static_cast<int>(kvi->DataLength) < passSize - 1
                             ? static_cast<int>(kvi->DataLength) : passSize - 1;
                    for (int i = 0; i < copy; ++i) pass[i] = static_cast<char>(kvi->Data[i]);
                    pass[copy] = '\0';
                }
            }
        }

        SecureZeroMemory(buf, sizeof(buf));
        LI_CACHED(NtClose)(hKey);
        return true;
    }

    inline void Save(const char* user, const char* pass, bool remember) {
        HANDLE hKey = OpenAppKey(KEY_SET_VALUE, true);
        if (!hKey) return;

        UNICODE_STRING us;
        DWORD rem = remember ? 1u : 0u;
        {
            auto sName = xorstr_w(L"NodeSlot");
            InitUString(&us, sName);
            LI_CACHED(NtSetValueKey)(hKey, &us, 0, REG_DWORD, &rem, sizeof(rem));
        }

        if (remember && user && pass) {
            unsigned char ubuf[64] = {};
            unsigned char pbuf[64] = {};
            int ulen = 0, plen = 0;
            while (ulen < 63 && user[ulen]) { ubuf[ulen] = static_cast<unsigned char>(user[ulen]); ++ulen; }
            while (plen < 63 && pass[plen]) { pbuf[plen] = static_cast<unsigned char>(pass[plen]); ++plen; }
            XorBuf(ubuf, static_cast<ULONG>(ulen));
            XorBuf(pbuf, static_cast<ULONG>(plen));

            {
                auto sName = xorstr_w(L"ItemIDList");
                InitUString(&us, sName);
                LI_CACHED(NtSetValueKey)(hKey, &us, 0, REG_BINARY, ubuf, static_cast<ULONG>(ulen));
            }
            {
                auto sName = xorstr_w(L"MRUListEx");
                InitUString(&us, sName);
                LI_CACHED(NtSetValueKey)(hKey, &us, 0, REG_BINARY, pbuf, static_cast<ULONG>(plen));
            }

            SecureZeroMemory(ubuf, sizeof(ubuf));
            SecureZeroMemory(pbuf, sizeof(pbuf));
        } else {
            {
                auto sName = xorstr_w(L"ItemIDList");
                InitUString(&us, sName);
                LI_CACHED(NtDeleteValueKey)(hKey, &us);
            }
            {
                auto sName = xorstr_w(L"MRUListEx");
                InitUString(&us, sName);
                LI_CACHED(NtDeleteValueKey)(hKey, &us);
            }
        }

        LI_CACHED(NtClose)(hKey);
    }

    inline void Clear() {
        HANDLE hKey = OpenAppKey(KEY_SET_VALUE, false);
        if (!hKey) return;

        UNICODE_STRING us;
        {
            auto sName = xorstr_w(L"ItemIDList");
            InitUString(&us, sName);
            LI_CACHED(NtDeleteValueKey)(hKey, &us);
        }
        {
            auto sName = xorstr_w(L"MRUListEx");
            InitUString(&us, sName);
            LI_CACHED(NtDeleteValueKey)(hKey, &us);
        }
        {
            auto sName = xorstr_w(L"NodeSlot");
            InitUString(&us, sName);
            LI_CACHED(NtDeleteValueKey)(hKey, &us);
        }

        LI_CACHED(NtClose)(hKey);
    }

}
