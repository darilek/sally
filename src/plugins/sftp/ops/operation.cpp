#include "operation.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace
{
constexpr int kMinWorkers = 1;
constexpr int kMaxWorkers = 8;
constexpr long long kNotifyIntervalMs = 100; // OnProgress at most this often
constexpr long long kSpeedWindowMs = 3000;

long long NowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::wstring LastLocalName(const std::wstring& path)
{
    size_t end = path.size();
    while (end > 0 && (path[end - 1] == L'\\' || path[end - 1] == L'/'))
        --end;
    const size_t slash = path.find_last_of(L"\\/", end == 0 ? 0 : end - 1);
    const size_t begin = slash == std::wstring::npos ? 0 : slash + 1;
    return path.substr(begin, end - begin);
}
} // namespace

COperationSettings SettingsFromSite(const CSite& site, TransferDirection direction, bool move)
{
    COperationSettings settings;
    settings.direction = direction;
    settings.move = move;
    settings.preserveTime = site.preserveTime;
    settings.safeUpload = site.safeUpload;
    settings.setUploadPermissions = site.setUploadPermissions;
    settings.uploadPermissions = site.uploadPermissions;
    settings.workers = site.workers;
    return settings;
}

// ---- roots ----------------------------------------------------------------------------------------------------

std::vector<CQueueItem> COperation::BuildDownloadRoots(const std::vector<CRemoteSource>& sources,
                                                       const COperationSettings& settings, const CNameCodec& codec)
{
    std::vector<CQueueItem> items;
    for (const CRemoteSource& source : sources)
    {
        std::string path = source.path;
        while (path.size() > 1 && path.back() == '/')
            path.pop_back();
        AddDownloadItems(items, path, LastRemoteName(path), source.isDir, source.size, source.mtime, settings.localRoot,
                         codec, 0);
    }
    return items;
}

std::vector<CQueueItem> COperation::BuildUploadRoots(const std::vector<CLocalSource>& sources,
                                                     const COperationSettings& settings, const CNameCodec& codec)
{
    std::vector<CQueueItem> items;
    for (const CLocalSource& source : sources)
        AddUploadItems(items, source.path, LastLocalName(source.path), source.isDir, source.size, source.mtime,
                       settings.remoteRoot, codec, 0);
    return items;
}

// ---- life -----------------------------------------------------------------------------------------------------

COperation::COperation(COperationSettings settings, CPanelConnectParams params, std::vector<CQueueItem> roots)
    : settings_(std::move(settings)), params_(std::move(params))
{
    settings_.workers = (std::min)((std::max)(settings_.workers, kMinWorkers), kMaxWorkers);
    queue_.AddRootItems(std::move(roots));
}

COperation::~COperation()
{
    RequestCancel();
    std::vector<std::unique_ptr<CWorker>> workers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        workers.swap(workers_);
    }
    workers.clear(); // ~CWorker joins the thread; no lock is held, the threads may still need mutex_ to finish
}

bool COperation::Start()
{
    std::vector<CWorker*> toStart;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (started_ || !params_.valid)
            return false;
        started_ = true;
        for (int i = 0; i < settings_.workers; ++i)
        {
            workers_.push_back(std::unique_ptr<CWorker>(new CWorker(*this, i))); // not make_unique: it cannot see the private base
            toStart.push_back(workers_.back().get());
        }
    }
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        samples_.push_back({NowMs(), 0});
        lastSampleMs_ = samples_.back().ms;
    }

    bool failed = false;
    for (CWorker* worker : toStart)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++activeWorkers_;
        }
        try
        {
            worker->Start();
        }
        catch (...)
        {
            ReleaseActive(); // this worker has no thread
            failed = true;
            break;
        }
    }
    if (failed)
        RequestCancel(); // the workers that did start must not run half an operation
    ReleaseActive();     // Start's own count: from here on the last exiting worker finishes the operation
    return !failed;
}

void COperation::RequestCancel()
{
    if (cancelRequested_.exchange(true))
        return;
    std::vector<CWorker*> workers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const std::unique_ptr<CWorker>& worker : workers_)
            workers.push_back(worker.get());
    }
    queue_.WakeAll();
    for (CWorker* worker : workers)
        worker->Cancel();
    try
    {
        if (observer_ != nullptr)
            observer_->OnStateChanged();
    }
    catch (...)
    {
    }
}

bool COperation::WaitFinished(DWORD timeoutMs)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (timeoutMs == INFINITE)
    {
        finishedCv_.wait(lock, [this] { return finished_; });
        return true;
    }
    return finishedCv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] { return finished_; });
}

bool COperation::IsFinished() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return finished_;
}

// Called by every worker thread as its last act, and once by Start. The last one finishes the operation.
void COperation::ReleaseActive()
{
    bool last = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last = --activeWorkers_ == 0;
    }
    if (!last)
        return;
    NotifyStateIfChanged();
    try
    {
        if (observer_ != nullptr)
            observer_->OnFinished();
    }
    catch (...)
    {
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        finished_ = true; // after OnFinished: whoever waits sees an operation whose observer was told
    }
    finishedCv_.notify_all();
}

// ---- IWorkerHost ----------------------------------------------------------------------------------------------

void COperation::WorkerExited()
{
    ReleaseActive();
}

void COperation::ItemChanged(uint64_t uid)
{
    try
    {
        if (observer_ != nullptr)
            observer_->OnItemChanged(uid);
    }
    catch (...)
    {
    }
    NotifyStateIfChanged();
}

void COperation::NotifyStateIfChanged()
{
    const int now = static_cast<int>(queue_.GetOperationState());
    if (reportedState_.exchange(now) == now)
        return;
    try
    {
        if (observer_ != nullptr)
            observer_->OnStateChanged();
    }
    catch (...)
    {
    }
}

void COperation::AddTransferred(uint64_t bytes)
{
    const long long now = NowMs();
    bool notify = false;
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        transferred_ += bytes;
        if (now - lastSampleMs_ >= kNotifyIntervalMs)
        {
            samples_.push_back({now, transferred_});
            lastSampleMs_ = now;
        }
        while (samples_.size() >= 2 && samples_[1].ms <= now - kSpeedWindowMs)
            samples_.pop_front(); // keeps one sample at or before the start of the window as the baseline
        if (now - lastNotifyMs_ >= kNotifyIntervalMs)
        {
            lastNotifyMs_ = now;
            notify = true;
        }
    }
    if (notify)
        NotifyProgress();
}

void COperation::NotifyProgress()
{
    try
    {
        if (observer_ != nullptr)
            observer_->OnProgress();
    }
    catch (...)
    {
    }
}

COperationStats COperation::Statistics() const
{
    const CQueueSummary summary = queue_.Summary(); // before the stats lock: the queue lock is taken inside
    COperationStats stats;
    stats.bytesTotal = summary.totals.bytesTotal;
    stats.bytesDone = summary.totals.bytesDone;
    stats.filesTotal = summary.totals.filesTotal;
    stats.filesDone = summary.totals.filesDone;

    const long long now = NowMs();
    std::lock_guard<std::mutex> lock(statsMutex_);
    while (samples_.size() >= 2 && samples_[1].ms <= now - kSpeedWindowMs)
        samples_.pop_front();
    if (!samples_.empty())
    {
        const CSample& baseline = samples_.front();
        const long long span = now - baseline.ms;
        if (span > 0)
            stats.bytesPerSecond = static_cast<double>(transferred_ - baseline.bytes) * 1000.0 / static_cast<double>(span);
    }
    if (stats.bytesPerSecond > 0)
    {
        const uint64_t left = stats.bytesTotal > stats.bytesDone ? stats.bytesTotal - stats.bytesDone : 0;
        stats.etaSeconds = static_cast<int64_t>(static_cast<double>(left) / stats.bytesPerSecond);
    }
    return stats;
}
