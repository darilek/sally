// SPDX-FileCopyrightText: 2026 Sally Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <vector>

#include "site_config.h"
#include "text_util.h"

CSiteConfig SiteConfig;

namespace
{
const wchar_t* const kConfigVersion = L"ConfigVersion";
const wchar_t* const kSitesKey = L"Sites";
const DWORD kCurrentVersion = 1;
const int kMaxSites = 100000; // guards the load loop against a registry that never ends

typedef std::vector<BYTE> CBlob;

std::wstring SecretsKey(const std::wstring& name)
{
    std::wstring key = name;
    if (!key.empty())
        CharLowerBuffW(&key[0], static_cast<DWORD>(key.size()));
    return key;
}

void WipeBlob(CBlob& blob)
{
    if (!blob.empty())
        SecureZeroMemory(blob.data(), blob.size());
    blob.clear();
}

// Frees a buffer that Sally's password manager allocated.
void FreeSallyBuffer(BYTE* buffer, int size)
{
    if (buffer == nullptr)
        return;
    if (size > 0)
        SecureZeroMemory(buffer, size);
    SalamanderGeneral->Free(buffer);
}

// UTF-8 secret -> wiped-after-use UTF-16 buffer (zero-terminated). False on invalid UTF-8.
bool ToWideBuffer(const CSecret& secret, std::vector<wchar_t>& wide)
{
    const int length = static_cast<int>(secret.size());
    wide.assign(static_cast<size_t>(length) + 1, L'\0');
    if (length == 0)
        return true;
    const int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, secret.data(), length, wide.data(), length);
    return written > 0;
}

void WipeWide(std::vector<wchar_t>& wide)
{
    if (!wide.empty())
        SecureZeroMemory(wide.data(), wide.size() * sizeof(wchar_t));
}

// plain -> blob. aes: encrypt with the master password (it must be entered), otherwise only scramble.
bool EncryptSecret(const CSecret& secret, bool aes, CBlob& blob) noexcept
{
    std::vector<wchar_t> wide;
    BYTE* encrypted = nullptr;
    int encryptedSize = 0;
    bool ok = false;
    try
    {
        CSalamanderPasswordManagerAbstract* pm = SalamanderGeneral->GetSalamanderPasswordManager();
        if (pm != nullptr && ToWideBuffer(secret, wide) &&
            pm->EncryptPassword(wide.data(), &encrypted, &encryptedSize, aes ? TRUE : FALSE) && encrypted != nullptr &&
            encryptedSize > 0)
        {
            CBlob result(encrypted, encrypted + encryptedSize);
            blob.swap(result);
            ok = true;
        }
    }
    catch (...)
    {
        ok = false;
    }
    WipeWide(wide);
    FreeSallyBuffer(encrypted, encryptedSize);
    return ok;
}

// blob -> plain secret (UTF-8, wiped container). The intermediate UTF-16 text lives in the password manager's buffer,
// which is wiped here.
bool DecryptSecret(const CBlob& blob, CSecret& out) noexcept
{
    out.clear();
    if (blob.empty())
        return false;
    CSalamanderStringBufferOwner owner;
    bool ok = false;
    try
    {
        CSalamanderPasswordManagerAbstract* pm = SalamanderGeneral->GetSalamanderPasswordManager();
        CSalamanderStringBuffer* buffer = owner.Buffer();
        if (pm != nullptr && buffer != nullptr && pm->DecryptPassword(blob.data(), static_cast<int>(blob.size()), buffer) &&
            buffer->Data != nullptr && buffer->Length < buffer->Capacity)
        {
            const int length = static_cast<int>(buffer->Length);
            if (length == 0)
                ok = true;
            else
            {
                const int bytes = WideCharToMultiByte(CP_UTF8, 0, buffer->Data, length, nullptr, 0, nullptr, nullptr);
                if (bytes > 0)
                {
                    out.resize(static_cast<size_t>(bytes));
                    ok = WideCharToMultiByte(CP_UTF8, 0, buffer->Data, length, out.data(), bytes, nullptr, nullptr) == bytes;
                }
            }
        }
        if (buffer != nullptr && buffer->Data != nullptr)
            SecureZeroMemory(buffer->Data, static_cast<size_t>(buffer->Capacity) * sizeof(wchar_t));
    }
    catch (...)
    {
        ok = false;
    }
    if (!ok)
        out.clear();
    return ok;
}

// The master password must be known before AES encryption or decryption. False: the user cancelled or it is wrong.
bool EnsureMasterPassword(HWND parent)
{
    CSalamanderPasswordManagerAbstract* pm = SalamanderGeneral->GetSalamanderPasswordManager();
    if (pm == nullptr)
        return false;
    return !pm->IsUsingMasterPassword() || pm->IsMasterPasswordSet() || pm->AskForMasterPassword(parent);
}

// scrambled <-> AES conversion of one blob, as FTP's EncryptPasswordAux does. Unconvertible: false, blob unchanged.
bool ConvertBlob(CBlob& blob, bool toAes) noexcept
{
    if (blob.empty())
        return true;
    CSalamanderPasswordManagerAbstract* pm = SalamanderGeneral->GetSalamanderPasswordManager();
    if (pm == nullptr)
        return false;
    const bool isAes = pm->IsPasswordEncrypted(blob.data(), static_cast<int>(blob.size())) != FALSE;
    if (isAes == toAes)
        return true;
    CSecret plain;
    CBlob converted;
    if (!DecryptSecret(blob, plain) || !EncryptSecret(plain, toAes, converted))
        return false;
    WipeBlob(blob);
    blob.swap(converted);
    return true;
}

bool IsAesBlob(const CBlob& blob)
{
    CSalamanderPasswordManagerAbstract* pm = SalamanderGeneral->GetSalamanderPasswordManager();
    return pm != nullptr && pm->IsPasswordEncrypted(blob.data(), static_cast<int>(blob.size())) != FALSE;
}

// ---- registry helpers ----

bool GetDword(CSalamanderRegistryAbstract* registry, HKEY key, const wchar_t* name, DWORD& value)
{
    DWORD v = 0;
    if (!registry->GetValue(key, name, REG_DWORD, &v, sizeof(DWORD)))
        return false;
    value = v;
    return true;
}

void SetDword(CSalamanderRegistryAbstract* registry, HKEY key, const wchar_t* name, DWORD value)
{
    registry->SetValue(key, name, REG_DWORD, &value, sizeof(DWORD));
}

void SetStringIfNotEmpty(CSalamanderRegistryAbstract* registry, HKEY key, const wchar_t* name, const std::wstring& value)
{
    if (!value.empty())
        SPLRegistrySetString(registry, key, name, value);
}

bool GetBlob(CSalamanderRegistryAbstract* registry, HKEY key, const wchar_t* name, CBlob& blob)
{
    DWORD size = 0;
    if (!registry->GetSize(key, name, REG_BINARY, size) || size == 0 || size > 1024 * 1024)
        return false;
    CBlob data(size);
    if (!registry->GetValue(key, name, REG_BINARY, data.data(), size))
        return false;
    blob.swap(data);
    return true;
}

// the AES blob and the scrambled blob have separate value names (like FTP), so an old plugin version or a
// different master password state never mistakes one for the other
void LoadBlob(CSalamanderRegistryAbstract* registry, HKEY key, const wchar_t* encryptedName, const wchar_t* scrambledName,
              CBlob& blob)
{
    if (!GetBlob(registry, key, encryptedName, blob))
        GetBlob(registry, key, scrambledName, blob);
}

void SaveBlob(CSalamanderRegistryAbstract* registry, HKEY key, const wchar_t* encryptedName, const wchar_t* scrambledName,
              const CBlob& blob)
{
    // never leave a stale blob under the other name
    registry->DeleteValue(key, encryptedName);
    registry->DeleteValue(key, scrambledName);
    if (blob.empty())
        return;
    registry->SetValue(key, IsAesBlob(blob) ? encryptedName : scrambledName, REG_BINARY, blob.data(),
                       static_cast<DWORD>(blob.size()));
}

bool LoadSite(CSalamanderRegistryAbstract* registry, HKEY key, CSite& site, CStoredSecrets& secrets)
{
    if (!SPLRegistryGetStringOwned(registry, key, L"Name", site.name))
        return false;
    SPLRegistryGetStringOwned(registry, key, L"Host", site.host);
    SPLRegistryGetStringOwned(registry, key, L"User", site.user);
    SPLRegistryGetStringOwned(registry, key, L"KeyFile", site.keyFile);
    SPLRegistryGetStringOwned(registry, key, L"InitialRemotePath", site.initialRemotePath);
    SPLRegistryGetStringOwned(registry, key, L"InitialLocalPath", site.initialLocalPath);

    DWORD dw;
    if (GetDword(registry, key, L"Port", dw) && dw >= 1 && dw <= 65535)
        site.port = static_cast<int>(dw);
    if (GetDword(registry, key, L"Timeout", dw) && dw >= 1) // an old or edited value above the limit is clamped
        site.timeoutSeconds = static_cast<int>(dw > static_cast<DWORD>(MaxTimeoutSeconds) ? MaxTimeoutSeconds : dw);
    if (GetDword(registry, key, L"UseAgent", dw))
        site.useAgent = dw != 0;
    if (GetDword(registry, key, L"Compression", dw))
        site.compression = dw != 0;
    if (GetDword(registry, key, L"Encoding", dw))
        EncodingFromStored(dw, site.encoding);
    if (GetDword(registry, key, L"CodePage", dw) && dw != 0)
        site.codePage = dw;
    if (GetDword(registry, key, L"RememberPassword", dw))
        site.rememberPassword = dw != 0;

    if (site.rememberPassword)
    {
        LoadBlob(registry, key, L"PasswordEncrypted", L"PasswordScrambled", secrets.password);
        LoadBlob(registry, key, L"PassphraseEncrypted", L"PassphraseScrambled", secrets.passphrase);
    }
    return true;
}

void SaveSite(CSalamanderRegistryAbstract* registry, HKEY key, const CSite& site, const CStoredSecrets* secrets)
{
    SPLRegistrySetString(registry, key, L"Name", site.name);
    SetStringIfNotEmpty(registry, key, L"Host", site.host);
    SetDword(registry, key, L"Port", static_cast<DWORD>(site.port));
    SetStringIfNotEmpty(registry, key, L"User", site.user);
    SetStringIfNotEmpty(registry, key, L"KeyFile", site.keyFile);
    SetDword(registry, key, L"UseAgent", site.useAgent ? 1 : 0);
    SetDword(registry, key, L"Compression", site.compression ? 1 : 0);
    SetDword(registry, key, L"Timeout", static_cast<DWORD>(site.timeoutSeconds));
    SetDword(registry, key, L"Encoding", static_cast<DWORD>(site.encoding));
    SetDword(registry, key, L"CodePage", site.codePage);
    SetStringIfNotEmpty(registry, key, L"InitialRemotePath", site.initialRemotePath);
    SetStringIfNotEmpty(registry, key, L"InitialLocalPath", site.initialLocalPath);
    SetDword(registry, key, L"RememberPassword", site.rememberPassword ? 1 : 0);
    if (site.rememberPassword && secrets != nullptr)
    {
        SaveBlob(registry, key, L"PasswordEncrypted", L"PasswordScrambled", secrets->password);
        SaveBlob(registry, key, L"PassphraseEncrypted", L"PassphraseScrambled", secrets->passphrase);
    }
}
} // namespace

// ---- list editing with secrets ----

CStoredSecrets* CSiteConfig::FindSecrets(const std::wstring& siteName)
{
    auto it = secrets_.find(SecretsKey(siteName));
    return it == secrets_.end() ? nullptr : &it->second;
}

const CStoredSecrets* CSiteConfig::FindSecrets(const std::wstring& siteName) const
{
    auto it = secrets_.find(SecretsKey(siteName));
    return it == secrets_.end() ? nullptr : &it->second;
}

bool CSiteConfig::HasPassword(const std::wstring& siteName) const
{
    const CStoredSecrets* stored = FindSecrets(siteName);
    return stored != nullptr && !stored->password.empty();
}

bool CSiteConfig::HasPassphrase(const std::wstring& siteName) const
{
    const CStoredSecrets* stored = FindSecrets(siteName);
    return stored != nullptr && !stored->passphrase.empty();
}

void CSiteConfig::ClearSecrets(const std::wstring& siteName)
{
    auto it = secrets_.find(SecretsKey(siteName));
    if (it == secrets_.end())
        return;
    WipeBlob(it->second.password);
    WipeBlob(it->second.passphrase);
    secrets_.erase(it);
}

bool CSiteConfig::RemoveSite(const std::wstring& name)
{
    if (!list_.Remove(name))
        return false;
    ClearSecrets(name);
    return true;
}

bool CSiteConfig::UpdateSite(const std::wstring& name, const CSite& site)
{
    try
    {
        const std::wstring oldKey = SecretsKey(name);
        const std::wstring newKey = SecretsKey(site.name);
        if (oldKey != newKey)
        {
            auto it = secrets_.find(oldKey);
            if (it != secrets_.end())
            {
                // Everything that can throw (the copy, the new map node) happens before the list changes, so a
                // bad_alloc leaves the old state intact. Only no-throw steps follow.
                CStoredSecrets moved = it->second;
                const bool hadNewKey = secrets_.find(newKey) != secrets_.end();
                if (!hadNewKey)
                    secrets_[newKey]; // reserve the node (empty value)
                bool updated = false;
                try
                {
                    updated = list_.Update(name, site);
                }
                catch (...)
                {
                    if (!hadNewKey)
                        secrets_.erase(newKey);
                    throw;
                }
                if (!updated)
                {
                    if (!hadNewKey)
                        secrets_.erase(newKey);
                    return false;
                }
                secrets_[newKey] = std::move(moved); // node exists, vector move does not throw
                secrets_.erase(oldKey);
            }
            else if (!list_.Update(name, site))
                return false;
        }
        else if (!list_.Update(name, site))
            return false;
        if (!site.rememberPassword)
            ClearSecrets(site.name);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::wstring CSiteConfig::DuplicateSite(const std::wstring& name)
{
    try
    {
        const CStoredSecrets* source = FindSecrets(name);
        const CStoredSecrets copy = source != nullptr ? *source : CStoredSecrets();
        const std::wstring newName = list_.Duplicate(name);
        if (!newName.empty() && !copy.IsEmpty())
            secrets_[SecretsKey(newName)] = copy;
        return newName;
    }
    catch (...)
    {
        return std::wstring();
    }
}

// ---- secrets ----

bool CSiteConfig::GetSecret(const std::wstring& siteName, bool passphrase, CSecret& out, HWND parent)
{
    out.clear();
    try
    {
        const CStoredSecrets* stored = FindSecrets(siteName);
        if (stored == nullptr)
            return false;
        const CBlob& blob = passphrase ? stored->passphrase : stored->password;
        if (blob.empty())
            return false;
        if (IsAesBlob(blob) && !EnsureMasterPassword(parent))
            return false;
        return DecryptSecret(blob, out);
    }
    catch (...)
    {
        out.clear();
        return false;
    }
}

bool CSiteConfig::SetSecret(const std::wstring& siteName, bool passphrase, const CSecret& secret, HWND parent)
{
    try
    {
        if (list_.Find(siteName) == nullptr)
            return false;
        if (secret.empty())
        {
            if (CStoredSecrets* stored = FindSecrets(siteName))
            {
                WipeBlob(passphrase ? stored->passphrase : stored->password);
                if (stored->IsEmpty())
                    secrets_.erase(SecretsKey(siteName));
            }
            return true;
        }
        CSalamanderPasswordManagerAbstract* pm = SalamanderGeneral->GetSalamanderPasswordManager();
        if (pm == nullptr)
            return false;
        const bool aes = pm->IsUsingMasterPassword() != FALSE;
        if (aes && !EnsureMasterPassword(parent))
            return false;
        CBlob blob;
        if (!EncryptSecret(secret, aes, blob))
            return false;
        CStoredSecrets& stored = secrets_[SecretsKey(siteName)];
        CBlob& target = passphrase ? stored.passphrase : stored.password;
        WipeBlob(target);
        target.swap(blob);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool CSiteConfig::GetPassword(const std::wstring& siteName, CSecret& out, HWND parent)
{
    return GetSecret(siteName, false, out, parent);
}

bool CSiteConfig::GetPassphrase(const std::wstring& siteName, CSecret& out, HWND parent)
{
    return GetSecret(siteName, true, out, parent);
}

bool CSiteConfig::SetPassword(const std::wstring& siteName, const CSecret& secret, HWND parent)
{
    return SetSecret(siteName, false, secret, parent);
}

bool CSiteConfig::SetPassphrase(const std::wstring& siteName, const CSecret& secret, HWND parent)
{
    return SetSecret(siteName, true, secret, parent);
}

bool CSiteConfig::ReencryptAll(bool encrypt)
{
    bool all = true;
    for (auto& entry : secrets_)
    {
        all &= ConvertBlob(entry.second.password, encrypt);
        all &= ConvertBlob(entry.second.passphrase, encrypt);
    }
    return all;
}

// ---- registry ----

void CSiteConfig::Load(HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    loaded_ = false;
    try
    {
        list_.Clear();
        for (auto& entry : secrets_)
        {
            WipeBlob(entry.second.password);
            WipeBlob(entry.second.passphrase);
        }
        secrets_.clear();
        if (regKey == nullptr || registry == nullptr)
        {
            loaded_ = true; // nothing saved yet
            return;
        }

        DWORD version = 0;
        GetDword(registry, regKey, kConfigVersion, version);
        if (version > kCurrentVersion)
        {
            TRACE_E("SFTP: configuration version " << version << " is newer than supported; sites are not loaded or saved.");
            return;
        }

        HKEY sitesKey;
        if (registry->OpenKey(regKey, kSitesKey, sitesKey))
        {
            for (int i = 1; i <= kMaxSites; i++)
            {
                HKEY itemKey;
                if (!registry->OpenKey(sitesKey, std::to_wstring(i).c_str(), itemKey))
                    break;
                CSite site;
                CStoredSecrets secrets;
                const bool read = LoadSite(registry, itemKey, site, secrets);
                registry->CloseKey(itemKey);
                // an unreadable or clashing entry is dropped (it is rewritten without it on the next save)
                if (read && list_.Add(site) && !secrets.IsEmpty())
                    secrets_[SecretsKey(site.name)] = secrets;
            }
            registry->CloseKey(sitesKey);
        }
        loaded_ = true;
    }
    catch (...)
    {
        TRACE_E("SFTP: out of memory while loading the sites.");
        list_.Clear();
        secrets_.clear();
        loaded_ = false; // Save must not wipe what is stored
    }
}

void CSiteConfig::Save(HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    if (!loaded_ || regKey == nullptr || registry == nullptr)
        return;
    try
    {
        EnsureDirectory(AppDataSallyDir()); // known_hosts and other plugin files live there
        SetDword(registry, regKey, kConfigVersion, kCurrentVersion);

        HKEY sitesKey;
        if (!registry->CreateKey(regKey, kSitesKey, sitesKey))
            return;
        registry->ClearKey(sitesKey);
        for (size_t i = 0; i < list_.Count(); i++)
        {
            HKEY itemKey;
            if (!registry->CreateKey(sitesKey, std::to_wstring(i + 1).c_str(), itemKey))
                break;
            const CSite& site = list_.At(i);
            SaveSite(registry, itemKey, site, FindSecrets(site.name));
            registry->CloseKey(itemKey);
        }
        registry->CloseKey(sitesKey);
    }
    catch (...)
    {
        TRACE_E("SFTP: out of memory while saving the sites.");
    }
}
