#include "text_util.h"

#include <windows.h>
#include <shlobj.h>

std::wstring AppDataSallyDir()
{
    PWSTR roaming = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, &roaming)))
    {
        result = roaming;
        while (!result.empty() && result.back() == L'\\')
            result.pop_back();
        result += L"\\Sally\\SFTP";
    }
    CoTaskMemFree(roaming); // also needed when the call failed
    return result;
}

bool EnsureDirectory(const std::wstring& path)
{
    if (path.empty())
        return false;
    const DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES)
        return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;

    const size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos && slash > 0 && path[slash - 1] != L':')
    {
        if (!EnsureDirectory(path.substr(0, slash)))
            return false;
    }
    if (CreateDirectoryW(path.c_str(), nullptr))
        return true;
    // another thread or process may have created it in the meantime
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

std::wstring FormatErrorMessage(const std::wstring& pattern, const std::vector<std::string>& args)
{
    const CNameCodec utf8 = CNameCodec::Utf8();
    std::wstring out;
    for (size_t i = 0; i < pattern.size(); ++i)
    {
        const wchar_t ch = pattern[i];
        const wchar_t next = i + 1 < pattern.size() ? pattern[i + 1] : L'\0';
        if (ch == L'%' && next == L'%')
        {
            out += L'%';
            ++i;
        }
        else if (ch == L'%' && next >= L'1' && next <= L'9')
        {
            const size_t index = static_cast<size_t>(next - L'1');
            if (index < args.size()) // a missing argument becomes empty text
                out += utf8.Decode(args[index]);
            ++i;
        }
        else
            out += ch; // includes a lone '%'
    }
    return out;
}

std::wstring DescribeSshError(const CSshError& error, const CSshErrorLabels& labels)
{
    return DescribeSshError(error, labels, std::wstring());
}

std::wstring DescribeSshError(const CSshError& error, const CSshErrorLabels& labels, const std::wstring& translatedText)
{
    std::wstring text = translatedText.empty() ? CNameCodec::Utf8().Decode(error.message) : translatedText;
    if (text.empty())
        text = labels.unknown;
    if (error.libssh2Code != 0)
    {
        text += L" (" + labels.code + L" " + std::to_wstring(error.libssh2Code);
        if (error.sftpStatus != 0)
            text += L", " + labels.sftpStatus + L" " + std::to_wstring(error.sftpStatus);
        text += L")";
    }
    return text;
}

std::wstring KnownHostsOwnPath()
{
    const std::wstring dir = AppDataSallyDir();
    return dir.empty() ? dir : dir + L"\\known_hosts";
}

std::wstring KnownHostsUserPath()
{
    PWSTR profile = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, KF_FLAG_DEFAULT, nullptr, &profile)))
    {
        result = profile;
        while (!result.empty() && result.back() == L'\\')
            result.pop_back();
        result += L"\\.ssh\\known_hosts";
    }
    CoTaskMemFree(profile);
    return result;
}

std::string FormatHostPort(const std::string& host, int port)
{
    const bool bracket = host.find(':') != std::string::npos && (host.empty() || host.front() != '[');
    return (bracket ? "[" + host + "]" : host) + ":" + std::to_string(port);
}

void SplitPathAndMask(const std::wstring& payload, std::wstring& path, std::wstring& mask)
{
    const size_t separator = payload.find(L'\0');
    path = payload.substr(0, separator);
    mask = separator == std::wstring::npos ? std::wstring() : payload.substr(separator + 1);
}

TargetMaskKind ClassifyTargetMask(const std::wstring& mask, std::wstring& newName)
{
    newName.clear();
    if (mask.empty() || mask == L"*" || mask == L"*.*")
        return TargetMaskKind::KeepName;
    if (mask.find_first_of(L"*?") != std::wstring::npos)
        return TargetMaskKind::Wildcard;
    newName = mask;
    return TargetMaskKind::Rename;
}
