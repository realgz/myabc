// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/deployment/backend/com_register.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.4
//
// 参考微软 SampleIME（MIT）Register.cpp 的 RegisterServer/UnregisterServer 逐函数对照移植。
// DECISION: docs/decisions/_debt-log.md（SampleIME 片段来源登记）。

#include "com_register.hpp"

#include <objbase.h>   // StringFromGUID2

#include <iterator>
#include <string>

#include "guids.hpp"

namespace myabc::deploy {

namespace {

std::wstring GuidToString(const GUID& g) {
    wchar_t buf[64] = {};
    ::StringFromGUID2(g, buf, static_cast<int>(std::size(buf)));
    return buf;
}

std::wstring ClsidKeyPath() {
    return L"CLSID\\" + GuidToString(CLSID_MyabcTextService);
}

std::wstring ModuleDir() {
    wchar_t path[MAX_PATH] = {};
    const DWORD n = ::GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    std::wstring p(path, n);
    const auto slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : p.substr(0, slash);
}

LSTATUS SetString(HKEY key, const wchar_t* name, const std::wstring& value) {
    return ::RegSetValueExW(
        key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
}

}  // namespace

HRESULT RegisterComServer(const std::wstring& dll_path) {
    const std::wstring dll = dll_path.empty() ? (ModuleDir() + L"\\myabc-tip.dll") : dll_path;

    const std::wstring clsid_key = ClsidKeyPath();
    HKEY key = nullptr;
    LSTATUS st = ::RegCreateKeyExW(HKEY_CLASSES_ROOT, clsid_key.c_str(), 0, nullptr,
                                   REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr);
    if (st != ERROR_SUCCESS) return HRESULT_FROM_WIN32(st);
    SetString(key, nullptr, L"myabc Text Service");

    HKEY inproc = nullptr;
    st = ::RegCreateKeyExW(key, L"InprocServer32", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE,
                           nullptr, &inproc, nullptr);
    ::RegCloseKey(key);
    if (st != ERROR_SUCCESS) return HRESULT_FROM_WIN32(st);

    st = SetString(inproc, nullptr, dll);
    if (st == ERROR_SUCCESS) st = SetString(inproc, L"ThreadingModel", L"Apartment");
    ::RegCloseKey(inproc);
    return HRESULT_FROM_WIN32(st);
}

HRESULT UnregisterComServer() {
    const std::wstring clsid_key = ClsidKeyPath();
    const LSTATUS st = ::RegDeleteTreeW(HKEY_CLASSES_ROOT, clsid_key.c_str());
    if (st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND) return S_OK;
    return HRESULT_FROM_WIN32(st);
}

bool QueryComServer(std::wstring& out_path) {
    const std::wstring inproc_key = ClsidKeyPath() + L"\\InprocServer32";
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CLASSES_ROOT, inproc_key.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }
    wchar_t buf[MAX_PATH] = {};
    DWORD cb = sizeof(buf);
    DWORD type = 0;
    const LSTATUS st = ::RegQueryValueExW(key, nullptr, nullptr, &type,
                                          reinterpret_cast<BYTE*>(buf), &cb);
    ::RegCloseKey(key);
    if (st != ERROR_SUCCESS || type != REG_SZ) return false;
    out_path.assign(buf);
    return ::GetFileAttributesW(out_path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

}  // namespace myabc::deploy
