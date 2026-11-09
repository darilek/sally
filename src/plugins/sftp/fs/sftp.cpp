// SPDX-FileCopyrightText: 2026 Sally Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <libssh2.h>
#include <openssl/crypto.h>

#include "SallyUrls.h"

#define SFTP_WIDEN_IMPL(value) L##value
#define SFTP_WIDEN(value) SFTP_WIDEN_IMPL(value)

// the plugin interface; the object is returned from SalamanderPluginEntry
class CPluginInterface : public CPluginInterfaceAbstract
{
public:
    virtual void WINAPI About(HWND parent);

    virtual BOOL WINAPI Release(HWND parent, BOOL force);

    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) {}
    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) {}
    virtual void WINAPI Configuration(HWND parent) {}

    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander);

    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) {}

    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() { return NULL; }
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() { return NULL; }
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt() { return NULL; }
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS();
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() { return NULL; }

    virtual void WINAPI Event(int event, DWORD param) {}
    virtual void WINAPI ClearHistory(HWND parent) {}
    virtual void WINAPI AcceptChangeOnPathNotification(const wchar_t* path, BOOL includingSubdirs) {}

    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) {}
};

CPluginInterface PluginInterface;
CPluginInterfaceForFS InterfaceForFS;

HINSTANCE DLLInstance = NULL; // handle to this DLL
HINSTANCE HLanguage = NULL;   // module with the language-dependent resources (this DLL)

CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// variable definition for "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// variable definition for "spl_com.h"
int SalamanderVersion = 0;

CSalamanderGUIAbstract* SalamanderGUI = NULL;

std::wstring AssignedFSName;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
        DLLInstance = hinstDLL;

    return TRUE; // DLL can be loaded
}

std::wstring LangStr(int resID)
{
    return SPLLoadStrOwned(SalamanderGeneral, HLanguage, resID);
}

//
// ****************************************************************************
// SalamanderPluginGetReqVer and SalamanderPluginGetSDKVer
//

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

int WINAPI SalamanderPluginGetSDKVer()
{
    return LAST_VERSION_OF_SALAMANDER; // return current SDK version
}

//
// ****************************************************************************
// SalamanderPluginEntry
//

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // set SalamanderDebug for "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // set SalamanderVersion for "spl_com.h"
    SalamanderVersion = salamander->GetVersion();

    HANDLES_CAN_USE_TRACE();
    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // this plugin is made for the current version of Salamander and higher - perform a check
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // reject older versions
        MessageBoxW(salamander->GetParentWindow(), SFTP_WIDEN(REQUIRE_LAST_VERSION_OF_SALAMANDER),
                    L"SFTP" /* do not translate */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // the plugin's resources (every language) are in its own DLL
    HLanguage = DLLInstance;

    // obtain the general Salamander interface
    SalamanderGeneral = salamander->GetSalamanderGeneral();

    // obtain the interface providing customized Windows controls used in Salamander
    SalamanderGUI = salamander->GetSalamanderGUI();

    try
    {
        // provide basic information about the plugin
        if (!salamander->SetBasicPluginData(LangStr(IDS_PLUGINNAME).c_str(), FUNCTION_FILESYSTEM,
                                            SFTP_WIDEN(VERSINFO_VERSION_NO_PLATFORM), SFTP_WIDEN(VERSINFO_COPYRIGHT),
                                            LangStr(IDS_PLUGIN_DESCRIPTION).c_str(), L"SFTP", NULL, L"sftp"))
            return NULL;

        salamander->SetPluginHomePageURL(Sally::Urls::Website);

        // obtain our FS name (it might not stay "sftp" because Salamander can adjust it)
        AssignedFSName = SPLGetPluginFSNameOwned(SalamanderGeneral, 0);
        if (AssignedFSName.empty())
            return NULL;

        // last step, so that nothing has to be undone when an earlier step fails
        const int sshInit = libssh2_init(0);
        if (sshInit != 0)
        {
            SalamanderGeneral->SalMessageBox(salamander->GetParentWindow(),
                                             SPLFormatStringOwned(LangStr(IDS_LIBSSH2_INIT_FAILED).c_str(), sshInit).c_str(),
                                             LangStr(IDS_PLUGINNAME).c_str(), MB_OK | MB_ICONERROR);
            return NULL;
        }
    }
    catch (...)
    {
        TRACE_E("SalamanderPluginEntry: out of memory.");
        return NULL;
    }

    return &PluginInterface;
}

//
// ****************************************************************************
// CPluginInterface
//

void WINAPI
CPluginInterface::About(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::About()");
    try
    {
        const std::wstring text = SPLFormatStringOwned(
            LangStr(IDS_ABOUT_FORMAT).c_str(), LangStr(IDS_PLUGINNAME).c_str(),
            SFTP_WIDEN(VERSINFO_VERSION), SFTP_WIDEN(VERSINFO_COPYRIGHT),
            LangStr(IDS_PLUGIN_DESCRIPTION).c_str(), libssh2_version(0), OpenSSL_version(OPENSSL_VERSION));
        SalamanderGeneral->SalMessageBox(parent, text.c_str(), LangStr(IDS_ABOUT).c_str(), MB_OK | MB_ICONINFORMATION);
    }
    catch (...)
    {
        TRACE_E("About: out of memory.");
    }
}

BOOL WINAPI
CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);
    libssh2_exit();
    return TRUE;
}

void WINAPI
CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");
    try
    {
        // item in the Change Drive menu and in the Drive bars (-1 = no icon yet)
        salamander->SetChangeDriveMenuItem(LangStr(IDS_CHNGDRVITEM).c_str(), -1);
    }
    catch (...)
    {
        TRACE_E("Connect: out of memory.");
    }
}

CPluginInterfaceForFSAbstract* WINAPI
CPluginInterface::GetInterfaceForFS()
{
    return &InterfaceForFS;
}
