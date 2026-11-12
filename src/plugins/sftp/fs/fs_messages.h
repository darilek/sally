// SPDX-FileCopyrightText: 2026 Sally Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// Small helpers for texts and message boxes that every fs/*.cpp file needs (one copy instead of one per file).

#pragma once

#include <string>

#include "site.h"
#include "ssh_types.h"

// The string-table pattern (with %1, %2) of a core error id; empty when there is none.
std::wstring LangStrForMsg(SshMsg msg);

// The error text with the words taken from the string table (the ops layer has no texts of its own).
std::wstring DescribeError(const CSshError& error);

// An OK message box with the exclamation icon and the plugin name as the title.
void ShowMessage(HWND parent, const std::wstring& text);
// The same, with Sally's current message box parent as the owner.
void ShowMessage(const std::wstring& text);

// "host:port" as the operation window names the server.
std::wstring HostTextOf(const CSite& site);
