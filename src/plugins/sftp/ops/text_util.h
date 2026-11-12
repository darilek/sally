// Small helpers for the operations layer: the plugin's data folder and user-presentable error text.
// No Sally or UI headers. UTF-8 <-> UTF-16 conversion is CNameCodec::Utf8() in core/encoding.h.
#pragma once

#include <string>
#include <vector>

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
// Same, but 'translatedText' (when not empty) replaces error.message as the main text. fs/ passes the string-table
// text of error.msgId, already formatted with FormatErrorMessage.
std::wstring DescribeSshError(const CSshError& error, const CSshErrorLabels& labels, const std::wstring& translatedText);

// Fills a string-table pattern: %1..%9 become the UTF-8 arguments (decoded; invalid bytes become U+FFFD), "%%" is a
// literal '%'. A translation may use the arguments in another order. A %N without an argument becomes empty text;
// any other '%' stays as written. Never throws for bad input.
std::wstring FormatErrorMessage(const std::wstring& pattern, const std::vector<std::string>& args);

// Sally's own known_hosts file: AppDataSallyDir() + "\known_hosts"; empty when AppDataSallyDir() is empty.
std::wstring KnownHostsOwnPath();

// The user's OpenSSH file: <profile>\.ssh\known_hosts (read only for Sally); empty when the profile folder is unknown.
std::wstring KnownHostsUserPath();

// "host:port" for display (host key dialog): an IPv6 literal (contains ':') gets brackets, "[::1]:22".
std::string FormatHostPort(const std::string& host, int port);

// The target path Sally hands to CopyOrMoveFromFS is "path", optionally followed by a NUL and an operation mask.
// Splits at the first NUL ('mask' is empty when there is none).
void SplitPathAndMask(const std::wstring& payload, std::wstring& path, std::wstring& mask);

// What the operation mask of a download means for ONE selected file (Sally splits "C:/dir/new.txt" into the folder and
// the mask "new.txt").
enum class TargetMaskKind
{
    KeepName, // empty, "*" or "*.*": the file keeps its name
    Rename,   // a plain name: the file is saved under it
    Wildcard  // contains '*' or '?' (and is not "*" or "*.*"): not supported
};

// 'newName' is the plain name for Rename (empty otherwise).
TargetMaskKind ClassifyTargetMask(const std::wstring& mask, std::wstring& newName);
