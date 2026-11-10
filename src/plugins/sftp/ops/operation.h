// One copy/move run: the queue, the worker threads and the statistics. Sally-free; the UI layer watches it through an
// IOperationObserver and reads State()/Summary()/Statistics().
//
// Life of an operation:  construct (roots go into the queue) -> SetObserver -> Start -> workers run -> the queue has no
// more work (OperationState != Running) or RequestCancel -> workers exit -> OnFinished (once) -> WaitFinished returns.
// The destructor cancels and joins everything, so a COperation can be destroyed at any time, but NOT from inside an
// observer callback (a worker thread would then wait for itself).
//
// Step 4a: the operation is over when the workers run out of work. Items that were parked or failed stay in the queue;
// solving them later (step 4b) will restart workers.
//
// Lock order: operations list -> operation -> queue -> worker. COperation's own mutexes are leaves around plain data;
// none is held while the queue, the observer, a worker or libssh2 is called.
#pragma once

#include <winsock2.h>
#include <windows.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "op_queue.h"
#include "op_settings.h"
#include "op_worker.h"
#include "panel_session.h"

// Told about changes by the worker threads (and by Start/RequestCancel on the calling thread). Implementations must be
// cheap and thread safe: typically they post a message to a window and return. Exceptions are swallowed.
class IOperationObserver
{
public:
    virtual ~IOperationObserver() = default;
    virtual void OnItemChanged(uint64_t uid) = 0; // an item changed state (done, failed, parked, explored)
    virtual void OnProgress() = 0;                // bytes moved; called at most every 100 ms
    virtual void OnStateChanged() = 0;            // the operation state changed (also: cancel requested)
    virtual void OnFinished() = 0;                // once: all workers have exited
};

struct COperationStats
{
    uint64_t bytesTotal = 0; // all transfer items found so far (grows while directories are explored)
    uint64_t bytesDone = 0;
    int filesTotal = 0;
    int filesDone = 0;
    double bytesPerSecond = 0;  // average over the last ~3 seconds
    int64_t etaSeconds = -1;    // -1 = unknown (no speed yet); 0 = nothing left
};

// A selected item to download: the full raw remote path and what the listing said about it.
struct CRemoteSource
{
    std::string path;
    bool isDir = false;
    uint64_t size = 0;
    int64_t mtime = 0;
};

// A selected item to upload.
struct CLocalSource
{
    std::wstring path;
    bool isDir = false;
    uint64_t size = 0;
    int64_t mtime = 0; // Unix seconds
};

class COperation : private IWorkerHost
{
public:
    // The roots are the selected items (see BuildDownloadRoots/BuildUploadRoots). 'params' must be valid for Start to
    // succeed: it carries the site, the secrets and the host key fingerprint the workers trust.
    COperation(COperationSettings settings, CPanelConnectParams params, std::vector<CQueueItem> roots);
    ~COperation();
    COperation(const COperation&) = delete;
    COperation& operator=(const COperation&) = delete;

    // Before Start; the observer must outlive the operation (or at least its callbacks).
    void SetObserver(IOperationObserver* observer) { observer_ = observer; }

    // Creates the workers. False (nothing started) when called twice or the connect params are not valid.
    bool Start();
    // Any thread, returns at once. Running network steps are interrupted; unfinished items stay Waiting.
    void RequestCancel();
    // True when the operation is finished (observer told, workers exiting) within timeoutMs (INFINITE allowed).
    bool WaitFinished(DWORD timeoutMs);

    bool IsFinished() const;
    bool WasCancelled() const { return cancelRequested_.load(); }
    OperationState State() const { return queue_.GetOperationState(); } // Running also after a cancel
    CQueueSummary Summary() const { return queue_.Summary(); }
    COperationStats Statistics() const;

    // For the operation window: the item list and Solve/Retry (the queue is thread safe).
    CQueue& Queue() override { return queue_; }
    const COperationSettings& Settings() const override { return settings_; }

    // Root items for a selection. Names that cannot be used (see AddDownloadItems) are parked by the worker.
    static std::vector<CQueueItem> BuildDownloadRoots(const std::vector<CRemoteSource>& sources,
                                                      const COperationSettings& settings, const CNameCodec& codec);
    static std::vector<CQueueItem> BuildUploadRoots(const std::vector<CLocalSource>& sources,
                                                    const COperationSettings& settings, const CNameCodec& codec);

private:
    // IWorkerHost (called on the worker threads)
    const CPanelConnectParams& ConnectParams() const override { return params_; }
    bool IsCancelRequested() const override { return cancelRequested_.load(); }
    void ItemChanged(uint64_t uid) override;
    void AddTransferred(uint64_t bytes) override;
    void WorkerExited() override;

    void NotifyProgress();
    void NotifyStateIfChanged();
    void ReleaseActive();

    COperationSettings settings_;
    const CPanelConnectParams params_;
    CQueue queue_;
    IOperationObserver* observer_ = nullptr;
    std::atomic<bool> cancelRequested_{false};

    mutable std::mutex mutex_; // guards workers_, started_, finished_, activeWorkers_
    std::condition_variable finishedCv_;
    std::vector<std::unique_ptr<CWorker>> workers_;
    bool started_ = false;
    bool finished_ = false;
    int activeWorkers_ = 1; // 1 = Start itself, until every worker has been started

    // statistics: a leaf lock
    struct CSample
    {
        long long ms; // steady clock
        uint64_t bytes;
    };
    mutable std::mutex statsMutex_;
    uint64_t transferred_ = 0; // bytes moved by all workers so far (retries count again)
    mutable std::deque<CSample> samples_;
    long long lastNotifyMs_ = 0;
    long long lastSampleMs_ = 0;
    std::atomic<int> reportedState_{static_cast<int>(OperationState::Running)};
};
