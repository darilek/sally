// Tests for ops/CPanelSession against the Docker test server. The test thread plays the Sally main thread: it submits
// requests, waits on the events and answers interactions.
#include "test_support.h"

#include <windows.h>

#include <chrono>
#include <fstream>
#include <memory>
#include <thread>

#include "panel_session.h"
#include "site.h"

namespace
{
using Clock = std::chrono::steady_clock;

// What the "main thread" answers.
struct CPolicy
{
    HostKeyDecision hostKey = HostKeyDecision::AcceptOnce;
    bool promptOk = true;
    std::string promptAnswer = sftptest::kPassword;
    std::vector<CInteraction> seen;
};

CSite TestSite(int port = sftptest::kPort)
{
    CSite site;
    site.name = L"test";
    site.host = L"127.0.0.1";
    site.port = port;
    site.user = L"test";
    site.timeoutSeconds = 20;
    site.encoding = CSite::Encoding::Auto;
    return site;
}

// Waits for the request like fs/ will: on the done event and the interaction event. False on timeout.
bool Pump(CPanelSession& s, CPanelRequest& req, CPolicy& policy, DWORD timeoutMs = 30000)
{
    const auto end = Clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;)
    {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(end - Clock::now()).count();
        if (left <= 0)
            return req.IsDone();
        HANDLE handles[2] = {req.DoneEvent(), s.InteractionEvent()};
        DWORD r = WaitForMultipleObjects(2, handles, FALSE, static_cast<DWORD>(left));
        if (r == WAIT_OBJECT_0)
            return true;
        if (r != WAIT_OBJECT_0 + 1)
            return req.IsDone();
        CInteraction in;
        if (!s.TryGetInteraction(in))
            continue;
        policy.seen.push_back(in);
        if (in.kind == CInteraction::Kind::HostKey)
            s.AnswerHostKey(policy.hostKey);
        else
            s.AnswerPrompt(policy.promptOk, CSecret(policy.promptAnswer));
    }
}

// Waits until the session thread posts an interaction (without answering it).
bool WaitInteraction(CPanelSession& s, CInteraction& in, DWORD timeoutMs = 10000)
{
    if (WaitForSingleObject(s.InteractionEvent(), timeoutMs) != WAIT_OBJECT_0)
        return false;
    return s.TryGetInteraction(in);
}

DWORD HandleCount()
{
    DWORD n = 0;
    GetProcessHandleCount(GetCurrentProcess(), &n);
    return n;
}

std::string TempFileName(const char* tag)
{
    char dir[MAX_PATH];
    GetTempPathA(MAX_PATH, dir);
    return std::string(dir) + "sally_ps_" + tag + "_" + std::to_string(GetCurrentProcessId());
}

class PanelSessionTest : public ::testing::Test
{
protected:
    void SetUp() override { REQUIRE_SERVER(); }

    // connect (host key accepted once)
    static bool ConnectOk(CPanelSession& s, const CSite& site, CPolicy& policy)
    {
        auto req = s.Connect(site, CSecret(sftptest::kPassword), CSecret());
        return Pump(s, *req, policy) && req->Ok();
    }
};
} // namespace

TEST_F(PanelSessionTest, ConnectListRealPathStat)
{
    CPanelSession s(nullptr);
    CPolicy policy;
    auto connect = s.Connect(TestSite(), CSecret(sftptest::kPassword), CSecret());
    ASSERT_TRUE(Pump(s, *connect, policy));
    ASSERT_TRUE(connect->Ok()) << connect->Error().message;
    ASSERT_EQ(policy.seen.size(), 1u); // unknown host key (no known_hosts)
    EXPECT_EQ(policy.seen[0].kind, CInteraction::Kind::HostKey);
    EXPECT_FALSE(policy.seen[0].changed);
    EXPECT_EQ(policy.seen[0].fingerprint.substr(0, 7), "SHA256:");
    EXPECT_FALSE(policy.seen[0].keyType.empty());
    EXPECT_EQ(policy.seen[0].host, "127.0.0.1");
    EXPECT_EQ(policy.seen[0].port, sftptest::kPort);

    auto home = s.RealPath("");
    ASSERT_TRUE(home->Wait(20000));
    ASSERT_TRUE(home->Ok()) << home->Error().message;
    EXPECT_EQ(home->ResolvedPath().substr(0, 1), "/");

    auto list = s.List("fixtures/small");
    ASSERT_TRUE(list->Wait(20000));
    ASSERT_TRUE(list->Ok()) << list->Error().message;
    EXPECT_EQ(list->ResolvedPath(), home->ResolvedPath() + "/fixtures/small");
    bool found = false;
    for (const CSftpEntry& e : list->Entries())
        found |= e.rawName == "hello.txt" && e.size == 6 && !e.isDir;
    EXPECT_TRUE(found);

    auto stat = s.Stat("fixtures/small/hello.txt");
    ASSERT_TRUE(stat->Wait(20000));
    ASSERT_TRUE(stat->Ok());
    EXPECT_EQ(stat->Attributes().size, 6u);

    auto missing = s.List("fixtures/does-not-exist");
    ASSERT_TRUE(missing->Wait(20000));
    EXPECT_EQ(missing->Error().kind, SshErrorKind::NotFound);
    // a failed request leaves the connection usable
    auto again = s.Stat("fixtures/small/hello.txt");
    ASSERT_TRUE(again->Wait(20000));
    EXPECT_TRUE(again->Ok());
}

TEST_F(PanelSessionTest, ListsFiveThousandEntries)
{
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    auto list = s.List("fixtures/many");
    ASSERT_TRUE(list->Wait(60000));
    ASSERT_TRUE(list->Ok()) << list->Error().message;
    EXPECT_EQ(list->Entries().size(), 5000u);
}

TEST_F(PanelSessionTest, FifoOrder)
{
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    auto a = s.List("fixtures/many");
    auto b = s.RealPath("fixtures");
    auto c = s.Stat("fixtures/small");
    ASSERT_TRUE(c->Wait(60000));
    // when the last one is done, the earlier ones are too
    EXPECT_TRUE(a->IsDone());
    EXPECT_TRUE(b->IsDone());
    EXPECT_TRUE(a->Ok() && b->Ok() && c->Ok());
}

TEST_F(PanelSessionTest, WrongPasswordFailsAndForgetsTheSite)
{
    CPanelSession s(nullptr);
    CPolicy policy;
    auto connect = s.Connect(TestSite(), CSecret("wrong"), CSecret());
    ASSERT_TRUE(Pump(s, *connect, policy));
    EXPECT_EQ(connect->Error().kind, SshErrorKind::AuthFailed);
    // no automatic reconnect with a site whose login failed
    auto list = s.List("");
    ASSERT_TRUE(list->Wait(5000));
    EXPECT_EQ(list->Error().kind, SshErrorKind::ConnectFailed);
}

TEST_F(PanelSessionTest, DisconnectForgetsTheSite)
{
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    auto d = s.Disconnect();
    ASSERT_TRUE(d->Wait(10000));
    EXPECT_TRUE(d->Ok());
    auto list = s.List("");
    ASSERT_TRUE(list->Wait(5000));
    EXPECT_EQ(list->Error().kind, SshErrorKind::ConnectFailed);
    EXPECT_TRUE(ConnectOk(s, TestSite(), policy)); // an explicit Connect works again
}

TEST_F(PanelSessionTest, CancelStalledListingThenReconnects)
{
    sftptest::CStallProxy proxy(sftptest::kPort);
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(proxy.Port()), policy));

    proxy.Stall(true);
    auto list = s.List("fixtures/many");
    EXPECT_FALSE(list->Wait(500)); // stuck: the server "went silent"
    const auto t0 = Clock::now();
    s.Cancel();
    ASSERT_TRUE(list->Wait(3000));
    EXPECT_LT(Clock::now() - t0, std::chrono::seconds(3));
    EXPECT_EQ(list->Error().kind, SshErrorKind::Cancelled);
    EXPECT_TRUE(list->Entries().empty());

    proxy.Stall(false);
    // the next request reconnects by itself; the host key was accepted only once, so it is asked again
    auto next = s.List("fixtures/many");
    ASSERT_TRUE(Pump(s, *next, policy, 60000));
    ASSERT_TRUE(next->Ok()) << next->Error().message;
    EXPECT_EQ(next->Entries().size(), 5000u);
}

TEST_F(PanelSessionTest, RequestQueuedBehindCancelledOneStillRuns)
{
    sftptest::CStallProxy proxy(sftptest::kPort);
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(proxy.Port()), policy));

    proxy.Stall(true);
    auto first = s.List("fixtures/many");
    auto second = s.Stat("fixtures/small/hello.txt");
    EXPECT_FALSE(first->Wait(300));
    s.Cancel();
    ASSERT_TRUE(first->Wait(3000));
    EXPECT_TRUE(first->Cancelled());
    proxy.Stall(false);
    ASSERT_TRUE(Pump(s, *second, policy, 30000)); // reconnects (host key asked again)
    EXPECT_TRUE(second->Ok()) << second->Error().message;
}

TEST_F(PanelSessionTest, CancelWithNothingPendingDoesNotHitTheNextRequest)
{
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    s.Cancel();
    s.Cancel();
    auto list = s.List("fixtures/small");
    ASSERT_TRUE(list->Wait(20000));
    EXPECT_TRUE(list->Ok()) << list->Error().message;
}

TEST_F(PanelSessionTest, SecondCancelEndsTheQueuedRequestOrLetsItRun)
{
    sftptest::CStallProxy proxy(sftptest::kPort);
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(proxy.Port()), policy));
    proxy.Stall(true);
    auto running = s.List("fixtures/many");
    auto queued = s.RealPath("");
    EXPECT_FALSE(running->Wait(200));
    s.Cancel();
    ASSERT_TRUE(running->Wait(3000));
    EXPECT_TRUE(running->Cancelled());
    proxy.Stall(false);
    // the queued request (now running or still queued) ends in a defined state, never hangs
    ASSERT_TRUE(Pump(s, *queued, policy, 30000));
}

TEST_F(PanelSessionTest, HostKeyAccept)
{
    CPanelSession s(nullptr);
    auto req = s.Connect(TestSite(), CSecret(sftptest::kPassword), CSecret());
    CInteraction in;
    ASSERT_TRUE(WaitInteraction(s, in));
    EXPECT_EQ(in.kind, CInteraction::Kind::HostKey);
    EXPECT_FALSE(req->IsDone()); // blocked until the answer
    EXPECT_TRUE(s.AnswerHostKey(HostKeyDecision::AcceptOnce));
    ASSERT_TRUE(req->Wait(20000));
    EXPECT_TRUE(req->Ok()) << req->Error().message;
    EXPECT_FALSE(s.AnswerHostKey(HostKeyDecision::AcceptOnce)); // nothing waits any more
}

TEST_F(PanelSessionTest, HostKeyReject)
{
    CPanelSession s(nullptr);
    auto req = s.Connect(TestSite(), CSecret(sftptest::kPassword), CSecret());
    CInteraction in;
    ASSERT_TRUE(WaitInteraction(s, in));
    s.AnswerHostKey(HostKeyDecision::Reject);
    ASSERT_TRUE(req->Wait(20000));
    EXPECT_EQ(req->Error().kind, SshErrorKind::HostKeyRejected);
}

TEST_F(PanelSessionTest, HostKeyCancelledWhileAsking)
{
    CPanelSession s(nullptr);
    auto req = s.Connect(TestSite(), CSecret(sftptest::kPassword), CSecret());
    CInteraction in;
    ASSERT_TRUE(WaitInteraction(s, in));
    s.Cancel();
    ASSERT_TRUE(req->Wait(3000));
    EXPECT_TRUE(req->Cancelled());
}

TEST_F(PanelSessionTest, ChangedHostKeyIsFlagged)
{
    const std::string file = TempFileName("kh_changed");
    {
        std::ofstream f(file, std::ios::binary | std::ios::trunc);
        f << "[127.0.0.1]:2222 ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIPCXkH/dYSFrBOlTqCsONq3uq3/NVgQ9Ti8sm0cmbPdm\n";
    }
    CKnownHosts hosts(file);
    {
        CPanelSession s(&hosts);
        CPolicy policy;
        policy.hostKey = HostKeyDecision::Reject;
        auto req = s.Connect(TestSite(), CSecret(sftptest::kPassword), CSecret());
        ASSERT_TRUE(Pump(s, *req, policy));
        ASSERT_EQ(policy.seen.size(), 1u);
        EXPECT_TRUE(policy.seen[0].changed);
        EXPECT_EQ(req->Error().kind, SshErrorKind::HostKeyChanged);
    }
    DeleteFileA(file.c_str());
}

TEST_F(PanelSessionTest, AcceptAndSaveThenNoQuestion)
{
    const std::string file = TempFileName("kh_saved");
    DeleteFileA(file.c_str());
    CKnownHosts hosts(file);
    {
        CPanelSession s(&hosts);
        CPolicy policy;
        policy.hostKey = HostKeyDecision::AcceptAndSave;
        ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
        EXPECT_EQ(policy.seen.size(), 1u);
    }
    {
        CPanelSession s(&hosts);
        CPolicy policy;
        ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
        EXPECT_TRUE(policy.seen.empty());
    }
    DeleteFileA(file.c_str());
}

TEST_F(PanelSessionTest, KeyboardInteractivePromptAnswered)
{
    if (!sftptest::ServerReachable(sftptest::kPortKbdInt))
        GTEST_SKIP() << "port 2224 not reachable";
    CPanelSession s(nullptr);
    CPolicy policy;
    auto req = s.Connect(TestSite(sftptest::kPortKbdInt), CSecret(), CSecret()); // no stored password: asks
    ASSERT_TRUE(Pump(s, *req, policy));
    ASSERT_TRUE(req->Ok()) << req->Error().message;
    ASSERT_EQ(policy.seen.size(), 2u);
    EXPECT_EQ(policy.seen[1].kind, CInteraction::Kind::Prompt);
    EXPECT_NE(policy.seen[1].promptText.find("assword"), std::string::npos) << policy.seen[1].promptText;
    EXPECT_FALSE(policy.seen[1].echo);
    EXPECT_NE(policy.seen[1].threadId, std::this_thread::get_id());
    EXPECT_EQ(policy.seen[1].threadId, s.SessionThreadId());
    auto list = s.List("");
    ASSERT_TRUE(list->Wait(20000));
    EXPECT_TRUE(list->Ok());
}

TEST_F(PanelSessionTest, KeyboardInteractivePromptCancelledByUser)
{
    if (!sftptest::ServerReachable(sftptest::kPortKbdInt))
        GTEST_SKIP() << "port 2224 not reachable";
    CPanelSession s(nullptr);
    CPolicy policy;
    policy.promptOk = false;
    auto req = s.Connect(TestSite(sftptest::kPortKbdInt), CSecret(), CSecret());
    ASSERT_TRUE(Pump(s, *req, policy));
    EXPECT_EQ(req->Error().kind, SshErrorKind::AuthFailed);
}

TEST_F(PanelSessionTest, UnansweredInteractionDoesNotHitTheIdleTimeout)
{
    CSite site = TestSite();
    site.timeoutSeconds = 2;
    CPanelSession s(nullptr);
    auto req = s.Connect(site, CSecret(sftptest::kPassword), CSecret());
    CInteraction in;
    ASSERT_TRUE(WaitInteraction(s, in));
    EXPECT_FALSE(req->Wait(4000)); // twice the timeout, still waiting for the user
    EXPECT_TRUE(s.AnswerHostKey(HostKeyDecision::AcceptOnce));
    ASSERT_TRUE(req->Wait(20000));
    EXPECT_TRUE(req->Ok()) << req->Error().message;
}

TEST_F(PanelSessionTest, CloseWhileWaitingForAnInteraction)
{
    auto s = std::make_unique<CPanelSession>(nullptr);
    auto req = s->Connect(TestSite(), CSecret(sftptest::kPassword), CSecret());
    CInteraction in;
    ASSERT_TRUE(WaitInteraction(*s, in));
    const auto t0 = Clock::now();
    s->Close();
    EXPECT_LT(Clock::now() - t0, std::chrono::seconds(3));
    EXPECT_TRUE(req->IsDone());
    EXPECT_TRUE(req->Cancelled());
    s->Close(); // idempotent
    s.reset();
}

TEST_F(PanelSessionTest, CloseWhileARequestRuns)
{
    sftptest::CStallProxy proxy(sftptest::kPort);
    auto s = std::make_unique<CPanelSession>(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(*s, TestSite(proxy.Port()), policy));
    proxy.Stall(true);
    auto list = s->List("fixtures/many");
    auto queued = s->RealPath("");
    EXPECT_FALSE(list->Wait(300));
    const auto t0 = Clock::now();
    s.reset(); // the destructor is Close
    EXPECT_LT(Clock::now() - t0, std::chrono::seconds(3));
    EXPECT_TRUE(list->IsDone());
    EXPECT_TRUE(list->Cancelled());
    EXPECT_TRUE(queued->IsDone());
    EXPECT_TRUE(queued->Cancelled());
}

TEST_F(PanelSessionTest, RequestsAfterCloseFailAtOnce)
{
    CPanelSession s(nullptr);
    s.Close();
    auto req = s.List("");
    EXPECT_TRUE(req->IsDone());
    EXPECT_TRUE(req->Cancelled());
}

TEST_F(PanelSessionTest, CallbacksRunOnTheSessionThread)
{
    CPanelSession s(nullptr);
    EXPECT_NE(s.SessionThreadId(), std::this_thread::get_id());
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    ASSERT_EQ(policy.seen.size(), 1u);
    // the host key callback is called by libssh2 inside Connect: it ran on the session thread
    EXPECT_EQ(policy.seen[0].threadId, s.SessionThreadId());
    EXPECT_NE(policy.seen[0].threadId, std::this_thread::get_id());
}

TEST_F(PanelSessionTest, ManyConnectCloseCyclesDoNotLeakHandles)
{
    auto cycle = [] {
        CPanelSession s(nullptr);
        CPolicy policy;
        auto connect = s.Connect(TestSite(), CSecret(sftptest::kPassword), CSecret());
        EXPECT_TRUE(Pump(s, *connect, policy));
        auto list = s.List("fixtures/small");
        EXPECT_TRUE(list->Wait(20000));
        s.Close();
    };
    cycle(); // warm-up: one-time handles (Winsock, OpenSSL, ...)
    const DWORD before = HandleCount();
    for (int i = 0; i < 50; ++i)
        cycle();
    const DWORD after = HandleCount();
    EXPECT_LE(after, before + 5) << "handles before " << before << ", after " << after;
}

// ---- review fixes: answers, stale interactions, Disconnect, Connect twice, handles --------------------------------

TEST_F(PanelSessionTest, AnswersWithoutAMatchingInteractionAreRejected)
{
    CPanelSession s(nullptr);
    // nothing pending
    EXPECT_FALSE(s.AnswerHostKey(HostKeyDecision::AcceptOnce));
    EXPECT_FALSE(s.AnswerPrompt(true, CSecret("x")));

    // a host key question is pending: a prompt answer is the wrong kind, the host key answer works (once)
    auto req = s.Connect(TestSite(), CSecret(sftptest::kPassword), CSecret());
    CInteraction in;
    ASSERT_TRUE(WaitInteraction(s, in));
    ASSERT_EQ(in.kind, CInteraction::Kind::HostKey);
    EXPECT_FALSE(s.AnswerPrompt(true, CSecret("x")));
    EXPECT_TRUE(s.AnswerHostKey(HostKeyDecision::AcceptOnce));
    EXPECT_FALSE(s.AnswerHostKey(HostKeyDecision::AcceptOnce)); // already answered
    ASSERT_TRUE(req->Wait(20000));
    EXPECT_TRUE(req->Ok()) << req->Error().message;
    EXPECT_FALSE(s.AnswerHostKey(HostKeyDecision::AcceptOnce)); // interaction is gone
}

TEST_F(PanelSessionTest, AnswerAfterAbortIsRejected)
{
    CPanelSession s(nullptr);
    auto req = s.Connect(TestSite(), CSecret(sftptest::kPassword), CSecret());
    CInteraction in;
    ASSERT_TRUE(WaitInteraction(s, in));
    s.Cancel();
    ASSERT_TRUE(req->Wait(10000));
    EXPECT_TRUE(req->Cancelled());
    EXPECT_FALSE(s.AnswerHostKey(HostKeyDecision::AcceptOnce));
    EXPECT_FALSE(s.AnswerPrompt(true, CSecret("x")));
}

TEST_F(PanelSessionTest, CancelledRequestLeavesNoStaleInteraction)
{
    CPanelSession s(nullptr);
    auto req = s.Connect(TestSite(), CSecret(sftptest::kPassword), CSecret());
    // wait until the question is posted, but do not fetch it
    ASSERT_EQ(WaitForSingleObject(s.InteractionEvent(), 10000), WAIT_OBJECT_0);
    s.Cancel();
    CInteraction in;
    EXPECT_FALSE(s.TryGetInteraction(in)); // aborted: no dialog for a dead request
    ASSERT_TRUE(req->Wait(10000));
    EXPECT_TRUE(req->Cancelled());
    EXPECT_FALSE(s.TryGetInteraction(in));
    EXPECT_NE(WaitForSingleObject(s.InteractionEvent(), 0), WAIT_OBJECT_0); // the event is reset too
}

TEST_F(PanelSessionTest, DisconnectWhileARequestRuns)
{
    sftptest::CStallProxy proxy(sftptest::kPort);
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(proxy.Port()), policy));
    proxy.Stall(true);
    auto list = s.List("fixtures/many");
    auto disconnect = s.Disconnect(); // queued behind the stalled list
    EXPECT_FALSE(list->Wait(300));
    s.Cancel(); // the list is cancelled, then Disconnect runs
    ASSERT_TRUE(list->Wait(10000));
    EXPECT_TRUE(list->Cancelled());
    ASSERT_TRUE(disconnect->Wait(10000));
    EXPECT_TRUE(disconnect->Ok());
    proxy.Stall(false);
    auto after = s.List(""); // the site is forgotten: no automatic reconnect
    ASSERT_TRUE(after->Wait(10000));
    EXPECT_FALSE(after->Ok());
}

TEST_F(PanelSessionTest, ConnectTwiceInARow)
{
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy)); // the second Connect replaces the first connection
    auto list = s.List("fixtures/small");
    ASSERT_TRUE(list->Wait(20000));
    EXPECT_TRUE(list->Ok()) << list->Error().message;

    // two Connects queued without waiting in between
    auto c1 = s.Connect(TestSite(), CSecret(sftptest::kPassword), CSecret());
    auto c2 = s.Connect(TestSite(), CSecret(sftptest::kPassword), CSecret());
    ASSERT_TRUE(Pump(s, *c1, policy));
    ASSERT_TRUE(Pump(s, *c2, policy));
    EXPECT_TRUE(c1->Ok()) << c1->Error().message;
    EXPECT_TRUE(c2->Ok()) << c2->Error().message;
}

TEST_F(PanelSessionTest, RequestAfterCloseDoesNotLeakItsEvent)
{
    CPanelSession s(nullptr);
    s.Close();
    { s.List("")->Wait(0); } // warm-up
    const DWORD before = HandleCount();
    for (int i = 0; i < 200; ++i)
    {
        auto req = s.List("");
        EXPECT_TRUE(req->IsDone());
        EXPECT_TRUE(req->Cancelled());
    }
    EXPECT_LE(HandleCount(), before + 2);
}

// ---- CopyConnectParams ----------------------------------------------------------------------------------------------

TEST_F(PanelSessionTest, CopyConnectParamsIsInvalidUntilConnected)
{
    CPanelSession s(nullptr);
    EXPECT_FALSE(s.CopyConnectParams().valid);
    CPolicy policy;
    auto bad = s.Connect(TestSite(), CSecret("wrong"), CSecret());
    ASSERT_TRUE(Pump(s, *bad, policy));
    EXPECT_FALSE(bad->Ok());
    EXPECT_FALSE(s.CopyConnectParams().valid); // a failed connect forgets the site
}

TEST_F(PanelSessionTest, CopyConnectParamsRecordsAcceptedHostKey)
{
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    ASSERT_EQ(policy.seen.size(), 1u);

    CPanelConnectParams params = s.CopyConnectParams();
    ASSERT_TRUE(params.valid);
    EXPECT_EQ(params.hostKeyFingerprint, policy.seen[0].fingerprint); // the key the user accepted
    EXPECT_EQ(params.hostKeyType, policy.seen[0].keyType);
    EXPECT_EQ(params.site.host, L"127.0.0.1");
    EXPECT_EQ(params.site.port, sftptest::kPort);
    EXPECT_EQ(params.password.ToStdString(), sftptest::kPassword);
    EXPECT_TRUE(params.passphrase.empty());

    auto d = s.Disconnect();
    ASSERT_TRUE(d->Wait(10000));
    EXPECT_FALSE(s.CopyConnectParams().valid);
    EXPECT_EQ(params.password.ToStdString(), sftptest::kPassword); // the copy outlives the session's secrets
}

TEST_F(PanelSessionTest, CopyConnectParamsSecretsAreIndependentCopies)
{
    CPanelSession s(nullptr);
    CPolicy policy;
    auto req = s.Connect(TestSite(), CSecret(sftptest::kPassword), CSecret("phrase"));
    ASSERT_TRUE(Pump(s, *req, policy));
    ASSERT_TRUE(req->Ok()) << req->Error().message;

    CPanelConnectParams first = s.CopyConnectParams();
    ASSERT_TRUE(first.valid);
    first.password.clear(); // wiping one copy must not touch the session or another copy
    first.passphrase.assign("changed", 7);
    CPanelConnectParams second = s.CopyConnectParams();
    EXPECT_EQ(second.password.ToStdString(), sftptest::kPassword);
    EXPECT_EQ(second.passphrase.ToStdString(), "phrase");
    EXPECT_NE(second.password.data(), first.password.data());
}

TEST_F(PanelSessionTest, CopyConnectParamsHasTheFingerprintOfAnAlreadyTrustedKey)
{
    const std::string file = TempFileName("kh_params");
    DeleteFileA(file.c_str());
    CKnownHosts hosts(file);
    std::string accepted;
    {
        CPanelSession s(&hosts);
        CPolicy policy;
        policy.hostKey = HostKeyDecision::AcceptAndSave;
        ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
        accepted = s.CopyConnectParams().hostKeyFingerprint;
        EXPECT_EQ(accepted, policy.seen[0].fingerprint);
    }
    {
        CPanelSession s(&hosts);
        CPolicy policy;
        ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
        EXPECT_TRUE(policy.seen.empty()); // trusted: the callback was never asked
        const CPanelConnectParams params = s.CopyConnectParams();
        ASSERT_TRUE(params.valid);
        EXPECT_EQ(params.hostKeyFingerprint, accepted);
        EXPECT_FALSE(params.hostKeyType.empty());
    }
    DeleteFileA(file.c_str());
}

// ---- Rename ---------------------------------------------------------------------------------------------------------

namespace
{
// A private directory under the writable upload/ folder on the server, filled and removed through its own session
// (created and used on the test thread only). The panel session under test works in the same directory.
class CRemoteSandbox
{
public:
    CRemoteSandbox()
    {
        dir_ = "upload/ps_" + std::to_string(GetCurrentProcessId()) + "_" +
               ::testing::UnitTest::GetInstance()->current_test_info()->name();
        CSshSite site;
        site.host = sftptest::kHost;
        site.port = sftptest::kPort;
        site.user = sftptest::kUser;
        site.password.assign(sftptest::kPassword);
        site.timeoutSeconds = 20;
        site.nameCodec = CNameCodec::Auto();
        session_.reset(new CSshSession(&callbacks_, nullptr));
        ok_ = session_->Connect(site);
        if (ok_)
        {
            RemoveAll(); // leftovers of an aborted run
            ok_ = session_->Mkdir(dir_);
        }
    }
    ~CRemoteSandbox()
    {
        if (ok_)
            RemoveAll();
    }
    bool Ok() const { return ok_; }
    std::string Path(const std::string& name) const { return dir_ + "/" + name; }

    bool PutFile(const std::string& name, const std::string& content)
    {
        const std::string local = TempFileName("sandbox");
        {
            std::ofstream out(local, std::ios::binary | std::ios::trunc);
            out << content;
        }
        HANDLE h = CreateFileA(local.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        const bool put = h != INVALID_HANDLE_VALUE && session_->Upload(h, Path(name), 0, false, nullptr);
        if (h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
        DeleteFileA(local.c_str());
        return put;
    }
    bool Link(const std::string& target, const std::string& name)
    {
        return session_->Symlink(target, Path(name));
    }
    // The permission bits (07777) of the item, links followed; false when it cannot be read.
    bool Mode(const std::string& name, uint32_t& mode)
    {
        CSftpEntry entry;
        if (!session_->Stat(Path(name), entry))
            return false;
        mode = entry.permissions & 07777;
        return true;
    }
    bool Exists(const std::string& name, uint64_t* size = nullptr)
    {
        CSftpEntry entry;
        if (!session_->Stat(Path(name), entry))
            return false;
        if (size != nullptr)
            *size = entry.size;
        return true;
    }

private:
    struct CAccept : ISshCallbacks
    {
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

    void RemoveAll()
    {
        std::vector<CSftpEntry> list;
        if (session_->ListDir(dir_, list))
            for (const CSftpEntry& e : list)
                if (!session_->Remove(dir_ + "/" + e.rawName))
                    session_->Rmdir(dir_ + "/" + e.rawName); // an empty sub-directory
        session_->Rmdir(dir_);
    }

    CAccept callbacks_;
    std::unique_ptr<CSshSession> session_;
    std::string dir_;
    bool ok_ = false;
};
} // namespace

TEST_F(PanelSessionTest, RenamePlain)
{
    CRemoteSandbox box;
    ASSERT_TRUE(box.Ok());
    ASSERT_TRUE(box.PutFile("a.txt", "abc"));
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    auto rename = s.Rename(box.Path("a.txt"), box.Path("b.txt"), false);
    ASSERT_TRUE(rename->Wait(20000));
    EXPECT_TRUE(rename->Ok()) << rename->Error().message;
    EXPECT_EQ(rename->GetKind(), CPanelRequest::Kind::Rename);
    uint64_t size = 0;
    EXPECT_FALSE(box.Exists("a.txt"));
    ASSERT_TRUE(box.Exists("b.txt", &size));
    EXPECT_EQ(size, 3u);

    auto missing = s.Rename(box.Path("a.txt"), box.Path("c.txt"), false); // the source is gone now
    ASSERT_TRUE(missing->Wait(20000));
    EXPECT_EQ(missing->Error().kind, SshErrorKind::NotFound);
}

TEST_F(PanelSessionTest, RenameOverwriteReplacesTheTarget)
{
    CRemoteSandbox box;
    ASSERT_TRUE(box.Ok());
    ASSERT_TRUE(box.PutFile("src.txt", "new!"));
    ASSERT_TRUE(box.PutFile("dst.txt", "old"));
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    auto rename = s.Rename(box.Path("src.txt"), box.Path("dst.txt"), true);
    ASSERT_TRUE(rename->Wait(20000));
    EXPECT_TRUE(rename->Ok()) << rename->Error().message;
    uint64_t size = 0;
    EXPECT_FALSE(box.Exists("src.txt"));
    ASSERT_TRUE(box.Exists("dst.txt", &size));
    EXPECT_EQ(size, 4u); // the content of the source
}

TEST_F(PanelSessionTest, RenameOntoAnExistingTargetWithoutOverwriteGivesExists)
{
    CRemoteSandbox box;
    ASSERT_TRUE(box.Ok());
    ASSERT_TRUE(box.PutFile("src.txt", "new!"));
    ASSERT_TRUE(box.PutFile("dst.txt", "old"));
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    auto rename = s.Rename(box.Path("src.txt"), box.Path("dst.txt"), false);
    ASSERT_TRUE(rename->Wait(20000));
    EXPECT_EQ(rename->Error().kind, SshErrorKind::Exists);
    uint64_t size = 0;
    EXPECT_TRUE(box.Exists("src.txt"));
    ASSERT_TRUE(box.Exists("dst.txt", &size));
    EXPECT_EQ(size, 3u); // untouched
}

TEST_F(PanelSessionTest, RenameWithoutConnectFails)
{
    CPanelSession s(nullptr);
    auto rename = s.Rename("upload/x", "upload/y", false);
    ASSERT_TRUE(rename->Wait(10000));
    EXPECT_EQ(rename->Error().kind, SshErrorKind::ConnectFailed);
}

TEST_F(PanelSessionTest, MkdirCreatesAndReportsExists)
{
    CRemoteSandbox box;
    ASSERT_TRUE(box.Ok());
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    auto make = s.Mkdir(box.Path("newdir"));
    ASSERT_TRUE(make->Wait(20000));
    EXPECT_TRUE(make->Ok()) << make->Error().message;
    EXPECT_EQ(make->GetKind(), CPanelRequest::Kind::Mkdir);
    EXPECT_TRUE(box.Exists("newdir"));

    auto again = s.Mkdir(box.Path("newdir"));
    ASSERT_TRUE(again->Wait(20000));
    EXPECT_EQ(again->Error().kind, SshErrorKind::Exists);

    ASSERT_TRUE(box.PutFile("afile", "x"));
    auto onFile = s.Mkdir(box.Path("afile")); // a file with that name counts as existing too
    ASSERT_TRUE(onFile->Wait(20000));
    EXPECT_EQ(onFile->Error().kind, SshErrorKind::Exists);
}

TEST_F(PanelSessionTest, RemoveFileDeletesAFile)
{
    CRemoteSandbox box;
    ASSERT_TRUE(box.Ok());
    ASSERT_TRUE(box.PutFile("gone.txt", "abc"));
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    auto remove = s.RemoveFile(box.Path("gone.txt"));
    ASSERT_TRUE(remove->Wait(20000));
    EXPECT_TRUE(remove->Ok()) << remove->Error().message;
    EXPECT_EQ(remove->GetKind(), CPanelRequest::Kind::RemoveFile);
    EXPECT_FALSE(box.Exists("gone.txt"));
}

TEST_F(PanelSessionTest, RemoveFileOfAMissingFileGivesNotFound)
{
    CRemoteSandbox box;
    ASSERT_TRUE(box.Ok());
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    auto remove = s.RemoveFile(box.Path("never_existed.txt"));
    ASSERT_TRUE(remove->Wait(20000));
    EXPECT_EQ(remove->Error().kind, SshErrorKind::NotFound);
}

TEST_F(PanelSessionTest, RemoveFileOnALinkRemovesTheLinkAndKeepsTheTarget)
{
    CRemoteSandbox box;
    ASSERT_TRUE(box.Ok());
    ASSERT_TRUE(box.PutFile("target.txt", "keep me"));
    ASSERT_TRUE(box.Link("target.txt", "link.txt"));
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    auto remove = s.RemoveFile(box.Path("link.txt"));
    ASSERT_TRUE(remove->Wait(20000));
    EXPECT_TRUE(remove->Ok()) << remove->Error().message;
    uint64_t size = 0;
    EXPECT_FALSE(box.Exists("link.txt"));
    ASSERT_TRUE(box.Exists("target.txt", &size));
    EXPECT_EQ(size, 7u);
}

TEST_F(PanelSessionTest, RemoveFileOnADirectoryFailsAndKeepsIt)
{
    CRemoteSandbox box;
    ASSERT_TRUE(box.Ok());
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    auto make = s.Mkdir(box.Path("adir"));
    ASSERT_TRUE(make->Wait(20000));
    auto remove = s.RemoveFile(box.Path("adir"));
    ASSERT_TRUE(remove->Wait(20000));
    EXPECT_FALSE(remove->Ok());
    EXPECT_TRUE(box.Exists("adir"));
}

TEST_F(PanelSessionTest, ChmodChangesThePermissionBits)
{
    CRemoteSandbox box;
    ASSERT_TRUE(box.Ok());
    ASSERT_TRUE(box.PutFile("m.txt", "x"));
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    auto change = s.Chmod(box.Path("m.txt"), 0640);
    ASSERT_TRUE(change->Wait(20000));
    EXPECT_TRUE(change->Ok()) << change->Error().message;
    EXPECT_EQ(change->GetKind(), CPanelRequest::Kind::Chmod);
    uint32_t mode = 0;
    ASSERT_TRUE(box.Mode("m.txt", mode));
    EXPECT_EQ(mode, 0640u);
}

TEST_F(PanelSessionTest, ChmodKeepsTheSpecialBitsAndDropsTheFileType)
{
    CRemoteSandbox box;
    ASSERT_TRUE(box.Ok());
    ASSERT_TRUE(box.PutFile("sticky.txt", "x"));
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    // 0100000 is the "regular file" type bit of a full st_mode: it must be masked off, never sent
    auto change = s.Chmod(box.Path("sticky.txt"), 0100000 | 02644);
    ASSERT_TRUE(change->Wait(20000));
    EXPECT_TRUE(change->Ok()) << change->Error().message;
    uint32_t mode = 0;
    ASSERT_TRUE(box.Mode("sticky.txt", mode));
    EXPECT_EQ(mode, 02644u); // setgid on a file we own (our group): the server keeps it
}

TEST_F(PanelSessionTest, ChmodOfAMissingPathGivesNotFound)
{
    CRemoteSandbox box;
    ASSERT_TRUE(box.Ok());
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    auto change = s.Chmod(box.Path("never_existed.txt"), 0644);
    ASSERT_TRUE(change->Wait(20000));
    EXPECT_EQ(change->Error().kind, SshErrorKind::NotFound);
}

TEST_F(PanelSessionTest, ChmodWithoutConnectFails)
{
    CPanelSession s(nullptr);
    auto change = s.Chmod("upload/x", 0644);
    ASSERT_TRUE(change->Wait(10000));
    EXPECT_EQ(change->Error().kind, SshErrorKind::ConnectFailed);
}

TEST_F(PanelSessionTest, RemoveFileWithoutConnectFails)
{
    CPanelSession s(nullptr);
    auto remove = s.RemoveFile("upload/x");
    ASSERT_TRUE(remove->Wait(10000));
    EXPECT_EQ(remove->Error().kind, SshErrorKind::ConnectFailed);
}

TEST_F(PanelSessionTest, MkdirNonAsciiName)
{
    CRemoteSandbox box;
    ASSERT_TRUE(box.Ok());
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    const std::string name = CNameCodec::Utf8().Encode(L"složená říže");
    auto make = s.Mkdir(box.Path(name));
    ASSERT_TRUE(make->Wait(20000));
    EXPECT_TRUE(make->Ok()) << make->Error().message;
    EXPECT_TRUE(box.Exists(name));
}

TEST_F(PanelSessionTest, MkdirWithMissingParentFails)
{
    CRemoteSandbox box;
    ASSERT_TRUE(box.Ok());
    CPanelSession s(nullptr);
    CPolicy policy;
    ASSERT_TRUE(ConnectOk(s, TestSite(), policy));
    auto make = s.Mkdir(box.Path("no_such_parent/child"));
    ASSERT_TRUE(make->Wait(20000));
    EXPECT_FALSE(make->Ok());
    EXPECT_EQ(make->Error().kind, SshErrorKind::NotFound);
}

TEST_F(PanelSessionTest, MkdirWithoutConnectFails)
{
    CPanelSession s(nullptr);
    auto make = s.Mkdir("upload/x");
    ASSERT_TRUE(make->Wait(10000));
    EXPECT_EQ(make->Error().kind, SshErrorKind::ConnectFailed);
}
