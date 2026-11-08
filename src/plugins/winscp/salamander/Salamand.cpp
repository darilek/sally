// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-FileCopyrightText: 2026 Sally Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------------
#define NO_WIN32_LEAN_AND_MEAN
#include <vcl.h>
#include <Finally.h>
#pragma hdrstop

#include <Consts.hpp>
#include "Salamand.h"
#include "Salamander.rh"
#include "FileSystem.h"
#include "FileSystemInterface.h"
#include "MenuInterface.h"
#include "TextsWin.h"
#include "CoreMain.h"
#include "SalamandConfiguration.h"

#include <Common.h>
#include <CoreMain.h>
#include <SessionData.h>
#include <WinInterface.h>
#include <Terminal.h>
#include <VCLCommon.h>
#include "winliblt.h"
#include "SalamandText.h"
//---------------------------------------------------------------------------
CPluginInterface PluginInterface;
HINSTANCE DLLInstance = NULL;
HINSTANCE HLanguage = NULL;
CSalamanderDebugAbstract* SalamanderDebug = NULL;
int SalamanderVersion = 0;
// required by checkkey.cpp
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;
//---------------------------------------------------------------------------
namespace
{
bool WideToWinScpNameBytes(const wchar_t* Value, AnsiString& Result)
{
    Result = "";
    if (Value == NULL)
        return true;

    BOOL UsedDefault = FALSE;
    const int Required = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS,
                                              Value, -1, NULL, 0, NULL,
                                              &UsedDefault);
    if (Required <= 0 || UsedDefault)
        return false;

    std::string Storage(static_cast<size_t>(Required), '\0');
    UsedDefault = FALSE;
    if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, Value, -1,
                            Storage.data(), Required, NULL, &UsedDefault) != Required ||
        UsedDefault)
        return false;

    Result = Storage.c_str();
    return true;
}
} // namespace
//---------------------------------------------------------------------------
int WINAPI DllEntryPoint(HINSTANCE HInst, unsigned long Reason,
                         void* /*Reserved*/)
{
    BOOL Result = TRUE;

    switch (Reason)
    {
    case DLL_PROCESS_ATTACH:
        DLLInstance = HInst;
        break;

    case DLL_PROCESS_DETACH:
        try
        {
            FinalizeSystemSettings();
            FinalizeWinHelp();
            CoreFinalize();
        }
        catch (...)
        {
        }
        break;
    }

    return Result;
}
//---------------------------------------------------------------------------
// WinSCP formats its messages as AnsiString, so it gets resource strings as code-page bytes.
char* SalLoadStr(int resID)
{
    static char failed[] = "ERROR LOADING STRING";
    std::wstring wide;
    std::string bytes;
    if (SalamanderGeneral == NULL ||
        !SPLLoadStrOwned(::SalamanderGeneral, HLanguage, resID, wide) ||
        !SalNarrowExact(wide.c_str(), bytes))
        return failed;
    return RetainBoundedText(std::move(bytes));
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Last line of defence at SDK entry points: Sally is built with MSVC and cannot
// catch exceptions thrown by this (clang/bcc64x) module, so they must stop here.
void PluginBoundaryException(const char* Where, Exception* E, bool Show)
{
    try
    {
        if (Show && (E != NULL))
        {
            ShowExtendedException(E);
        }
    }
    catch (...)
    {
    }
}
//---------------------------------------------------------------------------
#define DLLEXPORT extern "C" __declspec(dllexport)
//---------------------------------------------------------------------------
DLLEXPORT int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}
//---------------------------------------------------------------------------
DLLEXPORT CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(
    CSalamanderPluginEntryAbstract* Salamander)
{
    try // PLUGIN_BOUNDARY
    {
        if (PluginInterface.Init(Salamander))
        {
            return &PluginInterface;
        }
    }
    catch (...)
    {
        PluginBoundaryException("SalamanderPluginEntry", NULL, false);
    }
    return NULL;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CSalamanderGeneralLocal::CSalamanderGeneralLocal(CPluginInterface* APlugin) : FPlugin(APlugin)
{
}
//---------------------------------------------------------------------------
CSalamanderGeneralAbstract* CSalamanderGeneralLocal::SalamanderGeneral()
{
    return FPlugin->GetSalamanderGeneral();
}
//---------------------------------------------------------------------------
CSalamanderGUIAbstract* CSalamanderGeneralLocal::SalamanderGUI()
{
    return FPlugin->GetSalamanderGUI();
}
//---------------------------------------------------------------------------
void CSalamanderGeneralLocal::SetParentWindow(HWND Parent)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneralLocal::SetParentWindow(%x)", Parent);

    assert(Application);
    assert(Parent);
    Application->Handle = Parent;
}
//---------------------------------------------------------------------------
void CSalamanderGeneralLocal::ClearParentWindow()
{
    CALL_STACK_MESSAGE1("CSalamanderGeneralLocal::ClearParentWindow()");

    assert(Application);
    Application->Handle = NULL;
}
//---------------------------------------------------------------------------
void CSalamanderGeneralLocal::ChangePanelPathOutOfPlugin(int Panel)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneralLocal::ChangePanelPathOutOfPlugin(%d)", Panel);

    SalamanderGeneral()->DisconnectFSFromPanel(SalamanderGeneral()->GetMsgBoxParent(), Panel);
}
//---------------------------------------------------------------------------
CPluginFSInterface* __fastcall CSalamanderGeneralLocal::GetPanelFS(int Panel)
{
    CALL_STACK_MESSAGE2("CSalamanderGeneralLocal::GetPanelFS(%d)", Panel);

    CPluginFSInterface* Result = NULL;
    CPluginFSInterfaceAbstract* PluginFS;
    PluginFS = SalamanderGeneral()->GetPanelPluginFS(Panel);
    if (PluginFS != NULL)
    {
        Result = dynamic_cast<CPluginFSInterface*>(PluginFS);
    }
    return Result;
}
//---------------------------------------------------------------------------
int CSalamanderGeneralLocal::SalamanderMessageDialog(HWND Parent,
                                                     const AnsiString Message, TStrings* MoreMessages, int Type, int Answers,
                                                     AnsiString /*HelpKeyword*/, const TMessageParams* Params)
{
    CALL_STACK_MESSAGE7("CSalamanderGeneralLocal::SalamanderMessageDialog(%x, %s, %p, %d, %x, %p)",
                        Parent, Message.c_str(), MoreMessages, Type, Answers, Params);

    const ResourceString* CaptionRes;
    unsigned int Flags;
    switch (Type)
    {
    case qtConfirmation:
        Flags = MSGBOXEX_ICONQUESTION;
        CaptionRes = &_SMsgDlgConfirm;
        break;

    case qtWarning:
        Flags = MSGBOXEX_ICONEXCLAMATION;
        CaptionRes = &_SMsgDlgWarning;
        break;

    case qtError:
        Flags = MSGBOXEX_ICONHAND;
        CaptionRes = &_SMsgDlgError;
        break;

    case qtInformation:
        Flags = MSGBOXEX_ICONINFORMATION;
        CaptionRes = &_SMsgDlgInformation;
        break;

    default:
        assert(false);
    }
    AnsiString Caption = LoadResourceString(CaptionRes);

    AnsiString Text = Message;
    if ((MoreMessages != NULL) && !MoreMessages->Text.IsEmpty())
    {
        Text += "\n \n" + MoreMessages->Text;
    }

    AnsiString AliasBtnNames;
    if (FLAGSET(Answers, qaYes | qaNo | qaCancel))
    {
        // possible qaRetry | qaNoToAll | qaYesToAll | qaAll is ignored
        Flags |= MSGBOXEX_YESNOCANCEL;
    }
    else if (FLAGSET(Answers, qaOK | qaAbort) || FLAGSET(Answers, qaOK | qaCancel))
    {
        Flags |= MSGBOXEX_OKCANCEL;
    }
    else if (FLAGSET(Answers, qaRetry | qaAbort | qaSkip))
    {
        Flags |= MSGBOXEX_ABORTRETRYIGNORE | MSGBOXEX_ESCAPEENABLED;
        AliasBtnNames = FORMAT("%d\t%s", (DIALOG_IGNORE, LoadStr(SKIP_BUTTON)));
    }
    else if (FLAGSET(Answers, qaRetry | qaAbort))
    {
        Flags |= MSGBOXEX_RETRYCANCEL;
    }
    else if (FLAGSET(Answers, qaYes | qaNo))
    {
        Flags |= MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED;
    }
    else if (FLAGSET(Answers, qaOK))
    {
        Flags |= MSGBOXEX_OK;
    }

    BOOL CheckBoxValue = FALSE;
    AnsiString CheckBoxText;
    const std::wstring CaptionW = SalWide(Caption.c_str());
    const std::wstring TextW = SalWide(Text.c_str());
    const std::wstring AliasBtnNamesW = SalWide(AliasBtnNames.c_str());
    std::wstring CheckBoxTextW;
    MSGBOXEX_PARAMS MsgParams;
    memset(&MsgParams, 0, sizeof(MsgParams));
    MsgParams.HParent = Parent;
    MsgParams.Caption = CaptionW.c_str();
    MsgParams.Text = TextW.c_str();
    MsgParams.Flags = Flags;
    if (!AliasBtnNames.IsEmpty())
    {
        MsgParams.AliasBtnNames = AliasBtnNamesW.c_str();
    }
    bool NeverAskAgainCheck = ((Params != NULL) && FLAGSET(Params->Params, mpNeverAskAgainCheck));
    if (NeverAskAgainCheck)
    {
        MsgParams.CheckBoxValue = &CheckBoxValue;
        CheckBoxText = LoadStr(Answers == qaOK ? NEVER_SHOW_AGAIN : NEVER_ASK_AGAIN);
        CheckBoxTextW = SalWide(CheckBoxText.c_str());
        MsgParams.CheckBoxText = CheckBoxTextW.c_str();
    }
    // TODO: map TMessageParams::Aliases to MSGBOXEX_PARAMS::AliasBtnNames
    // (currently only qaAll is mapped and qaRetry for yes/no/cancel/retry=append/...,
    // where both are ignored)

    int Answer;
    switch (SalamanderGeneral()->SalMessageBoxEx(&MsgParams))
    {
    case DIALOG_OK:
        Answer = qaOK;
        break;

    case DIALOG_YES:
        Answer = qaYes;
        break;

    case DIALOG_NO:
        Answer = qaNo;
        break;

    case DIALOG_ABORT:
        Answer = qaAbort;
        break;

    case DIALOG_RETRY:
        Answer = qaRetry;
        break;

    case DIALOG_IGNORE:
        Answer = qaSkip;
        break;

    case DIALOG_CANCEL:
    default:
        Answer = CancelAnswer(Answers);
        break;
    }

    if (NeverAskAgainCheck && CheckBoxValue &&
        (Answer == qaYes || Answer == qaOK || Answer == qaYesToAll))
    {
        Answer = qaNeverAskAgain;
    }

    return Answer;
}
//---------------------------------------------------------------------------
void CSalamanderGeneralLocal::BugReport(AnsiString Report)
{
    CALL_STACK_MESSAGE3("CSalamanderGeneralLocal::BugReport(%p, %s)",
                        this, Report.c_str());

    int DivByZero = 0;
    DivByZero = 1 / DivByZero;
    USEDPARAM(DivByZero);
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
CPluginInterface::CPluginInterface() : CSalamanderGeneralLocal(this)
{
    FInterfaceForFS = NULL;
    FInterfaceForMenuExt = NULL;
    FSalamanderGeneral = NULL;
    FSalamanderGUI = NULL;
}
//---------------------------------------------------------------------------
CPluginInterface::~CPluginInterface()
{
    // TODO
    TControl** HintControl = (TControl**)(((unsigned char*)Application) + 96);
    TLabel* Label = new TLabel(Application);
    *HintControl = Label;
    { // try/__finally -> scope guard (bcc64x)
      FINALLY_GUARD([&]() {
        *HintControl = NULL;
    });
        Application->CancelHint();
    }

    ClearParentWindow();
    assert(!FInterfaceForFS);
    assert(!FInterfaceForMenuExt);
}
//---------------------------------------------------------------------------
void CPluginInterface::ConnectFileSystem(int Panel)
{
    CALL_STACK_MESSAGE2("CPluginInterface::ConnectFileSystem(%d)", Panel);

    assert(FInterfaceForFS);
    FInterfaceForFS->ConnectFileSystem(Panel);
}
//---------------------------------------------------------------------------
void CPluginInterface::Help()
{
    CALL_STACK_MESSAGE1("CPluginInterface::Help()");

    HWND Parent = ((Screen->ActiveForm != NULL) ? Screen->ActiveForm->Handle : SalamanderGeneral()->GetMsgBoxParent());
    SalamanderGeneral()->OpenHtmlHelp(Parent, HHCDisplayTOC, 0, FALSE);
}
//---------------------------------------------------------------------------
void CPluginInterface::HelpContext(unsigned int HelpContext)
{
    CALL_STACK_MESSAGE2("CPluginInterface::HelpContext(%u)", HelpContext);

    SalamanderGeneral()->OpenHtmlHelp(ParentWindow(), HHCDisplayContext, HelpContext, FALSE);
}
//---------------------------------------------------------------------------
HWND CPluginInterface::ParentWindow()
{
    return ((Screen->ActiveForm != NULL) ? Screen->ActiveForm->Handle : SalamanderGeneral()->GetMsgBoxParent());
}
//---------------------------------------------------------------------------
void CPluginInterface::PreferencesDialog()
{
    CALL_STACK_MESSAGE1("CPluginInterface::PreferencesDialog()");

    if (DoPreferencesDialog(::pmDefault))
    {
        CPluginFSInterface* FileSystem;
        FileSystem = GetPanelFS(PANEL_SOURCE);
        if (FileSystem != NULL)
        {
            FileSystem->RefreshPanel();
        }
        FileSystem = GetPanelFS(PANEL_TARGET);
        if (FileSystem != NULL)
        {
            FileSystem->RefreshPanel();
        }
    }
}
//---------------------------------------------------------------------------
bool CPluginInterface::Init(CSalamanderPluginEntryAbstract* Salamander)
{
    SalamanderDebug = Salamander->GetSalamanderDebug();
    SalamanderVersion = Salamander->GetVersion();
    HANDLES_CAN_USE_TRACE();

    CALL_STACK_MESSAGE2("CPluginInterface::Init(%p)", Salamander);

    bool Result = FALSE;

    try
    {
        // obtain Salamander interfaces first: CoreInitialize and error reporting
        // (forms, message boxes) may already need them
        FSalamanderGeneral = Salamander->GetSalamanderGeneral();
        ::SalamanderGeneral = FSalamanderGeneral;
        FSalamanderGUI = Salamander->GetSalamanderGUI();
        HLanguage = DLLInstance;

        CoreInitialize();
        InitializeWinHelp();
        InitializeSystemSettings();

        if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
        {
            MessageBoxA(Salamander->GetParentWindow(), REQUIRE_LAST_VERSION_OF_SALAMANDER,
                       "WinSCP" /* neprekladat! */, MB_OK | MB_ICONERROR);
        }
        else
        {
            FSalamanderGeneral = Salamander->GetSalamanderGeneral();
            ::SalamanderGeneral = FSalamanderGeneral;
            FSalamanderGUI = Salamander->GetSalamanderGUI();

            assert(Application);
            SetParentWindow(SalamanderGeneral()->GetMainWindowHWND());

            InitializeWinLib(L"WinSCP" /* neprekladat! */, DLLInstance);

#ifndef IDE
            // SDK 109: LoadLanguageModule was removed, all languages live in the plugin DLL
            HLanguage = DLLInstance;
#else
            HLanguage = DLLInstance;
#endif

            SalamanderGeneral()->SetHelpFileName(L"winscp.chm");

            AnsiString ProductVersion = ::Configuration->Version;
            AnsiString Copyright = ::Configuration->FileInfoString["LegalCopyright"];
            AnsiString Description = SalLoadStr(SAL_PLUGIN_DESCRIPTION);
            Salamander->SetBasicPluginData(SalW(SalLoadStr(SAL_PLUGIN_NAME)).c_str(),
                                           FUNCTION_CONFIGURATION | FUNCTION_FILESYSTEM,
                                           SalW(ProductVersion.c_str()).c_str(), SalW(Copyright.c_str()).c_str(), SalW(Description.c_str()).c_str(),
                                           L"WinSCP" /* do not translate! */, L"", L"winscp");

            // we want to receive notifications about the creation/change/removal of the master password
            SalamanderGeneral()->SetPluginUsesPasswordManager();

            Salamander->SetPluginHomePageURL(SalW(SalLoadStr(SAL_HOMEPAGE)).c_str());

            const std::wstring FSName0 = SPLGetPluginFSNameOwned(
                SalamanderGeneral(), 0);
            if (!WideToWinScpNameBytes(FSName0.c_str(), FFSNames[0]))
                Abort();
            assert(!FFSNames[0].IsEmpty());
            int FSIndex;
            if (Salamander->AddFSName(L"scp", &FSIndex))
            {
                assert(FSIndex == 1);
                const std::wstring FSName1 = SPLGetPluginFSNameOwned(
                    SalamanderGeneral(), FSIndex);
                if (!WideToWinScpNameBytes(FSName1.c_str(), FFSNames[1]))
                    Abort();
                if (Salamander->AddFSName(L"sftp", &FSIndex))
                {
                    assert(FSIndex == 2);
                    const std::wstring FSName2 = SPLGetPluginFSNameOwned(
                        SalamanderGeneral(), FSIndex);
                    if (!WideToWinScpNameBytes(FSName2.c_str(), FFSNames[2]))
                        Abort();
                }
                else
                {
                    FFSNames[1] = "";
                }
            }

            assert(!FInterfaceForFS);
            FInterfaceForFS = new CPluginInterfaceForFS(this);
            assert(!FInterfaceForMenuExt);
            FInterfaceForMenuExt = new CPluginInterfaceForMenuExt(this);

            Result = TRUE;
        }
    }
    catch (Exception& E)
    {
        ShowExtendedException(&E);
    }
    catch (...)
    {
        throw;
    }

    return Result;
}
//---------------------------------------------------------------------------
const char* CPluginInterface::GetFSName(int Protocol, bool Current)
{
    CALL_STACK_MESSAGE3("CPluginInterface::GetFSName(%d, %d)", Protocol, Current);

    int Index = 0;
    if (Current)
    {
        switch (Protocol)
        {
        case cfsSCP:
            Index = 1;
            break;

        case cfsSFTP:
            Index = 2;
            break;
        }
    }
    else
    {
        switch (Protocol)
        {
        case fsSCPonly:
            Index = 1;
            break;

        case fsSFTPonly:
            Index = 2;
            break;
        }
    }
    if (FFSNames[Index].IsEmpty())
    {
        Index = 0;
    }
    return FFSNames[Index].c_str();
}
//---------------------------------------------------------------------------
int CPluginInterface::GetFSNameIndex(const char* FSName)
{
    CALL_STACK_MESSAGE2("CPluginInterface::GetFSNameIndex(%s)", FSName);

    int Result = 0;
    for (int Index = 0; Index < LENOF(FFSNames); Index++)
    {
        if (SameText(FSName, FFSNames[Index]))
        {
            Result = Index;
            break;
        }
    }
    return Result;
}
//---------------------------------------------------------------------------
void WINAPI CPluginInterface::About(HWND Parent)
{
    CALL_STACK_MESSAGE2("CPluginInterface::About(%x)", Parent);

    SetParentWindow(Parent);
    DoAboutDialog(::Configuration, false);
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginInterface::Release(HWND /*Parent*/, BOOL /*Force*/)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Release()");

    // debug only, must be outside try..catch
    if ((WindowsManager.WindowsCount != 0) ||
        (Screen->CustomFormCount != 0))
    {
        AnsiString FormNames;
        for (int i = 0; i < Screen->CustomFormCount; i++)
        {
            FormNames += (i > 0 ? ", " : "") + Screen->CustomForms[i]->Caption;
        }
        BugReport(FORMAT("%d WinLib windows, %d VCL windows (%s)",
                         (WindowsManager.WindowsCount, Screen->CustomFormCount, FormNames)));
    }

    try
    {
        delete FInterfaceForFS;
        FInterfaceForFS = NULL;
        delete FInterfaceForMenuExt;
        FInterfaceForMenuExt = NULL;

        AnsiString Root;
        for (int Index = 0; Index < LENOF(FFSNames); Index++)
        {
            if (!FFSNames[Index].IsEmpty())
            {
                Root = (AnsiString(FFSNames[Index]) + ":").LowerCase();
                SalamanderGeneral()->RemoveFilesFromCache(SalW(Root.c_str()).c_str());
            }
        }

        ReleaseWinLib(DLLInstance);
    }
    catch (...)
    {
    }

    // Sally unloads the DLL right after Release: detach VCL from Sally's main window
    if (Application != NULL)
    {
        Application->Handle = NULL;
    }

    return TRUE;
}
//---------------------------------------------------------------------------
void WINAPI CPluginInterface::LoadConfiguration(HWND /*Parent*/, HKEY /*RegKey*/,
                                                CSalamanderRegistryAbstract* /*Registry*/)
{
}
//---------------------------------------------------------------------------
void WINAPI CPluginInterface::SaveConfiguration(HWND /*Parent*/, HKEY /*RegKey*/,
                                                CSalamanderRegistryAbstract* /*Registry*/)
{
}
//---------------------------------------------------------------------------
void WINAPI CPluginInterface::Configuration(HWND Parent)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Configuration(%x)", Parent);

    SetParentWindow(Parent);
    PreferencesDialog();
}
//---------------------------------------------------------------------------
void WINAPI CPluginInterface::Connect(HWND Parent,
                                      CSalamanderConnectAbstract* Salamander)
{
    CALL_STACK_MESSAGE3("CPluginInterface::Connect(%x, %p)", Parent, Salamander);

    try
    {
        SetParentWindow(Parent);

        Salamander->AddMenuItem(-1, SPLLoadStrOwned(::SalamanderGeneral, HLanguage, SAL_CONNECT).c_str(),
                                SALHOTKEY('W', HOTKEYF_CONTROL | HOTKEYF_SHIFT), pmConnect,
                                FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);
        Salamander->AddMenuItem(-1, SPLLoadStrOwned(::SalamanderGeneral, HLanguage, SAL_DISCONNECT_F12).c_str(), SALHOTKEY_HINT, pmDisconnectF12,
                                FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);
        Salamander->AddMenuItem(-1, NULL, 0, 0, FALSE, 0, 0, MENU_SKILLLEVEL_ALL);
        Salamander->AddMenuItem(-1, SPLLoadStrOwned(::SalamanderGeneral, HLanguage, SAL_FULL_SYNCHRONIZE).c_str(), 0, pmFullSynchronize,
                                FALSE, MENU_EVENT_THIS_PLUGIN_FS | MENU_EVENT_TARGET_THIS_PLUGIN_FS,
                                MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);
        Salamander->AddMenuItem(-1, SPLLoadStrOwned(::SalamanderGeneral, HLanguage, SAL_SYNCHRONIZE).c_str(), 0, pmSynchronize,
                                FALSE, MENU_EVENT_THIS_PLUGIN_FS | MENU_EVENT_TARGET_THIS_PLUGIN_FS,
                                MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);

        HBITMAP HBitmap = (HBITMAP)HANDLES(LoadImage(DLLInstance,
                                                     MAKEINTRESOURCE(IDB_WINSCP), IMAGE_BITMAP, 16, 16, LR_DEFAULTCOLOR));
        Salamander->SetBitmapWithIcons(HBitmap);
        HANDLES(DeleteObject(HBitmap));
        Salamander->SetChangeDriveMenuItem(SalW(SalLoadStr(SAL_FS_NAME)).c_str(), 0);

        Salamander->SetPluginIcon(0);
        Salamander->SetPluginMenuAndToolbarIcon(0);
    }
    catch (Exception& E)
    {
        ShowExtendedException(&E);
    }
}
//---------------------------------------------------------------------------
void WINAPI CPluginInterface::ReleasePluginDataInterface(
    CPluginDataInterfaceAbstract* PluginData)
{
    try // PLUGIN_BOUNDARY: never let exceptions unwind into Sally (MSVC) frames
    {
        CALL_STACK_MESSAGE2("CPluginInterface::ReleasePluginDataInterface(%p)", PluginData);
    
        CPluginFSDataInterface* FSDataInterface;
        FSDataInterface = dynamic_cast<CPluginFSDataInterface*>(PluginData);
        assert(FSDataInterface != NULL);
        if (FSDataInterface != NULL)
        {
            delete FSDataInterface;
        }
    }
    catch (Exception& E)
    {
        PluginBoundaryException("CPluginInterface::ReleasePluginDataInterface", &E, false);
    }
    catch (...)
    {
        PluginBoundaryException("CPluginInterface::ReleasePluginDataInterface", NULL, false);
    }
}
//---------------------------------------------------------------------------
void WINAPI CPluginInterface::ClearHistory(HWND /*Parent*/)
{
}
//---------------------------------------------------------------------------
CPluginInterfaceForArchiverAbstract* WINAPI CPluginInterface::GetInterfaceForArchiver()
{
    return NULL;
}
//---------------------------------------------------------------------------
CPluginInterfaceForViewerAbstract* WINAPI CPluginInterface::GetInterfaceForViewer()
{
    return NULL;
}
//---------------------------------------------------------------------------
CPluginInterfaceForMenuExtAbstract* WINAPI CPluginInterface::GetInterfaceForMenuExt()
{
    CALL_STACK_MESSAGE1("CPluginInterface::GetInterfaceForMenuExt()");

    return FInterfaceForMenuExt;
}
//---------------------------------------------------------------------------
CPluginInterfaceForFSAbstract* WINAPI CPluginInterface::GetInterfaceForFS()
{
    CALL_STACK_MESSAGE1("CPluginInterface::GetInterfaceForFS()");

    return FInterfaceForFS;
}
//---------------------------------------------------------------------------
CPluginInterfaceForThumbLoaderAbstract* WINAPI CPluginInterface::GetInterfaceForThumbLoader()
{
    return NULL;
}
//---------------------------------------------------------------------------
void WINAPI CPluginInterface::Event(int /*Event*/, DWORD /*Param*/)
{
}
//---------------------------------------------------------------------------
void CPluginInterface::RecryptPasswords()
{
    CALL_STACK_MESSAGE1("CPluginInterface::RecryptPasswords()");
    FInterfaceForFS->RecryptPasswords();
}
//---------------------------------------------------------------------------
void WINAPI CPluginInterface::PasswordManagerEvent(HWND Parent, int /*Event*/)
{
    CALL_STACK_MESSAGE1("CPluginInterface::RecryptPasswords()");
    try
    {
        SetParentWindow(Parent);
        (dynamic_cast<TSalamandConfiguration*>(::Configuration))->RecryptPasswords();
    }
    catch (Exception& E)
    {
        ShowExtendedException(&E);
    }
}
//---------------------------------------------------------------------------
