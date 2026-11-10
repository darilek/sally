#include "op_worker.h"

#include <exception>
#include <utility>

#include "listing_format.h"
#include "local_name.h"
#include "ssh_init.h"
#include "text_util.h"

namespace
{
constexpr int kIdleWaitMs = 200; // a waiting worker looks again at least this often
constexpr uint32_t kModeTypeMask = 0170000;
constexpr uint32_t kModeRegular = 0100000;
constexpr uint32_t kModeDirectory = 0040000;
constexpr uint32_t kModeSymlink = 0120000;

std::wstring ToWide(const std::string& utf8)
{
    return CNameCodec::Utf8().Decode(utf8);
}

std::wstring WindowsErrorText(DWORD error)
{
    if (error == 0)
        error = ERROR_GEN_FAILURE;
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0,
        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring text;
    if (length > 0 && buffer != nullptr)
        text.assign(buffer, length);
    LocalFree(buffer);
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' '))
        text.pop_back();
    return text + L" (" + std::to_wstring(error) + L")";
}

Problem ProblemFromWin32(DWORD error, Problem fallback)
{
    if (error == ERROR_DISK_FULL || error == ERROR_HANDLE_DISK_FULL)
        return Problem::DiskFull;
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        return Problem::NotFound;
    return fallback;
}

// The core reports local file errors as kind Io with "(Windows error N)" in the message and remote ones as kind Io
// without it; that is all the information there is.
Problem ProblemFromSshError(const CSshError& error, Problem fallback)
{
    switch (error.kind)
    {
    case SshErrorKind::NotFound:
        return Problem::NotFound;
    case SshErrorKind::PermissionDenied:
        return Problem::PermissionDenied;
    case SshErrorKind::Exists:
        return Problem::TargetExists;
    case SshErrorKind::Io:
    {
        if (error.message.find("Windows error 112)") != std::string::npos ||
            error.message.find("Windows error 39)") != std::string::npos)
            return Problem::DiskFull;
        const bool local = error.message.find("(Windows error ") != std::string::npos;
        return local ? Problem::LocalIoError : Problem::TransferFailed;
    }
    default:
        return fallback;
    }
}

int64_t UnixTimeOf(const FILETIME& ft)
{
    ULARGE_INTEGER t;
    t.LowPart = ft.dwLowDateTime;
    t.HighPart = ft.dwHighDateTime;
    constexpr int64_t kEpochDiff100ns = 116444736000000000LL; // 1601 -> 1970
    return (static_cast<int64_t>(t.QuadPart) - kEpochDiff100ns) / 10000000;
}

uint64_t SizeOf(DWORD high, DWORD low)
{
    return (static_cast<uint64_t>(high) << 32) | low;
}

std::string ParentOfRemote(const std::string& path)
{
    std::string p = path;
    while (p.size() > 1 && p.back() == '/')
        p.pop_back();
    const size_t slash = p.find_last_of('/');
    if (slash == std::string::npos)
        return std::string(); // relative to the login directory
    return slash == 0 ? std::string("/") : p.substr(0, slash);
}

std::wstring ParentOfLocal(const std::wstring& path)
{
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

bool IsSpecialFile(const CSftpEntry& entry)
{
    // devices, pipes and sockets would block or return nothing; "no type bits" (a server that sends none) is a file
    const uint32_t type = entry.permissions & kModeTypeMask;
    return type != 0 && type != kModeRegular && type != kModeDirectory && type != kModeSymlink;
}

// Deletes a local file, also a read-only one.
bool DeleteLocalFileForce(const std::wstring& path, DWORD& error)
{
    if (DeleteFileW(path.c_str()))
        return true;
    error = GetLastError();
    if (error == ERROR_ACCESS_DENIED && SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL))
    {
        if (DeleteFileW(path.c_str()))
            return true;
        error = GetLastError();
    }
    return false;
}

// A symbolic link or junction: take size, time and "is a directory" from what it points to.
void FollowReparsePoint(const std::wstring& path, DWORD& attributes, uint64_t& size, FILETIME& writeTime)
{
    HANDLE h = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return; // a broken link stays what it is; the upload of it fails later with a clear error
    BY_HANDLE_FILE_INFORMATION info;
    if (GetFileInformationByHandle(h, &info))
    {
        attributes = info.dwFileAttributes;
        size = SizeOf(info.nFileSizeHigh, info.nFileSizeLow);
        writeTime = info.ftLastWriteTime;
    }
    CloseHandle(h);
}
} // namespace

// ---- shared helpers -------------------------------------------------------------------------------------------

std::string AppendRemoteName(const std::string& dir, const std::string& rawName)
{
    if (dir.empty())
        return rawName;
    return dir.back() == '/' ? dir + rawName : dir + "/" + rawName;
}

std::string LastRemoteName(const std::string& path)
{
    std::string p = path;
    while (p.size() > 1 && p.back() == '/')
        p.pop_back();
    const size_t slash = p.find_last_of('/');
    return slash == std::string::npos ? p : p.substr(slash + 1);
}

void AddDownloadItems(std::vector<CQueueItem>& out, const std::string& remotePath, const std::string& rawName, bool isDir,
                      uint64_t size, int64_t mtime, const std::wstring& localDir, const CNameCodec& codec, int depth)
{
    bool lossy = false;
    const std::wstring name = DecodeRemoteNameToLocal(codec, rawName, &lossy);
    const bool valid = !lossy && IsValidLocalName(name);

    CQueueItem item;
    item.remotePath = remotePath;
    item.localPath = valid ? MakeLocalPath(localDir, name) : std::wstring(); // empty: the worker parks the item
    item.depth = depth;
    if (isDir)
    {
        if (valid)
        {
            item.type = ItemType::MakeDirLocal;
            out.push_back(item);
        }
        item.type = ItemType::ExploreRemote;
    }
    else
    {
        item.type = ItemType::Download;
        item.size = size;
        item.mtime = mtime;
    }
    out.push_back(std::move(item));
}

void AddUploadItems(std::vector<CQueueItem>& out, const std::wstring& localPath, const std::wstring& name, bool isDir,
                    uint64_t size, int64_t mtime, const std::string& remoteDir, const CNameCodec& codec, int depth)
{
    bool unrepresentable = false;
    const std::string rawName = codec.Encode(name, &unrepresentable);

    CQueueItem item;
    item.localPath = localPath;
    item.remotePath = unrepresentable ? std::string() : AppendRemoteName(remoteDir, rawName);
    item.depth = depth;
    if (isDir)
    {
        if (!unrepresentable)
        {
            item.type = ItemType::MakeDirRemote;
            out.push_back(item);
        }
        item.type = ItemType::ExploreLocal;
    }
    else
    {
        item.type = ItemType::Upload;
        item.size = size;
        item.mtime = mtime;
    }
    out.push_back(std::move(item));
}

// ---- CItemResult builders -------------------------------------------------------------------------------------

CWorker::CItemResult CWorker::Failure(Problem problem, std::wstring text)
{
    CItemResult r;
    r.kind = CItemResult::Kind::Failed;
    r.problem = problem;
    r.text = std::move(text);
    return r;
}

CWorker::CItemResult CWorker::Parked(Problem problem)
{
    CItemResult r;
    r.kind = CItemResult::Kind::Parked;
    r.problem = problem;
    return r;
}

CWorker::CItemResult CWorker::Returned()
{
    CItemResult r;
    r.kind = CItemResult::Kind::ReturnToWaiting;
    return r;
}

CWorker::CItemResult CWorker::Replaced()
{
    CItemResult r;
    r.kind = CItemResult::Kind::Replaced;
    return r;
}

// ---- thread ---------------------------------------------------------------------------------------------------

CWorker::CWorker(IWorkerHost& host, int index) : host_(host), index_(index), session_(this, nullptr)
{
}

CWorker::~CWorker()
{
    Join();
}

void CWorker::Start()
{
    thread_ = std::thread([this] { ThreadMain(); });
}

void CWorker::Cancel()
{
    session_.Cancel();
}

void CWorker::Join()
{
    if (thread_.joinable())
        thread_.join();
}

void CWorker::ThreadMain()
{
    try
    {
        CSshThreadScope scope; // first: it must outlive every libssh2/OpenSSL call of this thread
        try
        {
            Run();
        }
        catch (...)
        {
            // nothing may leave a thread function; an item that was being processed stays Processing (only possible
            // after an out-of-memory in the queue itself)
        }
        session_.Disconnect();
    }
    catch (...)
    {
    }
    try
    {
        host_.WorkerExited();
    }
    catch (...)
    {
    }
}

void CWorker::Run()
{
    codec_ = host_.ConnectParams().site.MakeCodec();
    CQueue& queue = host_.Queue();
    CQueueItem item;
    while (!host_.IsCancelRequested())
    {
        if (queue.TakeNextWaiting(item))
        {
            ProcessItem(item);
            continue;
        }
        if (queue.GetOperationState() != OperationState::Running)
            break; // nothing left that this worker could ever do
        queue.WaitForWork(kIdleWaitMs);
    }
}

void CWorker::ProcessItem(const CQueueItem& item)
{
    CItemResult result;
    try
    {
        result = RunItem(item);
    }
    catch (const std::exception& e)
    {
        result = Failure(Problem::Other, ToWide(e.what()));
    }
    catch (...)
    {
        result = Failure(Problem::Other, std::wstring());
    }
    if (result.kind == CItemResult::Kind::ReturnToWaiting)
        session_.Disconnect(); // a cancelled session must be closed before it is used again

    try
    {
        Report(item, result);
    }
    catch (...)
    {
    }
}

void CWorker::Report(const CQueueItem& item, const CItemResult& result)
{
    CQueue& queue = host_.Queue();
    switch (result.kind)
    {
    case CItemResult::Kind::Done:
        queue.Finish(item.uid, ItemState::Done);
        break;
    case CItemResult::Kind::Failed:
        queue.Finish(item.uid, ItemState::Failed, result.problem, result.text);
        break;
    case CItemResult::Kind::Parked:
        queue.Finish(item.uid, ItemState::UserInputNeeded, result.problem, result.text);
        break;
    case CItemResult::Kind::ReturnToWaiting:
        queue.ReturnToWaiting(item.uid);
        break;
    case CItemResult::Kind::Replaced:
        break; // the queue call that replaced the item already changed its state
    }
    host_.ItemChanged(item.uid);
}

bool CWorker::NeedsConnection(ItemType type)
{
    switch (type)
    {
    case ItemType::ExploreRemote:
    case ItemType::Download:
    case ItemType::Upload:
    case ItemType::MakeDirRemote:
    case ItemType::DeleteRemoteFile:
    case ItemType::DeleteRemoteDir:
        return true;
    default:
        return false;
    }
}

// An item that fails because the connection broke is tried once more on a fresh connection; the second failure stays.
CWorker::CItemResult CWorker::RunItem(const CQueueItem& item)
{
    if (host_.IsCancelRequested())
        return Returned();
    if (!NeedsConnection(item.type))
        return ExecuteOnce(item);

    CItemResult result;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        if (!EnsureConnected(result))
            return result;
        result = ExecuteOnce(item);
        if (!result.connectionLost)
            return result;
        session_.Disconnect();
        if (host_.IsCancelRequested())
            return Returned();
    }
    return result; // Failed, Problem::ConnectionLost
}

CWorker::CItemResult CWorker::ExecuteOnce(const CQueueItem& item)
{
    switch (item.type)
    {
    case ItemType::ExploreRemote:
        return ExploreRemote(item);
    case ItemType::ExploreLocal:
        return ExploreLocal(item);
    case ItemType::Download:
        return Download(item);
    case ItemType::Upload:
        return Upload(item);
    case ItemType::MakeDirLocal:
        return MakeDirLocal(item);
    case ItemType::MakeDirRemote:
        return MakeDirRemote(item);
    case ItemType::DeleteRemoteFile:
        return DeleteRemote(item, false);
    case ItemType::DeleteRemoteDir:
        return DeleteRemote(item, true);
    case ItemType::DeleteLocalFile:
        return DeleteLocal(item, false);
    case ItemType::DeleteLocalDir:
        return DeleteLocal(item, true);
    }
    return Failure(Problem::Other, std::wstring());
}

// ---- connection -----------------------------------------------------------------------------------------------

HostKeyDecision CWorker::VerifyHostKey(const std::string&, int, const std::string&, const std::string& fingerprintSha256,
                                       bool)
{
    // Step 4a: trust exactly the key the panel accepted, ask about nothing else (4b publishes an interaction).
    return fingerprintSha256 == host_.ConnectParams().hostKeyFingerprint ? HostKeyDecision::AcceptOnce
                                                                         : HostKeyDecision::Reject;
}

bool CWorker::Prompt(PromptKind, const std::string&, const std::string&, const std::string&, bool, CSecret&)
{
    return false; // step 4a: a prompt fails the login (the connection error says so); 4b asks the user
}

bool CWorker::Progress(uint64_t done, uint64_t)
{
    if (host_.IsCancelRequested())
        return false;
    if (done > reported_)
    {
        host_.AddTransferred(done - reported_);
        reported_ = done;
    }
    host_.Queue().SetProgress(transferUid_, done);
    return true;
}

void CWorker::BeginTransfer(uint64_t uid, uint64_t startOffset)
{
    transferUid_ = uid;
    transferStart_ = startOffset;
    reported_ = startOffset;
}

// The progress callback is throttled, so the last bytes (and every byte of a small file) are added here.
void CWorker::EndTransfer(uint64_t bytesTransferred)
{
    const uint64_t end = transferStart_ + bytesTransferred;
    if (end > reported_)
    {
        host_.AddTransferred(end - reported_);
        reported_ = end;
    }
}

bool CWorker::EnsureConnected(CItemResult& failure)
{
    if (session_.IsConnected() && session_.HasSftp())
        return true;
    session_.Disconnect(); // a broken or half-open session starts from nothing

    const CPanelConnectParams& params = host_.ConnectParams();
    if (!session_.Connect(ToSshSite(params.site, params.password.Clone(), params.passphrase.Clone())))
    {
        const CSshError error = session_.LastError();
        session_.Disconnect();
        failure = error.kind == SshErrorKind::Cancelled ? Returned()
                                                        : Failure(Problem::ConnectionLost, ToWide(error.message));
        return false;
    }
    return true;
}

CWorker::CItemResult CWorker::FromSessionError(Problem fallback)
{
    const CSshError& error = session_.LastError();
    if (error.kind == SshErrorKind::Cancelled)
        return Returned();
    CItemResult result = Failure(ProblemFromSshError(error, fallback), ToWide(error.message));
    if (!session_.IsConnected())
    {
        result.problem = Problem::ConnectionLost;
        result.connectionLost = true;
    }
    return result;
}

// ---- explore --------------------------------------------------------------------------------------------------

CWorker::CItemResult CWorker::ReplaceWithChildren(const CQueueItem& item, std::vector<CQueueItem> children,
                                                  ItemType afterType)
{
    CQueue& queue = host_.Queue();
    if (host_.Settings().move)
    {
        CQueueItem after; // the directory itself is deleted once everything in it is done
        after.type = afterType;
        after.remotePath = item.remotePath;
        after.localPath = item.localPath;
        after.depth = item.depth;
        queue.ReplaceWithChildren(item.uid, std::move(children), &after);
    }
    else
    {
        queue.ReplaceWithChildren(item.uid, std::move(children));
    }
    return Replaced();
}

CWorker::CItemResult CWorker::ExploreRemote(const CQueueItem& item)
{
    if (item.depth > kMaxExploreDepth)
        return Failure(Problem::SymlinkLoop, std::wstring());
    if (item.localPath.empty())
        return Parked(Problem::InvalidLocalName);

    std::vector<CSftpEntry> entries;
    if (!session_.ListDir(item.remotePath, entries))
        return FromSessionError(Problem::Other);

    std::vector<CQueueItem> children;
    for (const CSftpEntry& entry : entries)
    {
        if (!IsValidRemoteEntryName(entry.rawName) || IsSpecialFile(entry))
            continue;
        const std::string remote = AppendRemoteName(item.remotePath, entry.rawName);
        const bool isDir = EntryIsDirectory(entry);
        uint64_t size = entry.size;
        int64_t mtime = entry.mtime;
        if (!isDir && entry.isLink)
        {
            // the listing describes the link itself; the file it points to has the size that will be transferred
            CSftpEntry target;
            if (session_.Stat(remote, target))
            {
                size = target.size;
                mtime = target.mtime;
            }
            else if (!session_.IsConnected())
            {
                return FromSessionError(Problem::Other);
            }
        }
        AddDownloadItems(children, remote, entry.rawName, isDir, size, mtime, item.localPath, codec_, item.depth + 1);
    }
    return ReplaceWithChildren(item, std::move(children), ItemType::DeleteRemoteDir);
}

CWorker::CItemResult CWorker::ExploreLocal(const CQueueItem& item)
{
    if (item.depth > kMaxExploreDepth)
        return Failure(Problem::SymlinkLoop, std::wstring());
    if (item.remotePath.empty())
        return Failure(Problem::InvalidRemoteName, std::wstring());

    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileW(MakeLocalPath(item.localPath, L"*").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE)
    {
        const DWORD error = GetLastError();
        return Failure(ProblemFromWin32(error, Problem::LocalIoError), WindowsErrorText(error));
    }

    std::vector<CQueueItem> children;
    for (bool more = true; more; more = FindNextFileW(find, &data) != FALSE)
    {
        const std::wstring name = data.cFileName;
        if (name == L"." || name == L"..")
            continue;
        const std::wstring path = MakeLocalPath(item.localPath, name);
        DWORD attributes = data.dwFileAttributes;
        uint64_t size = SizeOf(data.nFileSizeHigh, data.nFileSizeLow);
        FILETIME writeTime = data.ftLastWriteTime;
        if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            FollowReparsePoint(path, attributes, size, writeTime);
        AddUploadItems(children, path, name, (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0, size, UnixTimeOf(writeTime),
                       item.remotePath, codec_, item.depth + 1);
    }
    const DWORD error = GetLastError(); // of the failed FindNextFileW: ERROR_NO_MORE_FILES at the normal end
    FindClose(find);
    if (error != ERROR_NO_MORE_FILES)
        return Failure(Problem::LocalIoError, WindowsErrorText(error));
    return ReplaceWithChildren(item, std::move(children), ItemType::DeleteLocalDir);
}

// ---- transfers ------------------------------------------------------------------------------------------------

// truncate: start the file anew; otherwise keep the content (resume). A missing parent directory is created: its
// MakeDirLocal item may not have run yet (several workers take items in order but not in lock step).
HANDLE CWorker::OpenLocalTarget(const std::wstring& path, bool truncate, DWORD& error)
{
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                               truncate ? CREATE_ALWAYS : OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (h != INVALID_HANDLE_VALUE)
            return h;
        error = GetLastError();
        if (error != ERROR_PATH_NOT_FOUND || !EnsureDirectory(ParentOfLocal(path)))
            break;
    }
    return INVALID_HANDLE_VALUE;
}

CWorker::CItemResult CWorker::Download(const CQueueItem& item)
{
    if (item.localPath.empty())
        return Parked(Problem::InvalidLocalName);
    const COperationSettings& settings = host_.Settings();

    // Step 4b: DecideConflict(policy, item.forceAction, source, Stat of the target) goes here and decides between
    // "transfer from offset", skip, park and rename. For now an existing target is overwritten.
    const uint64_t offset = 0;

    DWORD error = 0;
    HANDLE file = OpenLocalTarget(item.localPath, offset == 0, error);
    if (file == INVALID_HANDLE_VALUE)
        return Failure(ProblemFromWin32(error, Problem::LocalIoError), WindowsErrorText(error));

    BeginTransfer(item.uid, offset);
    uint64_t transferred = 0;
    const bool ok = session_.Download(item.remotePath, file, offset, &transferred, nullptr);
    EndTransfer(transferred);
    if (ok && settings.preserveTime && item.mtime > 0)
    {
        const FILETIME time = UnixTimeToFileTime(item.mtime);
        SetFileTime(file, nullptr, nullptr, &time); // best effort: a file with the wrong time is still a good copy
    }
    CloseHandle(file); // a failed or cancelled download keeps the partial file (4b resumes it)
    if (!ok)
        return FromSessionError(Problem::TransferFailed);

    if (settings.move && !RemoveRemote(item.remotePath, false))
        return FromSessionError(Problem::DeleteFailed);
    return Done();
}

CWorker::CItemResult CWorker::Upload(const CQueueItem& item)
{
    if (item.remotePath.empty())
        return Failure(Problem::InvalidRemoteName, std::wstring());
    const COperationSettings& settings = host_.Settings();

    // Step 4b: the conflict decision (Stat of the remote target) and the safe-upload name go here.
    HANDLE file = CreateFileW(item.localPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        const DWORD error = GetLastError();
        return Failure(ProblemFromWin32(error, Problem::LocalIoError), WindowsErrorText(error));
    }

    BeginTransfer(item.uid, 0);
    uint64_t transferred = 0;
    bool ok = session_.Upload(file, item.remotePath, 0, false, &transferred);
    if (!ok && session_.LastError().kind == SshErrorKind::NotFound && EnsureRemoteDir(ParentOfRemote(item.remotePath), 0))
    {
        // the MakeDirRemote item of the parent may not have run yet
        ok = session_.Upload(file, item.remotePath, 0, false, &transferred);
    }
    EndTransfer(transferred);
    CloseHandle(file);
    if (!ok)
        return FromSessionError(Problem::TransferFailed);

    if (settings.preserveTime && item.mtime > 0)
        session_.SetTimes(item.remotePath, item.mtime, item.mtime); // best effort, like the local side
    if (settings.setUploadPermissions && !session_.Chmod(item.remotePath, settings.uploadPermissions))
        return FromSessionError(Problem::Other);

    if (settings.move)
    {
        DWORD error = 0;
        if (!DeleteLocalFileForce(item.localPath, error))
            return Failure(Problem::DeleteFailed, WindowsErrorText(error));
    }
    return Done();
}

// ---- directories and deletes ----------------------------------------------------------------------------------

CWorker::CItemResult CWorker::MakeDirLocal(const CQueueItem& item)
{
    if (item.localPath.empty())
        return Parked(Problem::InvalidLocalName);
    if (EnsureDirectory(item.localPath))
        return Done();
    DWORD error = GetLastError();
    if (error == ERROR_SUCCESS)
        error = ERROR_ALREADY_EXISTS; // exists, but is a file
    return Failure(Problem::DirCreateFailed, WindowsErrorText(error));
}

bool CWorker::MkdirOrExists(const std::string& path)
{
    if (session_.Mkdir(path))
        return true;
    if (session_.LastError().kind != SshErrorKind::Exists)
        return false;
    // Exists is fine when it is a directory (a merge, or another worker was faster); a file keeps the Exists error
    CSftpEntry entry;
    return session_.Stat(path, entry) && entry.isDir;
}

// Like "mkdir -p": a missing parent is created first.
bool CWorker::EnsureRemoteDir(const std::string& path, int depth)
{
    if (path.empty() || path == "/")
        return true;
    if (MkdirOrExists(path))
        return true;
    if (session_.LastError().kind != SshErrorKind::NotFound || depth >= kMaxExploreDepth)
        return false;
    const std::string parent = ParentOfRemote(path);
    return parent != path && EnsureRemoteDir(parent, depth + 1) && MkdirOrExists(path);
}

CWorker::CItemResult CWorker::MakeDirRemote(const CQueueItem& item)
{
    if (item.remotePath.empty())
        return Failure(Problem::InvalidRemoteName, std::wstring());
    if (EnsureRemoteDir(item.remotePath, 0))
        return Done();
    return FromSessionError(Problem::DirCreateFailed);
}

// A file or a link is removed with Remove, a real directory with Rmdir. Already gone counts as removed.
bool CWorker::RemoveRemote(const std::string& path, bool isDir)
{
    bool ok = false;
    if (!isDir)
    {
        ok = session_.Remove(path);
    }
    else
    {
        CSftpEntry entry;
        const bool isLink = session_.LStat(path, entry) && entry.isLink;
        ok = isLink ? session_.Remove(path) : session_.Rmdir(path);
    }
    return ok || session_.LastError().kind == SshErrorKind::NotFound;
}

CWorker::CItemResult CWorker::DeleteRemote(const CQueueItem& item, bool isDir)
{
    if (RemoveRemote(item.remotePath, isDir))
        return Done();
    return FromSessionError(Problem::DeleteFailed);
}

CWorker::CItemResult CWorker::DeleteLocal(const CQueueItem& item, bool isDir)
{
    DWORD error = 0;
    bool ok = false;
    if (isDir)
    {
        ok = RemoveDirectoryW(item.localPath.c_str()) != FALSE;
        if (!ok)
            error = GetLastError();
    }
    else
    {
        ok = DeleteLocalFileForce(item.localPath, error);
    }
    if (ok || error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        return Done();
    return Failure(Problem::DeleteFailed, WindowsErrorText(error));
}
