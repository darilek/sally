#include <gtest/gtest.h>

#include <windows.h>

#include "text_util.h"

TEST(TextUtil, Utf8DecodeRoundTrip)
{
    const std::wstring text = L"caf\u00e9 \u4e2d";
    const CNameCodec utf8 = CNameCodec::Utf8();
    EXPECT_EQ(utf8.Decode(utf8.Encode(text)), text);
    EXPECT_EQ(utf8.Decode(""), L"");
}

TEST(TextUtil, InvalidUtf8DoesNotCrash)
{
    const std::wstring decoded = CNameCodec::Utf8().Decode("a\xff" "b");
    EXPECT_EQ(decoded, L"a\ufffd" L"b");
}

TEST(TextUtil, AppDataSallyDirIsAbsoluteAndEndsWithSallySftp)
{
    const std::wstring dir = AppDataSallyDir();
    ASSERT_GT(dir.size(), 12u);
    EXPECT_EQ(dir[1], L':');
    EXPECT_EQ(dir.substr(dir.size() - 10), L"Sally\\SFTP");
}

TEST(TextUtil, EnsureDirectoryCreatesNestedAndIsIdempotent)
{
    wchar_t temp[MAX_PATH];
    ASSERT_GT(GetTempPathW(MAX_PATH, temp), 0u);
    const std::wstring root = std::wstring(temp) + L"sally_sftp_ops_" + std::to_wstring(GetCurrentProcessId());
    const std::wstring nested = root + L"\\a\\b";

    EXPECT_TRUE(EnsureDirectory(nested));
    EXPECT_NE(GetFileAttributesW(nested.c_str()), INVALID_FILE_ATTRIBUTES);
    EXPECT_TRUE(EnsureDirectory(nested));

    RemoveDirectoryW(nested.c_str());
    RemoveDirectoryW((root + L"\\a").c_str());
    RemoveDirectoryW(root.c_str());
    EXPECT_EQ(GetFileAttributesW(root.c_str()), INVALID_FILE_ATTRIBUTES);
}

TEST(TextUtil, DescribeSshErrorIsNotEmpty)
{
    const CSshErrorLabels labels = {L"Unknown error", L"code", L"SFTP status"};
    CSshError error;
    EXPECT_EQ(DescribeSshError(error, labels), L"Unknown error");

    error.kind = SshErrorKind::ConnectFailed;
    error.message = "Connection refused";
    error.libssh2Code = -43;
    std::wstring text = DescribeSshError(error, labels);
    EXPECT_NE(text.find(L"Connection refused"), std::wstring::npos);
    EXPECT_NE(text.find(L"-43"), std::wstring::npos);
    EXPECT_NE(text.find(L"code"), std::wstring::npos);

    error.sftpStatus = 2;
    text = DescribeSshError(error, {L"?", L"Kod", L"Stav SFTP"}); // the caller's words are used
    EXPECT_EQ(text, L"Connection refused (Kod -43, Stav SFTP 2)");
}

TEST(TextUtil, FormatHostPort)
{
    EXPECT_EQ(FormatHostPort("example.com", 22), "example.com:22");
    EXPECT_EQ(FormatHostPort("10.0.0.1", 2222), "10.0.0.1:2222");
    EXPECT_EQ(FormatHostPort("::1", 22), "[::1]:22");
    EXPECT_EQ(FormatHostPort("[::1]", 22), "[::1]:22");
}

TEST(TextUtil, SplitPathAndMask)
{
    std::wstring path, mask;
    SplitPathAndMask(std::wstring(L"c:\\dir\\") + L'\0' + L"*.txt", path, mask);
    EXPECT_EQ(L"c:\\dir\\", path);
    EXPECT_EQ(L"*.txt", mask);

    SplitPathAndMask(L"c:\\dir", path, mask);
    EXPECT_EQ(L"c:\\dir", path);
    EXPECT_EQ(L"", mask);

    SplitPathAndMask(std::wstring(L"c:\\dir") + L'\0', path, mask); // an empty mask after the NUL
    EXPECT_EQ(L"c:\\dir", path);
    EXPECT_EQ(L"", mask);

    SplitPathAndMask(L"", path, mask);
    EXPECT_EQ(L"", path);
    EXPECT_EQ(L"", mask);
}
