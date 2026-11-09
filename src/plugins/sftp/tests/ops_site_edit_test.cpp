// Tests for ops/site_edit: form <-> site conversion, error placement, list strings, quick connect parsing.
#include <gtest/gtest.h>

#include "site_edit.h"

namespace
{
CSiteForm ValidForm()
{
    CSite s;
    s.name = L"Work";
    s.host = L"example.com";
    s.user = L"bob";
    return FormFromSite(s);
}
} // namespace

TEST(SiteEdit, RoundTrip)
{
    CSite in;
    in.name = L"Work";
    in.host = L"example.com";
    in.port = 2222;
    in.user = L"bob";
    in.keyFile = L"C:\\keys\\id";
    in.useAgent = true;
    in.compression = true;
    in.timeoutSeconds = 45;
    in.encoding = CSite::Encoding::CodePage;
    in.codePage = 1250;
    in.initialRemotePath = L"/srv";
    in.initialLocalPath = L"D:\\data";
    in.rememberPassword = true;

    CSite out;
    ASSERT_EQ(SiteFromForm(FormFromSite(in), out), SiteError::None);
    EXPECT_EQ(out.name, in.name);
    EXPECT_EQ(out.host, in.host);
    EXPECT_EQ(out.port, 2222);
    EXPECT_EQ(out.user, in.user);
    EXPECT_EQ(out.keyFile, in.keyFile);
    EXPECT_TRUE(out.useAgent);
    EXPECT_TRUE(out.compression);
    EXPECT_EQ(out.timeoutSeconds, 45);
    EXPECT_EQ(out.encoding, CSite::Encoding::CodePage);
    EXPECT_EQ(out.codePage, 1250u);
    EXPECT_EQ(out.initialRemotePath, L"/srv");
    EXPECT_EQ(out.initialLocalPath, L"D:\\data");
    EXPECT_TRUE(out.rememberPassword);
}

TEST(SiteEdit, TrimsTextFields)
{
    CSiteForm f = ValidForm();
    f.name = L"  Work ";
    f.host = L" example.com ";
    f.user = L" bob";
    f.port = L" 22 ";
    CSite out;
    ASSERT_EQ(SiteFromForm(f, out), SiteError::None);
    EXPECT_EQ(out.name, L"Work");
    EXPECT_EQ(out.host, L"example.com");
    EXPECT_EQ(out.user, L"bob");
}

TEST(SiteEdit, ErrorsPerField)
{
    CSite out;
    CSiteForm f = ValidForm();
    f.name = L"";
    EXPECT_EQ(SiteFromForm(f, out), SiteError::NameEmpty);
    f = ValidForm();
    f.name = L"a/b";
    EXPECT_EQ(SiteFromForm(f, out), SiteError::NameInvalid);
    f = ValidForm();
    f.host = L"";
    EXPECT_EQ(SiteFromForm(f, out), SiteError::HostEmpty);
    f = ValidForm();
    f.host = L"a b";
    EXPECT_EQ(SiteFromForm(f, out), SiteError::HostInvalid);
    f = ValidForm();
    f.user = L"";
    EXPECT_EQ(SiteFromForm(f, out), SiteError::UserEmpty);
    for (const wchar_t* bad : {L"", L"0", L"65536", L"abc", L"-1", L"22x", L"99999999999"})
    {
        f = ValidForm();
        f.port = bad;
        EXPECT_EQ(SiteFromForm(f, out), SiteError::PortRange) << bad;
    }
    for (const wchar_t* bad : {L"", L"0", L"3601", L"x"})
    {
        f = ValidForm();
        f.timeout = bad;
        EXPECT_EQ(SiteFromForm(f, out), SiteError::TimeoutRange) << bad;
    }
}

TEST(SiteEdit, CodePageOnlyCheckedWhenChosen)
{
    CSite out;
    CSiteForm f = ValidForm();
    f.codePage = L"junk";
    EXPECT_EQ(SiteFromForm(f, out), SiteError::None); // UTF-8: the code page text is ignored
    EXPECT_EQ(out.codePage, 1252u);

    f.encoding = CSite::Encoding::CodePage;
    EXPECT_EQ(SiteFromForm(f, out), SiteError::CodePageInvalid);
    f.codePage = L"0";
    EXPECT_EQ(SiteFromForm(f, out), SiteError::CodePageInvalid);
    f.codePage = L"1250";
    EXPECT_EQ(SiteFromForm(f, out), SiteError::None);
    EXPECT_EQ(out.codePage, 1250u);
}

TEST(SiteEdit, FailureLeavesOutUnchanged)
{
    CSite out;
    out.name = L"Keep";
    CSiteForm f = ValidForm();
    f.host = L"";
    EXPECT_NE(SiteFromForm(f, out), SiteError::None);
    EXPECT_EQ(out.name, L"Keep");
}

TEST(SiteEdit, FieldAndTabOfError)
{
    EXPECT_EQ(FieldOfError(SiteError::NameInvalid), SiteField::Name);
    EXPECT_EQ(FieldOfError(SiteError::HostInvalid), SiteField::Host);
    EXPECT_EQ(FieldOfError(SiteError::PortRange), SiteField::Port);
    EXPECT_EQ(FieldOfError(SiteError::UserInvalid), SiteField::User);
    EXPECT_EQ(FieldOfError(SiteError::TimeoutRange), SiteField::Timeout);
    EXPECT_EQ(FieldOfError(SiteError::CodePageInvalid), SiteField::CodePage);
    EXPECT_EQ(TabOfField(SiteField::Port), SiteTabSession);
    EXPECT_EQ(TabOfField(SiteField::Timeout), SiteTabConnection);
    EXPECT_EQ(TabOfField(SiteField::CodePage), SiteTabConnection);
}

TEST(SiteEdit, HostColumn)
{
    CSite s;
    s.host = L"example.com";
    EXPECT_EQ(SiteHostColumn(s), L"example.com");
    s.port = 2222;
    EXPECT_EQ(SiteHostColumn(s), L"example.com:2222");
    s.host = L"::1";
    s.port = 22;
    EXPECT_EQ(SiteHostColumn(s), L"[::1]");
}

TEST(SiteEdit, QuickConnect)
{
    CSftpPath p;
    ASSERT_TRUE(ParseQuickConnect(L" bob@example.com:2222/srv/x ", p));
    EXPECT_EQ(p.user, L"bob");
    EXPECT_EQ(p.host, L"example.com");
    EXPECT_EQ(p.port, 2222);
    EXPECT_EQ(p.path, L"/srv/x");
    ASSERT_TRUE(ParseQuickConnect(L"SFTP:bob@example.com", p));
    EXPECT_EQ(p.port, 22);
    EXPECT_EQ(p.path, L"/");

    CSftpPath keep = p;
    EXPECT_FALSE(ParseQuickConnect(L"", p));
    EXPECT_FALSE(ParseQuickConnect(L"example.com", p));
    EXPECT_FALSE(ParseQuickConnect(L"@Work", p));
    EXPECT_FALSE(ParseQuickConnect(L"bob@host:99999", p));
    EXPECT_EQ(p.host, keep.host);
}

TEST(SiteEdit, ConnectPath)
{
    EXPECT_EQ(SiteConnectPath(L"Work"), L"@Work/");
    CSftpPath back;
    ASSERT_TRUE(ParseSftpPath(SiteConnectPath(L"My Site"), back));
    EXPECT_TRUE(back.isSite);
    EXPECT_EQ(back.siteName, L"My Site");
}
