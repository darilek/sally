#include "panel_session.h"

#include <cassert>
#include <exception>
#include <new>

#include "ssh_init.h"

CPanelRequest::CPanelRequest(Kind kind) : kind_(kind), done_(CreateEventW(nullptr, TRUE, FALSE, nullptr))
{
    if (done_ == nullptr)
        throw std::bad_alloc();
}

CPanelRequest::~CPanelRequest()
{
    CloseHandle(done_);
}

CPanelSession::CPanelSession(CKnownHosts* hosts)
    : hosts_(hosts), interactionEvent_(CreateEventW(nullptr, TRUE, FALSE, nullptr))
{
    if (interactionEvent_ == nullptr)
        throw std::bad_alloc();
    try
    {
        // The new thread starts with lock+unlock of mutex_, so it never runs before threadId_ is set.
        std::lock_guard<std::mutex> lock(mutex_);
        thread_ = std::thread([this] { ThreadMain(); });
        threadId_ = thread_.get_id();
    }
    catch (...)
    {
        CloseHandle(interactionEvent_);
        throw;
    }
}

CPanelSession::~CPanelSession()
{
    Close();
    CloseHandle(interactionEvent_);
}

// ---- owner thread --------------------------------------------------------------------------------------------

std::shared_ptr<CPanelRequest> CPanelSession::Submit(std::shared_ptr<CPanelRequest> req)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!closing_)
        {
            queue_.push_back(req);
            queueCv_.notify_one();
            return req;
        }
    }
    Finish(*req, SshErrorKind::Cancelled, "The session is closed");
    return req;
}

std::shared_ptr<CPanelRequest> CPanelSession::Connect(const CSite& site, CSecret password, CSecret passphrase)
{
    std::shared_ptr<CPanelRequest> req(new CPanelRequest(CPanelRequest::Kind::Connect));
    req->site_ = site;
    req->password_ = std::move(password);
    req->passphrase_ = std::move(passphrase);
    return Submit(req);
}

std::shared_ptr<CPanelRequest> CPanelSession::List(const std::string& rawPath)
{
    std::shared_ptr<CPanelRequest> req(new CPanelRequest(CPanelRequest::Kind::List));
    req->path_ = rawPath;
    return Submit(req);
}

std::shared_ptr<CPanelRequest> CPanelSession::RealPath(const std::string& rawPath)
{
    std::shared_ptr<CPanelRequest> req(new CPanelRequest(CPanelRequest::Kind::RealPath));
    req->path_ = rawPath;
    return Submit(req);
}

std::shared_ptr<CPanelRequest> CPanelSession::Stat(const std::string& rawPath)
{
    std::shared_ptr<CPanelRequest> req(new CPanelRequest(CPanelRequest::Kind::Stat));
    req->path_ = rawPath;
    return Submit(req);
}

std::shared_ptr<CPanelRequest> CPanelSession::Rename(const std::string& fromRaw, const std::string& toRaw, bool overwrite)
{
    std::shared_ptr<CPanelRequest> req(new CPanelRequest(CPanelRequest::Kind::Rename));
    req->path_ = fromRaw;
    req->path2_ = toRaw;
    req->overwrite_ = overwrite;
    return Submit(req);
}

CPanelConnectParams CPanelSession::CopyConnectParams() const
{
    CPanelConnectParams params;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!hasSite_ || hostKeyFingerprint_.empty())
        return params;
    params.site = site_;
    params.password = password_.Clone();
    params.passphrase = passphrase_.Clone();
    params.hostKeyType = hostKeyType_;
    params.hostKeyFingerprint = hostKeyFingerprint_;
    params.valid = true;
    return params;
}

std::shared_ptr<CPanelRequest> CPanelSession::Disconnect()
{
    return Submit(std::shared_ptr<CPanelRequest>(new CPanelRequest(CPanelRequest::Kind::Disconnect)));
}

bool CPanelSession::TryGetInteraction(CInteraction& out)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (interaction_.kind == CInteraction::Kind::None || interactionDelivered_)
        return false;
    if (AbortRequested())
        return false; // the request was cancelled (or the session closed): a dialog for it would be stale
    out = interaction_;
    interactionDelivered_ = true;
    ResetEvent(interactionEvent_);
    return true;
}

bool CPanelSession::AnswerHostKey(HostKeyDecision decision)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (interaction_.kind != CInteraction::Kind::HostKey || answered_)
        return false;
    hostKeyAnswer_ = decision;
    answered_ = true;
    answerCv_.notify_all();
    return true;
}

bool CPanelSession::AnswerPrompt(bool ok, CSecret answer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (interaction_.kind != CInteraction::Kind::Prompt || answered_)
        return false;
    promptOk_ = ok;
    promptAnswer_ = std::move(answer);
    answered_ = true;
    answerCv_.notify_all();
    return true;
}

void CPanelSession::Cancel()
{
    std::lock_guard<std::mutex> lock(mutex_);
    // Oldest unfinished request: the running one, else the first queued one.
    std::shared_ptr<CPanelRequest> target = running_ != nullptr ? running_ : (queue_.empty() ? nullptr : queue_.front());
    if (target == nullptr)
        return; // nothing to cancel; a flag set now would hit a later request
    target->cancelled_ = true;
    if (target == running_ && target->executed_)
        session_->Cancel(); // session_ exists: executed_ is set only by the session thread after creating it
    answerCv_.notify_all(); // wakes an interaction wait (AbortRequested sees the flag)
}

void CPanelSession::Close()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closing_ = true;
        if (running_ != nullptr)
        {
            running_->cancelled_ = true;
            if (running_->executed_)
                session_->Cancel();
        }
        queueCv_.notify_all();
        answerCv_.notify_all();
    }
    if (thread_.joinable() && std::this_thread::get_id() != threadId_)
        thread_.join();
}

// ---- session thread ------------------------------------------------------------------------------------------

void CPanelSession::Finish(CPanelRequest& req, SshErrorKind kind, const std::string& message) noexcept
{
    SetError(req, kind, message);
    SetEvent(req.done_);
}

// Never throws (it is also used in catch blocks): when the message cannot be copied, the kind alone is reported.
void CPanelSession::SetError(CPanelRequest& req, SshErrorKind kind, const std::string& message) noexcept
{
    req.error_ = CSshError();
    req.error_.kind = kind;
    try
    {
        req.error_.message = message;
    }
    catch (...)
    {
        req.error_.message.clear();
    }
}

void CPanelSession::ThreadMain()
{
    { std::lock_guard<std::mutex> wait(mutex_); } // see the constructor
    CSshThreadScope threadScope; // frees OpenSSL's per-thread state when the thread ends (declared first: destroyed last)
    try
    {
        session_.reset(new CSshSession(this, hosts_));
    }
    catch (...)
    {
    }
    for (;;)
    {
        std::shared_ptr<CPanelRequest> req;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queueCv_.wait(lock, [this] { return closing_ || !queue_.empty(); });
            if (closing_)
                break;
            req = queue_.front();
            queue_.pop_front();
            running_ = req;
            // A request cancelled while queued is not run (Disconnect always runs: it cannot be undone by a cancel).
            req->executed_ = session_ != nullptr && (!req->cancelled_ || req->kind_ == CPanelRequest::Kind::Disconnect);
        }
        if (req->executed_)
        {
            try
            {
                Execute(*req);
            }
            catch (const std::exception& e)
            {
                try
                {
                    SetError(*req, SshErrorKind::Other, std::string("Internal error: ") + e.what());
                }
                catch (...) // building the text failed (out of memory)
                {
                    SetError(*req, SshErrorKind::Other, "Internal error");
                }
            }
            catch (...)
            {
                SetError(*req, SshErrorKind::Other, "Internal error");
            }
        }
        else if (session_ == nullptr)
            SetError(*req, SshErrorKind::Other, "The SSH session could not be created");

        bool wasCancelled;
        {
            // Under the lock, so a Cancel() sees either "running" (handled here) or "finished" (then it is a no-op).
            std::lock_guard<std::mutex> lock(mutex_);
            wasCancelled = req->cancelled_ && req->kind_ != CPanelRequest::Kind::Disconnect;
            running_.reset();
        }
        if (wasCancelled)
        {
            if (req->executed_)
            {
                try
                {
                    session_->Disconnect(); // a cancelled session is broken until Disconnect (also clears the cancel flag)
                }
                catch (...)
                {
                }
            }
            SetError(*req, SshErrorKind::Cancelled, "Cancelled");
            req->entries_.clear();
        }
        SetEvent(req->done_);
    }

    // closing: finish what is left, drop the connection and the secrets
    std::deque<std::shared_ptr<CPanelRequest>> rest;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rest.swap(queue_);
    }
    for (const std::shared_ptr<CPanelRequest>& r : rest)
        Finish(*r, SshErrorKind::Cancelled, "The session is closed");
    try
    {
        if (session_ != nullptr)
            session_->Disconnect();
        session_.reset();
    }
    catch (...)
    {
    }
    ForgetSite();
}

void CPanelSession::ForgetSite()
{
    std::lock_guard<std::mutex> lock(mutex_);
    hasSite_ = false;
    site_ = CSite();
    password_.clear();
    passphrase_.clear();
    hostKeyType_.clear();
    hostKeyFingerprint_.clear();
}

bool CPanelSession::DoConnect(CPanelRequest& req)
{
    session_->Disconnect(); // also resets a broken session - and clears a cancel request that arrived before this line,
    {                       // so look at our own flag (set under the lock before CSshSession::Cancel) afterwards
        std::lock_guard<std::mutex> lock(mutex_);
        if (req.cancelled_ || closing_)
        {
            SetError(req, SshErrorKind::Cancelled, "Cancelled");
            return false;
        }
    }
    CSshSite site = ToSshSite(site_, password_.Clone(), passphrase_.Clone());
    if (session_->Connect(site))
    {
        // A successful connect means the host key was trusted (known_hosts match, AcceptOnce or AcceptAndSave), so the
        // session's key is the one workers must see, whichever way it was accepted.
        std::lock_guard<std::mutex> lock(mutex_);
        hostKeyType_ = session_->HostKeyType();
        hostKeyFingerprint_ = session_->HostKeyFingerprint();
        return true;
    }
    req.error_ = session_->LastError();
    return false;
}

// Reconnects with the remembered site when the connection is gone (cancelled, timed out, closed by the server).
bool CPanelSession::EnsureConnected(CPanelRequest& req)
{
    if (session_->IsConnected() && session_->HasSftp())
        return true;
    if (!hasSite_)
    {
        SetError(req, SshErrorKind::ConnectFailed, "Not connected");
        return false;
    }
    return DoConnect(req);
}

void CPanelSession::Execute(CPanelRequest& req)
{
    // "libssh2 only on the session thread", checked in debug builds
    assert(std::this_thread::get_id() == threadId_);
    const std::string path = req.path_.empty() ? "." : req.path_;
    switch (req.kind_)
    {
    case CPanelRequest::Kind::Connect:
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            site_ = req.site_;
            password_ = std::move(req.password_);
            passphrase_ = std::move(req.passphrase_);
            hostKeyType_.clear(); // belongs to the previous connection until this one is accepted
            hostKeyFingerprint_.clear();
            hasSite_ = true;
        }
        if (!DoConnect(req))
        {
            bool cancelled;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                cancelled = req.cancelled_ || closing_;
            }
            if (!cancelled)
                ForgetSite(); // a cancelled connect can be retried by the next request
        }
        break;
    }
    case CPanelRequest::Kind::Disconnect:
        session_->Disconnect();
        ForgetSite();
        break;
    case CPanelRequest::Kind::RealPath:
        if (EnsureConnected(req) && !session_->RealPath(path, req.realPath_))
            req.error_ = session_->LastError();
        break;
    case CPanelRequest::Kind::Stat:
        if (EnsureConnected(req) && !session_->Stat(path, req.attributes_))
            req.error_ = session_->LastError();
        break;
    case CPanelRequest::Kind::Rename:
        if (EnsureConnected(req) && !session_->Rename(req.path_, req.path2_, req.overwrite_))
            req.error_ = session_->LastError();
        break;
    case CPanelRequest::Kind::List:
        if (!EnsureConnected(req))
            break;
        // resolve first ("", ".", relative paths, links in the path), then list the real directory
        if (!session_->RealPath(path, req.realPath_) || !session_->ListDir(req.realPath_, req.entries_))
            req.error_ = session_->LastError();
        break;
    }
}

// ---- callbacks (session thread) ------------------------------------------------------------------------------

bool CPanelSession::AbortRequested() const
{
    return closing_ || (running_ != nullptr && running_->cancelled_);
}

// Posts interaction_ and blocks until it is answered or aborted. The core's idle timeout does not run meanwhile.
bool CPanelSession::WaitForAnswer(std::unique_lock<std::mutex>& lock)
{
    answered_ = false;
    interactionDelivered_ = false;
    SetEvent(interactionEvent_);
    answerCv_.wait(lock, [this] { return answered_ || AbortRequested(); });
    const bool ok = answered_;
    interaction_ = CInteraction();
    interactionDelivered_ = false;
    ResetEvent(interactionEvent_);
    return ok;
}

HostKeyDecision CPanelSession::VerifyHostKey(const std::string& host, int port, const std::string& keyType,
                                             const std::string& fingerprintSha256, bool changed)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (AbortRequested())
        return HostKeyDecision::Reject;
    interaction_ = CInteraction();
    interaction_.kind = CInteraction::Kind::HostKey;
    interaction_.host = host;
    interaction_.port = port;
    interaction_.keyType = keyType;
    interaction_.fingerprint = fingerprintSha256;
    interaction_.changed = changed;
    interaction_.threadId = std::this_thread::get_id();
    return WaitForAnswer(lock) ? hostKeyAnswer_ : HostKeyDecision::Reject;
}

bool CPanelSession::Prompt(PromptKind kind, const std::string& name, const std::string& instruction,
                           const std::string& promptText, bool echo, CSecret& answer)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (AbortRequested())
        return false;
    interaction_ = CInteraction();
    interaction_.kind = CInteraction::Kind::Prompt;
    interaction_.promptKind = kind;
    interaction_.name = name;
    interaction_.instruction = instruction;
    interaction_.promptText = promptText;
    interaction_.echo = echo;
    interaction_.threadId = std::this_thread::get_id();
    promptOk_ = false;
    if (!WaitForAnswer(lock) || !promptOk_)
    {
        promptAnswer_.clear();
        return false;
    }
    answer = std::move(promptAnswer_);
    return true;
}
