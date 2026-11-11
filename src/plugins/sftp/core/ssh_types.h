// Plain types shared by the SSH/SFTP core: errors, site settings, directory entries, callbacks.
// No Sally or UI headers here; strings are UTF-8 (messages) or raw bytes (remote names).
#pragma once

#include <cstdint>
#include <string>

#include "encoding.h"
#include "secret.h"

enum class SshErrorKind
{
    None,             // no error
    Cancelled,        // the user or the plugin cancelled the operation
    ConnectFailed,    // DNS, TCP connect, handshake or connection lost
    Timeout,          // no answer within the site's timeout
    HostKeyRejected,  // the user (or a missing callback) refused the host key
    HostKeyChanged,   // known key differs and was not accepted
    AuthFailed,       // no offered authentication method succeeded
    KeyFileError,     // key file missing, unreadable, corrupt or wrong passphrase
    NoSftp,           // the server has no SFTP subsystem
    NotFound,         // SFTP: no such file or path
    PermissionDenied, // SFTP: permission denied
    Exists,           // SFTP: target already exists
    NotEmpty,         // SFTP: directory not empty
    Unsupported,      // operation not supported by the server
    Protocol,         // unexpected protocol data
    Io,               // local or remote I/O failure (disk full, write failed, ...)
    Other             // anything else; see message
};

struct CSshError
{
    SshErrorKind kind = SshErrorKind::None;
    int libssh2Code = 0;          // LIBSSH2_ERROR_* (0 = none)
    unsigned long sftpStatus = 0; // LIBSSH2_FX_* when libssh2Code is LIBSSH2_ERROR_SFTP_PROTOCOL
    std::string message;          // UTF-8, for display and logs

    bool IsError() const { return kind != SshErrorKind::None; }
};

// Connection settings of one site. Movable, not copyable (CSecret has no implicit copy).
struct CSshSite
{
    std::string host;
    int port = 22;
    std::string user;
    CSecret password;
    std::string keyFile; // path, UTF-8; empty = none
    CSecret passphrase;
    bool useAgent = false;
    bool compression = false;
    int timeoutSeconds = 30;
    CNameCodec nameCodec; // how remote names are decoded for display / encoded for new names
};

// One directory entry. The raw name is what every later operation must use.
struct CSftpEntry
{
    std::string rawName; // bytes exactly as the server sent them
    uint64_t size = 0;
    int64_t mtime = 0;        // Unix time, seconds
    int64_t atime = 0;        // Unix time, seconds (0 = not reported)
    uint32_t permissions = 0; // st_mode bits
    std::string owner;        // may be empty (numeric ids or not reported)
    std::string group;
    bool isDir = false;
    bool isLink = false;
    bool linkTargetIsDir = false; // valid when isLink
};

// File system size of a remote path (the "statvfs@openssh.com" SFTP extension), all in bytes.
struct CSftpVfs
{
    uint64_t bytesTotal = 0;           // f_blocks * f_frsize
    uint64_t bytesFree = 0;            // f_bfree * f_frsize (free for root too)
    uint64_t bytesAvailableToUser = 0; // f_bavail * f_frsize (what a normal user can still write)
    uint64_t files = 0;                // inodes
    uint64_t filesFree = 0;
    bool readOnly = false;
};

// What the server told at connect time. libssh2 gives the identification banner only: it does not expose the SFTP
// protocol version the server answered (it speaks version 3 at most) nor the list of SFTP extensions.
struct CSshServerInfo
{
    std::string banner;          // e.g. "SSH-2.0-OpenSSH_9.6"; empty when not known
    int sftpVersionSupported = 0; // highest SFTP version libssh2 speaks (3); the server's own version is not reported
};

// Algorithms the handshake negotiated; empty text = not connected.
struct CSshMethods
{
    std::string kex;
    std::string hostKey;
    std::string cipherClientToServer;
    std::string cipherServerToClient;
    std::string macClientToServer;
    std::string macServerToClient;
    std::string compressionClientToServer;
    std::string compressionServerToClient;
};

// False for a name that cannot be one entry of a directory listing: empty, "." or "..", or containing '/' or NUL.
// (A server could send such names to make a client write outside the folder it asked for.) Not a check for local
// Windows file names.
bool IsValidRemoteEntryName(const std::string& name);

enum class HostKeyDecision
{
    Reject,
    AcceptOnce,
    AcceptAndSave
};

// What a Prompt callback asks for. The core sends no user-facing text of its own: the UI words KeyPassphrase itself.
enum class PromptKind
{
    KeyboardInteractive, // name/instruction/promptText are the server's own texts
    KeyPassphrase        // promptText is the key file (UTF-8); name and instruction are empty
};

// Implemented by the layer above (UI or tests). Called on the thread that owns the session.
class ISshCallbacks
{
public:
    virtual ~ISshCallbacks() = default;

    // Unknown (changed == false) or changed (changed == true) host key. fingerprintSha256 is the OpenSSH form "SHA256:...".
    virtual HostKeyDecision VerifyHostKey(const std::string& host, int port, const std::string& keyType,
                                          const std::string& fingerprintSha256, bool changed) = 0;

    // Keyboard-interactive prompt or passphrase request. Returns false when the user cancels.
    virtual bool Prompt(PromptKind kind, const std::string& name, const std::string& instruction, const std::string& promptText,
                        bool echo, CSecret& answer) = 0;

    // Transfer progress; return false to cancel. total may be 0 when unknown.
    virtual bool Progress(uint64_t done, uint64_t total) = 0;
};
