// One panel connection: a CSshSession plus the single thread that owns it. The owner thread (Sally's main thread, or a
// test) submits requests and waits for them with a Win32 event; it never calls libssh2.
//
// Threading rules
//  - Connect/List/Stat/RealPath/Disconnect/Answer*/TryGetInteraction/Close are called from ONE owner thread.
//  - Cancel() is meant for the owner thread too (while it waits for a request); it is thread safe anyway.
//  - The CSshSession is created, used and destroyed on the session thread only (except CSshSession::Cancel).
//
// Requests run in FIFO order, one at a time. Cancel() cancels the oldest unfinished request (the running one, or
// when none is running the first queued one) and nothing else: requests queued behind it still run. A cancelled
// session is broken (core rule), so the session thread disconnects after the cancelled request and the next
// List/Stat/RealPath reconnects automatically with the remembered site (kept in memory, secrets as CSecret, wiped
// by Disconnect and Close). A cancel that arrives when no request is pending does nothing, so it can never leak
// into a later request.
#pragma once

#include <winsock2.h>
#include <windows.h>

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "site.h"
#include "ssh_session.h"

// One request. Created by CPanelSession::Connect/List/..., shared with the session thread. Everything except
// DoneEvent()/IsDone()/Wait() may be read only after the request is done.
class CPanelRequest
{
public:
    enum class Kind
    {
        Connect,
        List,
        RealPath,
        Stat,
        Rename,
        Disconnect
    };

    ~CPanelRequest();
    CPanelRequest(const CPanelRequest&) = delete;
    CPanelRequest& operator=(const CPanelRequest&) = delete;

    // Manual-reset event, signalled when the request is done (successful or not). Wait on it with
    // MsgWaitForMultipleObjects; the handle belongs to the request (valid while you hold the shared_ptr).
    HANDLE DoneEvent() const { return done_; }
    bool IsDone() const { return WaitForSingleObject(done_, 0) == WAIT_OBJECT_0; }
    // True when done within timeoutMs (INFINITE allowed).
    bool Wait(DWORD timeoutMs) const { return WaitForSingleObject(done_, timeoutMs) == WAIT_OBJECT_0; }

    // ---- valid after done
    bool Ok() const { return !error_.IsError(); }
    bool Cancelled() const { return error_.kind == SshErrorKind::Cancelled; }
    const CSshError& Error() const { return error_; }
    Kind GetKind() const { return kind_; }
    // List: the entries (without "." and ".."). RealPath, List: the resolved absolute path. Stat: the attributes
    // (links followed; rawName empty).
    const std::vector<CSftpEntry>& Entries() const { return entries_; }
    const std::string& ResolvedPath() const { return realPath_; }
    const CSftpEntry& Attributes() const { return attributes_; }

private:
    friend class CPanelSession;
    explicit CPanelRequest(Kind kind);

    const Kind kind_;
    HANDLE done_;
    // input
    std::string path_;   // raw bytes
    std::string path2_;  // Rename: the new path (raw bytes)
    bool overwrite_ = false; // Rename
    CSite site_;         // Connect
    CSecret password_;
    CSecret passphrase_;
    // state, guarded by CPanelSession::mutex_
    bool cancelled_ = false;
    bool executed_ = false; // the session thread started it (so CSshSession::Cancel is meaningful)
    // output, written by the session thread before done_ is signalled
    CSshError error_;
    std::vector<CSftpEntry> entries_;
    std::string realPath_;
    CSftpEntry attributes_;
};

// What the session thread is asking the user. Plain data; show a dialog for it and call AnswerHostKey/AnswerPrompt.
struct CInteraction
{
    enum class Kind
    {
        None,
        HostKey, // host, port, keyType, fingerprint, changed
        Prompt   // promptKind, name, instruction, promptText, echo
    };
    Kind kind = Kind::None;
    std::string host; // UTF-8 like everything here
    int port = 22;
    std::string keyType;
    std::string fingerprint; // "SHA256:..."
    bool changed = false;    // a known key differs (show a warning)
    PromptKind promptKind = PromptKind::KeyboardInteractive; // KeyPassphrase: promptText is the key file
    std::string name;        // prompt: window title from the server (may be empty)
    std::string instruction; // prompt: text above the prompts (may be empty)
    std::string promptText;
    bool echo = false;        // false: ask like a password
    std::thread::id threadId; // thread the callback ran on (diagnostics; tests check it is not the owner thread)
};

// What a worker connection needs to log in like the panel did (see CPanelSession::CopyConnectParams).
struct CPanelConnectParams
{
    bool valid = false; // false: no site has been connected successfully (never, failed, disconnected, closed)
    CSite site;
    CSecret password;   // independent copies
    CSecret passphrase;
    std::string hostKeyType;        // "ssh-ed25519", ...
    std::string hostKeyFingerprint; // "SHA256:..." of the key the panel connection accepted
};

class CPanelSession : private ISshCallbacks
{
public:
    // hosts is owned by the caller and must outlive this object; nullptr = no known hosts (see CSshSession).
    explicit CPanelSession(CKnownHosts* hosts);
    ~CPanelSession();
    CPanelSession(const CPanelSession&) = delete;
    CPanelSession& operator=(const CPanelSession&) = delete;

    // ---- Requests (owner thread). After Close() every call returns an already finished, failed request.
    // Connect: disconnects, connects with the site (host key / prompt interactions may happen) and remembers the site
    // and the secrets for automatic reconnects. A failed (not cancelled) connect forgets them again.
    std::shared_ptr<CPanelRequest> Connect(const CSite& site, CSecret password, CSecret passphrase);
    // Raw bytes; "" means the login directory. Result: entries + the resolved real path.
    std::shared_ptr<CPanelRequest> List(const std::string& rawPath);
    std::shared_ptr<CPanelRequest> RealPath(const std::string& rawPath);
    std::shared_ptr<CPanelRequest> Stat(const std::string& rawPath);
    // Renames/moves on the server (same rules as CSshSession::Rename: an existing target gives Exists unless overwrite).
    std::shared_ptr<CPanelRequest> Rename(const std::string& fromRaw, const std::string& toRaw, bool overwrite);
    // Closes the connection and forgets the site and the secrets (no automatic reconnect until the next Connect).
    std::shared_ptr<CPanelRequest> Disconnect();

    // ---- Interaction. InteractionEvent() is a manual-reset event that is signalled when a NEW interaction waits for an
    // answer and reset by TryGetInteraction (which hands each interaction out once), so a wait loop cannot spin.
    // The session thread blocks (the core's idle timeout does not run meanwhile) until AnswerHostKey/AnswerPrompt, a
    // Cancel of the running request or Close; then the callback returns reject/cancel.
    HANDLE InteractionEvent() const { return interactionEvent_; }
    bool TryGetInteraction(CInteraction& out);
    // false when no matching interaction is waiting (already answered, cancelled, or of the other kind).
    bool AnswerHostKey(HostKeyDecision decision);
    bool AnswerPrompt(bool ok, CSecret answer);

    // Any thread. Copies the remembered site, the secrets and the host key the connection accepted, so worker
    // connections can log in the same way without asking again (the fingerprint is the key they must see). The result
    // is invalid until a Connect succeeded. Safe while a request runs.
    CPanelConnectParams CopyConnectParams() const;

    // See the class comment. Returns at once.
    void Cancel();

    // Cancels, stops accepting requests, joins the thread, disconnects, wipes the secrets. Idempotent; safe while a
    // request runs or an interaction waits. Pending requests finish as Cancelled.
    void Close();

    // The session thread's id (valid from the constructor on).
    std::thread::id SessionThreadId() const { return threadId_; }

private:
    // ISshCallbacks, called on the session thread
    HostKeyDecision VerifyHostKey(const std::string& host, int port, const std::string& keyType,
                                  const std::string& fingerprintSha256, bool changed) override;
    bool Prompt(PromptKind kind, const std::string& name, const std::string& instruction, const std::string& promptText,
                bool echo, CSecret& answer) override;
    bool Progress(uint64_t, uint64_t) override { return true; }

    std::shared_ptr<CPanelRequest> Submit(std::shared_ptr<CPanelRequest> req);
    static void Finish(CPanelRequest& req, SshErrorKind kind, const std::string& message) noexcept;
    static void SetError(CPanelRequest& req, SshErrorKind kind, const std::string& message) noexcept;

    // session thread
    void ThreadMain();
    void Execute(CPanelRequest& req);
    bool EnsureConnected(CPanelRequest& req);
    bool DoConnect(CPanelRequest& req);
    void ForgetSite();
    bool WaitForAnswer(std::unique_lock<std::mutex>& lock); // true: answered, false: aborted
    bool AbortRequested() const;                            // mutex_ held

    CKnownHosts* hosts_;
    std::thread thread_;
    std::thread::id threadId_; // same as thread_.get_id(), readable while thread_ is being joined

    // guarded by mutex_
    mutable std::mutex mutex_;
    std::condition_variable queueCv_;
    std::condition_variable answerCv_;
    std::deque<std::shared_ptr<CPanelRequest>> queue_;
    std::shared_ptr<CPanelRequest> running_;
    bool closing_ = false;
    CInteraction interaction_; // kind None = nothing waiting
    bool interactionDelivered_ = false;
    bool answered_ = false;
    HostKeyDecision hostKeyAnswer_ = HostKeyDecision::Reject;
    bool promptOk_ = false;
    CSecret promptAnswer_;

    HANDLE interactionEvent_;

    // session_ lives on the session thread only (Cancel reads it only while running_ is executing)
    std::unique_ptr<CSshSession> session_;
    // Written by the session thread under mutex_, read by it without the lock and by CopyConnectParams with the lock.
    CSite site_;
    CSecret password_;
    CSecret passphrase_;
    bool hasSite_ = false;
    std::string hostKeyType_;        // of the last successful connect
    std::string hostKeyFingerprint_;
};
