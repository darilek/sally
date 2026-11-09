// The part of a file system path after the "sftp:" prefix, as a value type with a parser and a formatter.
// Pure functions over std::wstring; no Sally, no I/O.
//
// Forms:  user@host/path   user@host:2222/path   user@[::1]:2222/path   @SiteName/path
// The user may contain '@' (the split is at the LAST '@' of the part before the first '/'). The port is 1..65535
// (default 22). The path is absolute, defaults to "/", and has trailing slashes removed (except for the root).
#pragma once

#include <string>

struct CSftpPath
{
    bool isSite = false;   // "@SiteName/...": user/host/port come from the saved site
    std::wstring siteName; // when isSite
    std::wstring user;     // otherwise
    std::wstring host;     // IPv6 without the brackets
    int port = 22;
    std::wstring path = L"/"; // always starts with '/'
};

// False for anything that is not one of the forms above (no host, bad port, ...). On failure out is unchanged.
bool ParseSftpPath(const std::wstring& afterPrefix, CSftpPath& out);

// The text after "sftp:". The port is omitted when it is 22; an IPv6 host gets its brackets back.
std::wstring FormatSftpPath(const CSftpPath& p);

// Same server, root directory.
CSftpPath RootOf(const CSftpPath& p);

// Same connection: for saved sites the names (case-insensitive), otherwise user (exact) + host (case-insensitive)
// + port. A site reference never equals an explicit user@host.
bool SameServer(const CSftpPath& a, const CSftpPath& b);
