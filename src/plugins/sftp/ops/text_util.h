// Small helpers for the operations layer: the plugin's data folder and user-presentable error text.
// No Sally or UI headers. UTF-8 <-> UTF-16 conversion is CNameCodec::Utf8() in core/encoding.h.
#pragma once

#include <string>

#include "ssh_types.h"

// %APPDATA%\Sally\SFTP (no trailing backslash); empty when the Roaming AppData folder cannot be determined.
std::wstring AppDataSallyDir();

// Creates the folder and any missing parents. True when the folder exists afterwards (also when it already did).
bool EnsureDirectory(const std::wstring& path);

// The words DescribeSshError needs, supplied by the caller (fs/ loads them from the string table; ops/ has no texts).
struct CSshErrorLabels
{
    std::wstring unknown;    // used when the error has no message, e.g. "Unknown error"
    std::wstring code;       // "code"
    std::wstring sftpStatus; // "SFTP status"
};

// "message (code N, SFTP status M)" as wide text for a message box or log; never empty (unless the labels are).
// Invalid UTF-8 in the message becomes U+FFFD.
std::wstring DescribeSshError(const CSshError& error, const CSshErrorLabels& labels);

// Sally's own known_hosts file: AppDataSallyDir() + "\known_hosts"; empty when AppDataSallyDir() is empty.
std::wstring KnownHostsOwnPath();

// The user's OpenSSH file: <profile>\.ssh\known_hosts (read only for Sally); empty when the profile folder is unknown.
std::wstring KnownHostsUserPath();

// "host:port" for display (host key dialog): an IPv6 literal (contains ':') gets brackets, "[::1]:22".
std::string FormatHostPort(const std::string& host, int port);
