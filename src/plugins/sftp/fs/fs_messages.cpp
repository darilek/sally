// SPDX-FileCopyrightText: 2026 Sally Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "fs_messages.h"

#include "text_util.h"

std::wstring DescribeError(const CSshError& error)
{
    const CSshErrorLabels labels = {LangStr(IDS_ERR_UNKNOWN), LangStr(IDS_ERR_CODE), LangStr(IDS_ERR_SFTP_STATUS)};
    return DescribeSshError(error, labels);
}

void ShowMessage(HWND parent, const std::wstring& text)
{
    SalamanderGeneral->SalMessageBox(parent, text.c_str(), LangStr(IDS_PLUGINNAME).c_str(),
                                     MB_OK | MB_ICONEXCLAMATION);
}

void ShowMessage(const std::wstring& text)
{
    ShowMessage(SalamanderGeneral->GetMsgBoxParent(), text);
}

std::wstring HostTextOf(const CSite& site)
{
    const CNameCodec utf8 = CNameCodec::Utf8();
    return utf8.Decode(FormatHostPort(utf8.Encode(site.host), site.port));
}
