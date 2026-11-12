// SPDX-FileCopyrightText: 2026 Sally Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "fs_messages.h"

#include "text_util.h"

namespace
{
// String-table id of the translatable text of a core error; 0 = no table entry (the English message is shown).
int StringIdOfMsg(SshMsg msg)
{
    switch (msg)
    {
    case SshMsg::ResolveFailed:
        return IDS_ERRMSG_RESOLVE_FAILED;
    case SshMsg::ResolveTimeout:
        return IDS_ERRMSG_RESOLVE_TIMEOUT;
    case SshMsg::ConnectTimeout:
        return IDS_ERRMSG_CONNECT_TIMEOUT;
    case SshMsg::SessionNotOpen:
        return IDS_ERRMSG_SESSION_NOT_OPEN;
    default:
        return 0;
    }
}
} // namespace

// The pattern of a core error from the string table; empty when the id has no entry or the entry is empty.
std::wstring LangStrForMsg(SshMsg msg)
{
    const int id = StringIdOfMsg(msg);
    std::wstring pattern;
    if (id == 0 || !SPLLoadStrOwned(SalamanderGeneral, HLanguage, id, pattern)) // not LangStr: that returns an error text
        return std::wstring();
    return pattern;
}

std::wstring DescribeError(const CSshError& error)
{
    const CSshErrorLabels labels = {LangStr(IDS_ERR_UNKNOWN), LangStr(IDS_ERR_CODE), LangStr(IDS_ERR_SFTP_STATUS)};
    std::wstring translated;
    if (error.msgId != SshMsg::None)
    {
        const std::wstring pattern = LangStrForMsg(error.msgId);
        if (!pattern.empty())
            translated = FormatErrorMessage(pattern, error.args);
    }
    return DescribeSshError(error, labels, translated); // empty translated text = the English message
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
