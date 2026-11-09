// The saved sites plus their remembered secrets, and their load/save through Sally's configuration registry.
// Main thread only (the Sally password manager and the registry interface are main-thread APIs).
//
// Registry layout below the plugin's root key:
//   ConfigVersion (DWORD) = 1
//   Sites\1, Sites\2, ... : Name, Host, Port, User, KeyFile, UseAgent, Compression, Timeout, Encoding (0 UTF-8,
//     1 auto, 2 code page), CodePage, InitialRemotePath, InitialLocalPath, RememberPassword and, only when
//     RememberPassword is 1, REG_BINARY blobs from the Sally password manager: PasswordEncrypted or
//     PasswordScrambled (AES with the master password / only scrambled), PassphraseEncrypted / PassphraseScrambled.
// A ConfigVersion newer than this plugin knows: nothing is loaded and nothing is ever written back, so the newer
// plugin's data survives.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "secret.h"
#include "site_list.h"

// The blobs as the password manager returned them (opaque bytes). Empty = nothing stored.
struct CStoredSecrets
{
    std::vector<BYTE> password;
    std::vector<BYTE> passphrase;

    bool IsEmpty() const { return password.empty() && passphrase.empty(); }
};

class CSiteConfig
{
public:
    // Read access to the sites. Change them only through the methods below so the secrets stay in step.
    const CSiteList& Sites() const { return list_; }

    // Adds a new site (false: invalid or duplicate name). Secrets are set separately.
    bool AddSite(const CSite& site) { return list_.Add(site); }
    bool RemoveSite(const std::wstring& name);
    // Replaces the site 'name' (may rename). The remembered secrets move with it and are dropped when the new
    // site has rememberPassword off. False: unknown, invalid or duplicate name (nothing changed).
    bool UpdateSite(const std::wstring& name, const CSite& site);
    // Copy under a unique name, with copies of the stored secrets; the new name, or empty when unknown.
    std::wstring DuplicateSite(const std::wstring& name);

    // Decrypts the remembered password / key passphrase into 'out' (wiped buffer). Asks for the master password
    // through 'parent' when it is in use and not entered yet. False: nothing remembered, the master password was
    // refused, or decryption failed ('out' is empty then).
    bool GetPassword(const std::wstring& siteName, CSecret& out, HWND parent);
    bool GetPassphrase(const std::wstring& siteName, CSecret& out, HWND parent);
    // Encrypts and stores (an empty secret clears the stored one). False: unknown site, master password refused
    // or encryption failed (the previous blob is kept).
    bool SetPassword(const std::wstring& siteName, const CSecret& secret, HWND parent);
    bool SetPassphrase(const std::wstring& siteName, const CSecret& secret, HWND parent);
    void ClearSecrets(const std::wstring& siteName);
    // True when a password / key passphrase is remembered for the site (no decryption, no master password).
    bool HasPassword(const std::wstring& siteName) const;
    bool HasPassphrase(const std::wstring& siteName) const;

    // Password manager events: converts every stored blob. encrypt == false: AES -> scrambled (needs the CURRENT
    // key); true: scrambled -> AES (needs the master password set). False when at least one blob could not be
    // converted (it is left as it was).
    bool ReencryptAll(bool encrypt);

    // Registry. regKey == NULL (no saved configuration yet) loads an empty list. Never throws.
    void Load(HKEY regKey, CSalamanderRegistryAbstract* registry);
    void Save(HKEY regKey, CSalamanderRegistryAbstract* registry);

private:
    typedef std::vector<BYTE> CBlob;
    CStoredSecrets* FindSecrets(const std::wstring& siteName);
    const CStoredSecrets* FindSecrets(const std::wstring& siteName) const;
    bool GetSecret(const std::wstring& siteName, bool passphrase, CSecret& out, HWND parent);
    bool SetSecret(const std::wstring& siteName, bool passphrase, const CSecret& secret, HWND parent);

    CSiteList list_;
    std::map<std::wstring, CStoredSecrets> secrets_; // key: site name in lower case
    bool loaded_ = false; // Save writes nothing unless Load completed (or the registry had nothing to load)
};

// The one instance of the plugin.
extern CSiteConfig SiteConfig;
