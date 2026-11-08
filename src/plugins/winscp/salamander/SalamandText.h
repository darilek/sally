// SPDX-FileCopyrightText: 2025-2026 Elias Bachaalany
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "plugin_text_encoding.h"
#include "retained_text.h"

// WinSCP's core keeps text in C++Builder AnsiString, i.e. code-page bytes. The Sally SDK is
// UTF-16. These are the conversions at that one boundary, used by the Salamander-facing layer.

// Code-page bytes -> UTF-16. Lossless: the bytes came from the code page. NULL or undecodable
// bytes give an empty string.
inline std::wstring SalWide(const char* text)
{
    std::wstring wide;
    if (text != NULL && !sally::plugin_text::DecodeAcp(text, wide))
        wide.clear();
    return wide;
}

// UTF-16 -> code-page bytes, exact or refused (a substituted file name could name another file).
inline bool SalNarrowExact(const wchar_t* text, std::string& bytes)
{
    if (text == NULL)
    {
        bytes.clear();
        return true;
    }
    return sally::plugin_text::EncodeAcpExact(text, bytes);
}

// Convenience: code-page AnsiString/char* -> UTF-16 for passing to the Sally SDK.
inline std::wstring SalW(const char* text)
{
    return SalWide(text);
}

// UTF-16 from the Sally SDK -> code-page AnsiString for the WinSCP core (lossy chars become '?').
inline AnsiString SalA(const wchar_t* text)
{
    std::string bytes;
    if (text == NULL)
        return AnsiString();
    if (!SalNarrowExact(text, bytes))
    {
        bytes.clear();
        sally::plugin_text::EncodeAcpLossy(text, bytes);
    }
    return AnsiString(bytes.c_str(), static_cast<int>(bytes.size()));
}

// AnsiString (code page) or UnicodeString -> std::wstring, for SDK calls taking const wchar_t*.
inline std::wstring SalWS(const AnsiString& text)
{
    return SalWide(text.c_str());
}

inline std::wstring SalWS(const UnicodeString& text)
{
    return std::wstring(text.c_str(), static_cast<size_t>(text.Length()));
}
