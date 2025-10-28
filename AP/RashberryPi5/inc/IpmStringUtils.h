#pragma once
/**
 * @file IpmStringUtils.h
 * @brief Lightweight, dependency-minimal UTF-8/UTF-16 helpers and enum stringifiers for IPM.
 *
 * - Windows uses Win32 WideChar APIs (no extra deps).
 * - Linux/macOS fallback uses `std::wstring_convert` (codecvt) for simple tools-layer conversions.
 *
 * Also provides `wchar_t*` stringifiers for public IPM enums declared in @ref CIpmEnv.h and
 * @ref CIpmGpuEnv.h (e.g., #ipm::En_CpuType, #ipm::En_GpuType, #ipm::SupportState).
 */

#include <string>
#include <cwchar>

#if defined(_WIN32) || defined(_WIN64)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
 // codecvt is deprecated but sufficient for a small tool/helper (no heavy ICU dependency).
#include <codecvt>
#include <locale>
#endif

namespace ipm::str {

    /**
     * @brief Convert UTF-8 string to wide (UTF-16 on Windows, UTF-32 on Linux).
     * @param s UTF-8 input.
     * @return Wide string; empty on failure.
     */
    inline std::wstring u8_to_w(const std::string& s) {
#if defined(_WIN32) || defined(_WIN64)
        if (s.empty()) return L"";
        int len = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (len <= 0) return L"";
        std::wstring w(static_cast<size_t>(len) - 1, L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), len);
        return w;
#else
        try {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            return conv.from_bytes(s);
        }
        catch (...) { return L""; }
#endif
    }

    /**
     * @brief Convert wide string to UTF-8.
     * @param w Wide input.
     * @return UTF-8 string; empty on failure.
     */
    inline std::string w_to_u8(const std::wstring& w) {
#if defined(_WIN32) || defined(_WIN64)
        if (w.empty()) return {};
        int len = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) return {};
        std::string s(static_cast<size_t>(len) - 1, '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), len, nullptr, nullptr);
        return s;
#else
        try {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            return conv.to_bytes(w);
        }
        catch (...) { return {}; }
#endif
    }

} // namespace ipm::str

// ===== Enum stringifiers depend on IPM public enums =====
#include "CIpmEnv.h"   // for En_CpuType
#include "CIpmGpuEnv.h"// for En_GpuType / SupportState

namespace ipm::str {

    /// @brief Convert CPU type to wide literal.
    inline const wchar_t* cpu_to_w(En_CpuType c) {
        switch (c) {
        case En_CpuType::x86:    return L"x86";
        case En_CpuType::x86_64: return L"x86_64";
        case En_CpuType::ARM8:   return L"ARMv8";
        case En_CpuType::ARM9:   return L"ARMv7/9";
        default:                 return L"Unknown";
        }
    }

    /// @brief Convert GPU kind to wide literal.
    inline const wchar_t* gpuType_to_w(En_GpuType t) {
        switch (t) {
        case En_GpuType::Internal: return L"Internal";
        case En_GpuType::nVidia:   return L"NVIDIA";
        case En_GpuType::None:     return L"None";
        default:                   return L"Unknown";
        }
    }

    /// @brief Convert feature/support state to wide literal.
    inline const wchar_t* state_to_w(SupportState s) {
        switch (s) {
        case SupportState::Available:    return L"Available";
        case SupportState::NotAvailable: return L"NotAvailable";
        default:                         return L"Unknown";
        }
    }

} // namespace ipm::str
