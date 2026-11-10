// Tests for ops/COperation and CWorker against the Docker test server: whole copy/move runs with real worker threads.
#include "test_support.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <thread>

#include "listing_format.h"
#include "operation.h"
#include "ssh_session.h"

namespace
{
namespace fs = std::filesystem;

// Accepts any host key once; remembers the fingerprint so the test can hand it to the workers.
class CAcceptAll : public ISshCallbacks
{
public:
    HostKeyDecision VerifyHostKey(const std::string&, int, const std::string&, const std::string&, bool) override
    {
        return HostKeyDecision::AcceptOnce;
    }
    bool Prompt(PromptKind, const std::string&, const std::string&, const std::string&, bool, CSecret&) override
    {
        return false;
    }
    bool Progress(uint64_t, uint64_t) override { return true; }
};

CSite TestSite()
{
    CSite site;
    site.name = L"test";
    site.host = L"127.0.0.1";
    site.port = sftptest::kPort;
    site.user = L"test";
    site.timeoutSeconds = 20;
    return site;
}

// Counts what the operation tells (all calls come from worker threads).
class CTestObserver : public IOperationObserver
{
public:
    std::atomic<int> items{0};
    std::atomic<int> progress{0};
    std::atomic<int> states{0};
    std::atomic<int> finished{0};
    void OnItemChanged(uint64_t) override { ++items; }
    void OnProgress() override { ++progress; }
    void OnStateChanged() override { ++states; }
    void OnFinished() override { ++finished; }
};

DWORD HandleCount()
{
    DWORD n = 0;
    GetProcessHandleCount(GetCurrentProcess(), &n);
    return n;
}

int64_t UnixMtimeOfLocal(const fs::path& path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
        return -1;
    ULARGE_INTEGER t;
    t.LowPart = data.ftLastWriteTime.dwLowDateTime;
    t.HighPart = data.ftLastWriteTime.dwHighDateTime;
    return (static_cast<int64_t>(t.QuadPart) - 116444736000000000LL) / 10000000;
}

std::string ReadAll(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteAll(const fs::path& path, const std::string& content)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << content;
}

std::string RandomBytes(size_t size, unsigned seed)
{
    std::string bytes(size, '\0');
    unsigned state = seed * 2654435761u + 1;
    for (char& c : bytes)
    {
        state = state * 1664525u + 1013904223u;
        c = static_cast<char>(state >> 24);
    }
    return bytes;
}

class OperationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        REQUIRE_SERVER();
        static std::atomic<int> counter{0};
        const std::string unique = std::to_string(GetCurrentProcessId()) + "_" + std::to_string(counter++);
        remoteDir_ = "upload/op_" + unique;
        local_ = fs::temp_directory_path() / ("sally_op_" + unique);
        fs::remove_all(local_);
        fs::create_directories(local_);
        ASSERT_TRUE(admin_.Connect(Admin())) << admin_.LastError().message;
        ASSERT_TRUE(admin_.Mkdir(remoteDir_)) << admin_.LastError().message;
    }

    void TearDown() override
    {
        if (admin_.IsConnected())
            RemoveRemoteTree(remoteDir_);
        std::error_code ignored;
        fs::remove_all(local_, ignored);
    }

    CSshSite Admin() const
    {
        return ToSshSite(TestSite(), CSecret(sftptest::kPassword), CSecret());
    }

    // The connect parameters a panel session would hand over.
    CPanelConnectParams Params()
    {
        CPanelConnectParams params;
        params.site = TestSite();
        params.password = CSecret(sftptest::kPassword);
        params.hostKeyFingerprint = admin_.HostKeyFingerprint();
        params.hostKeyType = admin_.HostKeyType();
        params.valid = true;
        return params;
    }

    COperationSettings Settings(TransferDirection direction, bool move = false, int workers = 2)
    {
        COperationSettings settings = SettingsFromSite(TestSite(), direction, move);
        settings.workers = workers;
        settings.localRoot = local_.wstring();
        settings.remoteRoot = remoteDir_;
        return settings;
    }

    static CRemoteSource RemoteSrc(const std::string& path, bool isDir, uint64_t size = 0, int64_t mtime = 0)
    {
        CRemoteSource s;
        s.path = path;
        s.isDir = isDir;
        s.size = size;
        s.mtime = mtime;
        return s;
    }

    static CLocalSource LocalSrc(const fs::path& path, bool isDir)
    {
        CLocalSource s;
        s.path = path.wstring();
        s.isDir = isDir;
        return s;
    }

    std::unique_ptr<COperation> MakeDownload(const std::vector<CRemoteSource>& sources, bool move = false, int workers = 2)
    {
        const COperationSettings settings = Settings(TransferDirection::Download, move, workers);
        return std::make_unique<COperation>(
            settings, Params(), COperation::BuildDownloadRoots(sources, settings, CNameCodec::Auto()));
    }

    std::unique_ptr<COperation> MakeUpload(const std::vector<CLocalSource>& sources, const std::string& remoteRoot,
                                           bool move = false, int workers = 2)
    {
        COperationSettings settings = Settings(TransferDirection::Upload, move, workers);
        settings.remoteRoot = remoteRoot;
        return std::make_unique<COperation>(
            settings, Params(), COperation::BuildUploadRoots(sources, settings, CNameCodec::Auto()));
    }

    // Starts the operation and waits for the end. False on a timeout (the operation is cancelled then).
    static bool Run(COperation& op, CTestObserver& observer, DWORD timeoutMs = 120000)
    {
        op.SetObserver(&observer);
        EXPECT_TRUE(op.Start());
        if (op.WaitFinished(timeoutMs))
            return true;
        op.RequestCancel();
        op.WaitFinished(20000);
        return false;
    }

    void PutRemoteFile(const std::string& remotePath, const std::string& content)
    {
        const fs::path temp = local_ / "put.tmp";
        WriteAll(temp, content);
        HANDLE h = CreateFileW(temp.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        ASSERT_NE(h, INVALID_HANDLE_VALUE);
        uint64_t n = 0;
        EXPECT_TRUE(admin_.Upload(h, remotePath, 0, false, &n)) << admin_.LastError().message;
        CloseHandle(h);
        fs::remove(temp);
    }

    // Reads a remote file through a separate session (the reference for the compare).
    std::string GetRemoteFile(const std::string& remotePath)
    {
        const fs::path temp = local_ / "get.tmp";
        HANDLE h = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        EXPECT_NE(h, INVALID_HANDLE_VALUE);
        uint64_t n = 0;
        EXPECT_TRUE(admin_.Download(remotePath, h, 0, &n, nullptr)) << admin_.LastError().message;
        CloseHandle(h);
        const std::string content = ReadAll(temp);
        fs::remove(temp);
        return content;
    }

    bool RemoteExists(const std::string& path)
    {
        CSftpEntry e;
        return admin_.LStat(path, e);
    }

    void RemoveRemoteTree(const std::string& path)
    {
        CSftpEntry e;
        if (!admin_.LStat(path, e))
            return;
        if (e.isLink || !e.isDir)
        {
            admin_.Remove(path);
            return;
        }
        std::vector<CSftpEntry> entries;
        if (admin_.ListDir(path, entries))
            for (const CSftpEntry& child : entries)
                RemoveRemoteTree(AppendRemoteName(path, child.rawName));
        admin_.Rmdir(path);
    }

    static std::vector<CQueueItem> ItemsWithProblem(COperation& op, Problem problem)
    {
        std::vector<CQueueItem> result;
        for (const CQueueItem& item : op.Queue().Items(true))
            if (item.problem == problem)
                result.push_back(item);
        return result;
    }

    CAcceptAll acceptAll_;
    CSshSession admin_{&acceptAll_, nullptr};
    std::string remoteDir_;
    fs::path local_;
};
} // namespace

TEST_F(OperationTest, DownloadsANestedTreeWithContentAndTime)
{
    auto op = MakeDownload({RemoteSrc("fixtures/nested", true), RemoteSrc("fixtures/small", true),
                            RemoteSrc("fixtures/empty", true)});
    CTestObserver observer;
    ASSERT_TRUE(Run(*op, observer));

    EXPECT_EQ(op->State(), OperationState::Finished);
    EXPECT_TRUE(op->IsFinished());
    EXPECT_FALSE(op->WasCancelled());
    EXPECT_EQ(observer.finished.load(), 1);
    EXPECT_GT(observer.items.load(), 0);

    EXPECT_EQ(ReadAll(local_ / "nested" / "a" / "b" / "c" / "deep.txt"), "deep\n");
    EXPECT_EQ(ReadAll(local_ / "small" / "hello.txt"), "hello\n");
    EXPECT_TRUE(fs::is_directory(local_ / "empty"));
    EXPECT_TRUE(fs::is_directory(local_ / "nested" / "a" / "b"));

    for (const char* name : {"random1k.bin", "random1m.bin", "hello.txt"})
    {
        const std::string remote = std::string("fixtures/small/") + name;
        EXPECT_EQ(ReadAll(local_ / "small" / name), GetRemoteFile(remote)) << name;
        CSftpEntry e;
        ASSERT_TRUE(admin_.Stat(remote, e));
        EXPECT_EQ(UnixMtimeOfLocal(local_ / "small" / name), e.mtime) << name;
    }

    const COperationStats stats = op->Statistics();
    EXPECT_EQ(stats.filesTotal, stats.filesDone);
    EXPECT_GE(stats.bytesTotal, 1048576u);
    EXPECT_EQ(stats.bytesTotal, stats.bytesDone);
}

TEST_F(OperationTest, DownloadFollowsSymlinks)
{
    auto op = MakeDownload({RemoteSrc("fixtures/dirlink", true), RemoteSrc("fixtures/filelink", false, 6)});
    CTestObserver observer;
    ASSERT_TRUE(Run(*op, observer));
    EXPECT_EQ(op->State(), OperationState::Finished);
    EXPECT_EQ(ReadAll(local_ / "dirlink" / "a" / "b" / "c" / "deep.txt"), "deep\n");
    EXPECT_EQ(ReadAll(local_ / "filelink"), "hello\n");
}

TEST_F(OperationTest, UploadsANestedTreeIntoAMissingFolder)
{
    const fs::path src = local_ / "src" / "tree";
    const std::string big = RandomBytes(700 * 1024, 7);
    WriteAll(src / "a.txt", "alpha\n");
    WriteAll(src / "sub" / "b.bin", big);
    WriteAll(src / "sub" / "deeper" / "c.txt", "gamma\n");
    fs::create_directories(src / "empty");
    const int64_t when = 1577836800; // 2020-01-01
    {
        HANDLE h = CreateFileW((src / "a.txt").c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0,
                               nullptr);
        ASSERT_NE(h, INVALID_HANDLE_VALUE);
        const FILETIME ft = UnixTimeToFileTime(when);
        ASSERT_TRUE(SetFileTime(h, nullptr, nullptr, &ft));
        CloseHandle(h);
    }

    const std::string target = remoteDir_ + "/new/target"; // "new" does not exist yet
    auto op = MakeUpload({LocalSrc(src, true)}, target);
    CTestObserver observer;
    ASSERT_TRUE(Run(*op, observer));
    EXPECT_EQ(op->State(), OperationState::Finished) << op->Queue().Items(true).size() << " items need a look";
    EXPECT_EQ(observer.finished.load(), 1);

    EXPECT_EQ(GetRemoteFile(target + "/tree/a.txt"), "alpha\n");
    EXPECT_EQ(GetRemoteFile(target + "/tree/sub/b.bin"), big);
    EXPECT_EQ(GetRemoteFile(target + "/tree/sub/deeper/c.txt"), "gamma\n");
    CSftpEntry e;
    ASSERT_TRUE(admin_.Stat(target + "/tree/empty", e));
    EXPECT_TRUE(e.isDir);
    ASSERT_TRUE(admin_.Stat(target + "/tree/a.txt", e));
    EXPECT_EQ(e.mtime, when);
    EXPECT_TRUE(fs::exists(src / "a.txt")); // a copy leaves the source alone
}

TEST_F(OperationTest, UploadSetsPermissionsWhenAsked)
{
    WriteAll(local_ / "src" / "p.txt", "perm\n");
    COperationSettings settings = Settings(TransferDirection::Upload);
    settings.setUploadPermissions = true;
    settings.uploadPermissions = 0600;
    auto op = std::make_unique<COperation>(
        settings, Params(), COperation::BuildUploadRoots({LocalSrc(local_ / "src" / "p.txt", false)}, settings, CNameCodec::Auto()));
    CTestObserver observer;
    ASSERT_TRUE(Run(*op, observer));
    EXPECT_EQ(op->State(), OperationState::Finished);
    CSftpEntry e;
    ASSERT_TRUE(admin_.Stat(remoteDir_ + "/p.txt", e));
    EXPECT_EQ(e.permissions & 07777, 0600u);
}

TEST_F(OperationTest, MoveDownRemovesTheRemoteSources)
{
    ASSERT_TRUE(admin_.Mkdir(remoteDir_ + "/tree"));
    ASSERT_TRUE(admin_.Mkdir(remoteDir_ + "/tree/sub"));
    ASSERT_TRUE(admin_.Mkdir(remoteDir_ + "/tree/emptydir"));
    PutRemoteFile(remoteDir_ + "/tree/one.txt", "one\n");
    PutRemoteFile(remoteDir_ + "/tree/sub/two.txt", "two\n");

    auto op = MakeDownload({RemoteSrc(remoteDir_ + "/tree", true)}, true);
    CTestObserver observer;
    ASSERT_TRUE(Run(*op, observer));
    EXPECT_EQ(op->State(), OperationState::Finished) << op->Queue().Items(true).size() << " items need a look";

    EXPECT_EQ(ReadAll(local_ / "tree" / "one.txt"), "one\n");
    EXPECT_EQ(ReadAll(local_ / "tree" / "sub" / "two.txt"), "two\n");
    EXPECT_TRUE(fs::is_directory(local_ / "tree" / "emptydir"));
    EXPECT_FALSE(RemoteExists(remoteDir_ + "/tree/one.txt"));
    EXPECT_FALSE(RemoteExists(remoteDir_ + "/tree/sub/two.txt"));
    EXPECT_FALSE(RemoteExists(remoteDir_ + "/tree/sub"));
    EXPECT_FALSE(RemoteExists(remoteDir_ + "/tree/emptydir"));
    EXPECT_FALSE(RemoteExists(remoteDir_ + "/tree"));
}

TEST_F(OperationTest, MoveUpRemovesTheLocalSources)
{
    const fs::path src = local_ / "movesrc" / "tree";
    WriteAll(src / "one.txt", "one\n");
    WriteAll(src / "sub" / "two.txt", "two\n");
    fs::create_directories(src / "emptydir");
    SetFileAttributesW((src / "one.txt").c_str(), FILE_ATTRIBUTE_READONLY); // a read-only file must go too

    auto op = MakeUpload({LocalSrc(src, true)}, remoteDir_, true);
    CTestObserver observer;
    ASSERT_TRUE(Run(*op, observer));
    EXPECT_EQ(op->State(), OperationState::Finished) << op->Queue().Items(true).size() << " items need a look";

    EXPECT_EQ(GetRemoteFile(remoteDir_ + "/tree/one.txt"), "one\n");
    EXPECT_EQ(GetRemoteFile(remoteDir_ + "/tree/sub/two.txt"), "two\n");
    CSftpEntry e;
    EXPECT_TRUE(admin_.Stat(remoteDir_ + "/tree/emptydir", e));
    EXPECT_FALSE(fs::exists(src));
    EXPECT_TRUE(fs::exists(local_ / "movesrc")); // only the selected folder is the source
}

TEST_F(OperationTest, FiveThousandFilesWithFourWorkers)
{
    auto op = MakeDownload({RemoteSrc("fixtures/many", true)}, false, 4);
    CTestObserver observer;
    ASSERT_TRUE(Run(*op, observer, 180000));
    EXPECT_EQ(op->State(), OperationState::Finished);

    size_t count = 0;
    for (const auto& entry : fs::directory_iterator(local_ / "many"))
    {
        (void)entry;
        ++count;
    }
    EXPECT_EQ(count, 5000u);
    EXPECT_EQ(op->Summary().totals.filesDone, 5000);
    EXPECT_EQ(observer.finished.load(), 1);
}

TEST_F(OperationTest, CancelReturnsPromptlyAndJoinsTheThreads)
{
    const DWORD handlesBefore = HandleCount();
    {
        // 5 GB sparse file: the download cannot finish within the test
        auto op = MakeDownload({RemoteSrc("fixtures/sparse5g.bin", false, 5ull << 30)}, false, 2);
        CTestObserver observer;
        op->SetObserver(&observer);
        ASSERT_TRUE(op->Start());
        for (int i = 0; i < 300 && observer.progress.load() == 0; ++i)
            Sleep(20);
        ASSERT_GT(observer.progress.load(), 0) << "the transfer did not start";

        const auto start = std::chrono::steady_clock::now();
        op->RequestCancel();
        EXPECT_TRUE(op->WaitFinished(10000));
        EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(5));

        EXPECT_TRUE(op->WasCancelled());
        EXPECT_EQ(observer.finished.load(), 1);
        EXPECT_EQ(op->State(), OperationState::Running); // the item went back to Waiting, it was not lost
        const std::vector<CQueueItem> items = op->Queue().Items(false);
        ASSERT_EQ(items.size(), 1u);
        EXPECT_EQ(items[0].state, ItemState::Waiting);
        EXPECT_GT(items[0].bytesDone, 0u); // the partial size is kept for a later resume
        EXPECT_TRUE(fs::exists(local_ / "sparse5g.bin"));
    }
    // sockets, threads and events are closed again (a little slack for lazily created process-wide handles)
    EXPECT_LE(HandleCount(), handlesBefore + 2);
}

TEST_F(OperationTest, DoesNotLeakHandles)
{
    auto runOnce = [this]
    {
        auto op = MakeDownload({RemoteSrc("fixtures/small", true)});
        CTestObserver observer;
        EXPECT_TRUE(Run(*op, observer));
        EXPECT_EQ(op->State(), OperationState::Finished);
    };
    runOnce(); // the first run creates process-wide state
    const DWORD before = HandleCount();
    for (int i = 0; i < 3; ++i)
        runOnce();
    EXPECT_LE(HandleCount(), before + 2);
}

TEST_F(OperationTest, ParksANameThatIsNotAValidWindowsName)
{
    ASSERT_TRUE(admin_.Mkdir(remoteDir_ + "/names"));
    PutRemoteFile(remoteDir_ + "/names/good.txt", "good\n");
    PutRemoteFile(remoteDir_ + "/names/a:b", "bad\n");
    ASSERT_TRUE(admin_.Mkdir(remoteDir_ + "/names/conx.d"));
    ASSERT_TRUE(admin_.Mkdir(remoteDir_ + "/names/aux")); // reserved device name, a directory this time
    PutRemoteFile(remoteDir_ + "/names/aux/inside.txt", "inside\n");

    auto op = MakeDownload({RemoteSrc(remoteDir_ + "/names", true)});
    CTestObserver observer;
    ASSERT_TRUE(Run(*op, observer));

    EXPECT_EQ(op->State(), OperationState::NeedsUser);
    EXPECT_EQ(ReadAll(local_ / "names" / "good.txt"), "good\n");
    EXPECT_TRUE(fs::is_directory(local_ / "names" / "conx.d"));
    EXPECT_FALSE(fs::exists(local_ / "names" / "aux"));

    const std::vector<CQueueItem> parked = ItemsWithProblem(*op, Problem::InvalidLocalName);
    ASSERT_EQ(parked.size(), 2u);
    for (const CQueueItem& item : parked)
        EXPECT_EQ(item.state, ItemState::UserInputNeeded);
    const CQueueSummary summary = op->Summary();
    EXPECT_EQ(summary.counters.userInput, 2);
    EXPECT_EQ(summary.totals.filesDone, 1);

    // the user skips both: nothing is left, the run counts as "finished with skips"
    EXPECT_EQ(op->Queue().Solve(parked[0].uid, SolveAction::Skip, L"", true), 2);
    EXPECT_EQ(op->State(), OperationState::FinishedWithSkips);
}

TEST_F(OperationTest, AMissingSourceFailsOnlyThatItem)
{
    PutRemoteFile(remoteDir_ + "/there.txt", "x\n");
    auto op = MakeDownload({RemoteSrc(remoteDir_ + "/there.txt", false, 2), RemoteSrc(remoteDir_ + "/gone.txt", false, 5)});
    CTestObserver observer;
    ASSERT_TRUE(Run(*op, observer));
    EXPECT_EQ(op->State(), OperationState::FinishedWithErrors);
    EXPECT_EQ(ReadAll(local_ / "there.txt"), "x\n");
    const std::vector<CQueueItem> failed = ItemsWithProblem(*op, Problem::NotFound);
    ASSERT_EQ(failed.size(), 1u);
    EXPECT_EQ(failed[0].state, ItemState::Failed);
    EXPECT_FALSE(failed[0].errorText.empty());
}

TEST_F(OperationTest, AMissingLocalSourceFailsOnlyThatItem)
{
    WriteAll(local_ / "src" / "here.txt", "x\n");
    auto op = MakeUpload({LocalSrc(local_ / "src" / "here.txt", false), LocalSrc(local_ / "src" / "nope.txt", false)},
                         remoteDir_);
    CTestObserver observer;
    ASSERT_TRUE(Run(*op, observer));
    EXPECT_EQ(op->State(), OperationState::FinishedWithErrors);
    EXPECT_EQ(GetRemoteFile(remoteDir_ + "/here.txt"), "x\n");
    EXPECT_EQ(ItemsWithProblem(*op, Problem::NotFound).size(), 1u);
}

TEST_F(OperationTest, TooDeepExploreItemsAreTakenForALoop)
{
    CQueueItem deep;
    deep.type = ItemType::ExploreRemote;
    deep.remotePath = "fixtures/small";
    deep.localPath = (local_ / "deep").wstring();
    deep.depth = kMaxExploreDepth + 1;
    COperation op(Settings(TransferDirection::Download), Params(), {deep});
    CTestObserver observer;
    ASSERT_TRUE(Run(op, observer));
    EXPECT_EQ(op.State(), OperationState::FinishedWithErrors);
    EXPECT_EQ(ItemsWithProblem(op, Problem::SymlinkLoop).size(), 1u);
}

TEST_F(OperationTest, StartNeedsValidParamsAndWorksOnce)
{
    COperation bad(Settings(TransferDirection::Download), CPanelConnectParams(), {});
    EXPECT_FALSE(bad.Start());

    auto op = MakeDownload({});
    CTestObserver observer;
    op->SetObserver(&observer);
    EXPECT_TRUE(op->Start());
    EXPECT_FALSE(op->Start());
    EXPECT_TRUE(op->WaitFinished(20000));
    EXPECT_EQ(op->State(), OperationState::Finished); // an empty queue is finished at once
    EXPECT_EQ(observer.finished.load(), 1);
}

TEST_F(OperationTest, ARejectedHostKeyFailsTheItemsWithConnectionLost)
{
    CPanelConnectParams params = Params();
    params.hostKeyFingerprint = "SHA256:not-the-key";
    const COperationSettings settings = Settings(TransferDirection::Download, false, 1);
    COperation op(settings, std::move(params),
                  COperation::BuildDownloadRoots({RemoteSrc("fixtures/small/hello.txt", false, 6)}, settings,
                                                 CNameCodec::Auto()));
    CTestObserver observer;
    ASSERT_TRUE(Run(op, observer));
    EXPECT_EQ(op.State(), OperationState::FinishedWithErrors);
    EXPECT_EQ(ItemsWithProblem(op, Problem::ConnectionLost).size(), 1u);
    EXPECT_FALSE(fs::exists(local_ / "hello.txt"));
}

TEST_F(OperationTest, DestroyingARunningOperationJoinsEverything)
{
    const DWORD before = HandleCount();
    for (int i = 0; i < 3; ++i)
    {
        auto op = MakeDownload({RemoteSrc("fixtures/many", true)}, false, 4);
        CTestObserver observer;
        op->SetObserver(&observer);
        ASSERT_TRUE(op->Start());
        Sleep(150); // workers are somewhere in connect/explore/transfer
    }                // ~COperation cancels and joins
    EXPECT_LE(HandleCount(), before + 2);
}
