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
    case SshMsg::ConnectFailed:
        return IDS_ERRMSG_CONNECT_FAILED;
    case SshMsg::NoHostGiven:
        return IDS_ERRMSG_NO_HOST_GIVEN;
    case SshMsg::NoHostKey:
        return IDS_ERRMSG_NO_HOST_KEY;
    case SshMsg::KnownHostsUnchecked:
        return IDS_ERRMSG_KNOWN_HOSTS_UNCHECKED;
    case SshMsg::HostKeyChanged:
        return IDS_ERRMSG_HOSTKEY_CHANGED;
    case SshMsg::HostKeyRejected:
        return IDS_ERRMSG_HOSTKEY_REJECTED;
    case SshMsg::NoKnownHostsFile:
        return IDS_ERRMSG_NO_KNOWN_HOSTS_FILE;
    case SshMsg::KnownHostsWriteFailed:
        return IDS_ERRMSG_KNOWN_HOSTS_WRITE_FAILED;
    case SshMsg::KnownHostsReplaceFailed:
        return IDS_ERRMSG_KNOWN_HOSTS_REPLACE_FAILED;
    case SshMsg::SftpNeedsAuth:
        return IDS_ERRMSG_SFTP_NEEDS_AUTH;
    case SshMsg::NoSftpSubsystem:
        return IDS_ERRMSG_NO_SFTP_SUBSYSTEM;
    case SshMsg::BrokenAfterTimeout:
        return IDS_ERRMSG_BROKEN_TIMEOUT;
    case SshMsg::BrokenAfterCancel:
        return IDS_ERRMSG_BROKEN_CANCEL;
    case SshMsg::BrokenAfterError:
        return IDS_ERRMSG_BROKEN_ERROR;
    case SshMsg::AuthCancelled:
        return IDS_ERRMSG_AUTH_CANCELLED;
    case SshMsg::AuthNeedsConnection:
        return IDS_ERRMSG_AUTH_NEEDS_CONNECTION;
    case SshMsg::PasswordExpired:
        return IDS_ERRMSG_PASSWORD_EXPIRED;
    case SshMsg::AuthFailed:
        return IDS_ERRMSG_AUTH_FAILED;
    case SshMsg::AuthFailedNoCredentials:
        return IDS_ERRMSG_AUTH_FAILED_NO_CREDENTIALS;
    case SshMsg::KeyFileUnusable:
        return IDS_ERRMSG_KEYFILE_UNUSABLE;
    case SshMsg::KeyFileWrongPassphrase:
        return IDS_ERRMSG_KEYFILE_WRONG_PASSPHRASE;
    case SshMsg::KeyFilePathInvalid:
        return IDS_ERRMSG_KEYFILE_PATH_INVALID;
    case SshMsg::KeyFileOpenFailed:
        return IDS_ERRMSG_KEYFILE_OPEN_FAILED;
    case SshMsg::KeyFileBadSize:
        return IDS_ERRMSG_KEYFILE_BAD_SIZE;
    case SshMsg::KeyFileReadFailed:
        return IDS_ERRMSG_KEYFILE_READ_FAILED;
    case SshMsg::PpkDecryptFailed:
        return IDS_ERRMSG_PPK_DECRYPT_FAILED;
    case SshMsg::PpkUnsupportedEncryption:
        return IDS_ERRMSG_PPK_UNSUPPORTED_ENCRYPTION;
    case SshMsg::PpkUnsupportedKdf:
        return IDS_ERRMSG_PPK_UNSUPPORTED_KDF;
    case SshMsg::PpkUnsupportedKeyType:
        return IDS_ERRMSG_PPK_UNSUPPORTED_KEY_TYPE;
    case SshMsg::PpkUnsupportedOldFormat:
        return IDS_ERRMSG_PPK_UNSUPPORTED_OLD_FORMAT;
    case SshMsg::PpkNotPuttyFile:
        return IDS_ERRMSG_PPK_NOT_PUTTY_FILE;
    case SshMsg::PpkDamaged:
        return IDS_ERRMSG_PPK_DAMAGED;
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
