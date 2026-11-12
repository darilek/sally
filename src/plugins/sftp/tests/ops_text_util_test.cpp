#include <gtest/gtest.h>

#include <winsock2.h> // before windows.h: libssh2 needs it
#include <windows.h>

#include <libssh2_sftp.h>

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

TEST(TextUtil, ClassifyTargetMask)
{
    std::wstring name = L"stale";
    EXPECT_EQ(ClassifyTargetMask(L"", name), TargetMaskKind::KeepName);
    EXPECT_TRUE(name.empty());
    EXPECT_EQ(ClassifyTargetMask(L"*", name), TargetMaskKind::KeepName);
    EXPECT_EQ(ClassifyTargetMask(L"*.*", name), TargetMaskKind::KeepName);

    EXPECT_EQ(ClassifyTargetMask(L"new name.ext", name), TargetMaskKind::Rename);
    EXPECT_EQ(name, L"new name.ext");

    EXPECT_EQ(ClassifyTargetMask(L"*.txt", name), TargetMaskKind::Wildcard);
    EXPECT_TRUE(name.empty());
    EXPECT_EQ(ClassifyTargetMask(L"a?c", name), TargetMaskKind::Wildcard);
    EXPECT_EQ(ClassifyTargetMask(L"data*", name), TargetMaskKind::Wildcard);
}

TEST(FormatErrorMessage, FillsArguments)
{
    EXPECT_EQ(FormatErrorMessage(L"Cannot resolve host '%1' (%2)", {"example.com", "Winsock error 11001"}),
              L"Cannot resolve host 'example.com' (Winsock error 11001)");
    EXPECT_EQ(FormatErrorMessage(L"no arguments", {}), L"no arguments");
    EXPECT_EQ(FormatErrorMessage(L"", {"x"}), L"");
    EXPECT_EQ(FormatErrorMessage(L"%1", {"caf\xC3\xA9"}), std::wstring(L"caf") + wchar_t(0xE9)); // arguments are UTF-8
}

TEST(FormatErrorMessage, TranslationMayReorderAndRepeat)
{
    EXPECT_EQ(FormatErrorMessage(L"%2 / %1 / %2", {"a", "b"}), L"b / a / b");
}

TEST(FormatErrorMessage, MissingArgumentBecomesEmpty)
{
    EXPECT_EQ(FormatErrorMessage(L"[%1][%2][%9]", {"a"}), L"[a][][]");
    EXPECT_EQ(FormatErrorMessage(L"[%1]", {}), L"[]");
}

TEST(FormatErrorMessage, PercentSigns)
{
    EXPECT_EQ(FormatErrorMessage(L"100%% of %1", {"x"}), L"100% of x");
    EXPECT_EQ(FormatErrorMessage(L"%%1", {"x"}), L"%1"); // an escaped percent is not an argument
    EXPECT_EQ(FormatErrorMessage(L"50% done", {}), L"50% done"); // lone '%' stays
    EXPECT_EQ(FormatErrorMessage(L"%0 %a end%", {"x"}), L"%0 %a end%");
}

TEST(FormatErrorMessage, InvalidUtf8ArgumentBecomesReplacementCharacter)
{
    EXPECT_EQ(FormatErrorMessage(L"<%1>", {"a\xff" "b"}), std::wstring(L"<a") + wchar_t(0xFFFD) + L"b>");
}

TEST(TextUtil, DescribeSshErrorUsesTranslatedTextWhenGiven)
{
    const CSshErrorLabels labels = {L"Unknown error", L"code", L"SFTP status"};
    CSshError error;
    error.message = "English text";
    error.libssh2Code = -5;
    EXPECT_EQ(DescribeSshError(error, labels, L"Prelozeny text"), L"Prelozeny text (code -5)");
    EXPECT_EQ(DescribeSshError(error, labels, L""), L"English text (code -5)"); // empty = fall back to message
}

// ---- Reasons of the SFTP action sentences ----------------------------------------------------------------------------

namespace
{
ErrorReason ReasonOfStatus(unsigned long status, SshErrorKind kind = SshErrorKind::Other)
{
    CSshError error;
    error.kind = kind;
    error.sftpStatus = status;
    return ReasonOfError(error);
}
} // namespace

TEST(ErrorReason, StatusesMapToTheirReasons)
{
    EXPECT_EQ(ReasonOfStatus(LIBSSH2_FX_NO_SUCH_FILE), ErrorReason::NotFound);
    EXPECT_EQ(ReasonOfStatus(LIBSSH2_FX_NO_SUCH_PATH), ErrorReason::NotFound);
    EXPECT_EQ(ReasonOfStatus(LIBSSH2_FX_PERMISSION_DENIED), ErrorReason::PermissionDenied);
    EXPECT_EQ(ReasonOfStatus(LIBSSH2_FX_FILE_ALREADY_EXISTS), ErrorReason::AlreadyExists);
    EXPECT_EQ(ReasonOfStatus(LIBSSH2_FX_DIR_NOT_EMPTY), ErrorReason::DirNotEmpty);
    EXPECT_EQ(ReasonOfStatus(LIBSSH2_FX_NOT_A_DIRECTORY), ErrorReason::NotADirectory);
    EXPECT_EQ(ReasonOfStatus(LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM), ErrorReason::NoSpace);
    EXPECT_EQ(ReasonOfStatus(LIBSSH2_FX_QUOTA_EXCEEDED), ErrorReason::NoSpace);
    EXPECT_EQ(ReasonOfStatus(LIBSSH2_FX_WRITE_PROTECT), ErrorReason::WriteProtected);
    EXPECT_EQ(ReasonOfStatus(LIBSSH2_FX_OP_UNSUPPORTED), ErrorReason::Unsupported);
    EXPECT_EQ(ReasonOfStatus(LIBSSH2_FX_CONNECTION_LOST), ErrorReason::ConnectionLost);
    EXPECT_EQ(ReasonOfStatus(LIBSSH2_FX_NO_CONNECTION), ErrorReason::ConnectionLost);
}

TEST(ErrorReason, OtherStatusesAreTheGenericFailure)
{
    EXPECT_EQ(ReasonOfStatus(LIBSSH2_FX_FAILURE), ErrorReason::Failed);
    EXPECT_EQ(ReasonOfStatus(LIBSSH2_FX_BAD_MESSAGE), ErrorReason::Failed);
    EXPECT_EQ(ReasonOfStatus(12345), ErrorReason::Failed);
}

TEST(ErrorReason, WithoutAStatusTheKindDecides)
{
    EXPECT_EQ(ReasonOfStatus(0, SshErrorKind::Timeout), ErrorReason::NoAnswer);
    EXPECT_EQ(ReasonOfStatus(0, SshErrorKind::ConnectFailed), ErrorReason::None); // the libssh2 text is shown instead
    // a generic FAILURE that was found to be a non-empty directory
    EXPECT_EQ(ReasonOfStatus(LIBSSH2_FX_FAILURE, SshErrorKind::NotEmpty), ErrorReason::DirNotEmpty);
}

TEST(ErrorReason, OnlyTheActionIdsTakeAReason)
{
    EXPECT_TRUE(IsSftpActionMsg(SshMsg::SftpList));
    EXPECT_TRUE(IsSftpActionMsg(SshMsg::SftpChangeAttrs));
    EXPECT_FALSE(IsSftpActionMsg(SshMsg::None));
    EXPECT_FALSE(IsSftpActionMsg(SshMsg::AlreadyExists));
    EXPECT_FALSE(IsSftpActionMsg(SshMsg::SftpNotOpen));
}
