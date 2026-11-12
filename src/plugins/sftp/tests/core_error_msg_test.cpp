// The translatable error ids (SshMsg) set by CSshSession; no server needed.
#include "test_support.h"

#include <windows.h>

#include <vector>

#include "ssh_init.h"
#include "ssh_session.h"

namespace
{
struct CNoCallbacks : ISshCallbacks
{
    HostKeyDecision VerifyHostKey(const std::string&, int, const std::string&, const std::string&, bool) override
    {
        return HostKeyDecision::Reject;
    }
    bool Prompt(PromptKind, const std::string&, const std::string&, const std::string&, bool, CSecret&) override { return false; }
    bool Progress(uint64_t, uint64_t) override { return true; }
};
} // namespace

TEST(ErrorMsg, ExecOnASessionThatWasNeverConnectedIsSessionNotOpen)
{
    ASSERT_TRUE(SshGlobalInit());
    {
        CNoCallbacks cb;
        CSshSession session(&cb, nullptr);
        CExecResult result;
        EXPECT_FALSE(session.Exec("echo x", CExecOptions(), result));
        const CSshError& error = session.LastError();
        EXPECT_EQ(error.msgId, SshMsg::SessionNotOpen);
        EXPECT_TRUE(error.args.empty());
        EXPECT_EQ(error.message, "The SSH session is not open"); // the English text stays
    }
    SshGlobalExit();
}

TEST(ErrorMsg, UnresolvableHostIsResolveFailedWithHostAndSystemText)
{
    ASSERT_TRUE(SshGlobalInit());
    {
        CNoCallbacks cb;
        CSshSession session(&cb, nullptr);
        CSshSite site;
        site.host = "no-such-host.invalid"; // .invalid never resolves (RFC 2606)
        site.timeoutSeconds = 20;
        EXPECT_FALSE(session.Connect(site));
        const CSshError& error = session.LastError();
        // a very slow DNS could end as a timeout instead; both are converted ids with the host as the first argument
        ASSERT_TRUE(error.msgId == SshMsg::ResolveFailed || error.msgId == SshMsg::ResolveTimeout) << error.message;
        ASSERT_EQ(error.args.size(), 2u);
        EXPECT_EQ(error.args[0], "no-such-host.invalid");
        EXPECT_FALSE(error.args[1].empty());
        EXPECT_NE(error.message.find("no-such-host.invalid"), std::string::npos);
    }
    SshGlobalExit();
}

TEST(ErrorMsg, ACancelledConnectCarriesNoId)
{
    ASSERT_TRUE(SshGlobalInit());
    {
        CNoCallbacks cb;
        CSshSession session(&cb, nullptr);
        session.Cancel();
        CSshSite site;
        site.host = "no-such-host.invalid";
        EXPECT_FALSE(session.Connect(site));
        EXPECT_EQ(session.LastError().kind, SshErrorKind::Cancelled);
        EXPECT_EQ(session.LastError().msgId, SshMsg::None);
        EXPECT_TRUE(session.LastError().args.empty());
    }
    SshGlobalExit();
}
