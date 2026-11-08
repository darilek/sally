// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-FileCopyrightText: 2026 Sally Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------------
#define NO_WIN32_LEAN_AND_MEAN
#include <vcl.h>
#include <Finally.h>
#pragma hdrstop

#include "Salamand.h"
#include "Salamander.rh"
#include "FileSystemInterface.h"
#include "FileSystem.h"
#include "SalamandText.h"

#include <WinInterface.h>
#include <SessionData.h>
#include <CoreMain.h>
#include <Common.h>
#include <VCLCommon.h>
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
bool CPluginInterfaceForFS::FForceNewSesion = false;
const int fsmDetached = 0x0100;
//---------------------------------------------------------------------------
CPluginInterfaceForFS::CPluginInterfaceForFS(CPluginInterface* APlugin) : CPluginInterfaceForFSAbstract(), CSalamanderGeneralLocal(APlugin)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForFS::CPluginInterfaceForFS(%p)", APlugin);

    FActiveFSCount = 0;
    FSessionToOpen = NULL;
}
//---------------------------------------------------------------------------
CPluginInterfaceForFS::~CPluginInterfaceForFS()
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForFS::~CPluginInterfaceForFS()");

    assert(!FSessionToOpen);
    assert(FActiveFSCount == 0);
}
//---------------------------------------------------------------------------
void __fastcall CPluginInterfaceForFS::ParseUserPart(AnsiString UserPart,
                                                     bool UnixPath, AnsiString* ConnectInfo, AnsiString* HostName, int* PortNumber,
                                                     AnsiString* UserName, AnsiString* Password, AnsiString* Path)
{
    CALL_STACK_MESSAGE9("CPluginInterfaceForFS::ParseUserPart(%s, %d, %p, %p, %p, %p, %p, %p)",
                        UserPart.c_str(), UnixPath, ConnectInfo, HostName, PortNumber, UserName, Password, Path);

    if (UserPart.SubString(1, 2) == "//")
    {
        UserPart = UserPart.SubString(3, UserPart.Length() - 2);

        int P = 0;

        for (int Index = 0; Index < StoredSessions->Count + StoredSessions->HiddenCount; Index++)
        {
            TSessionData* AData = static_cast<TSessionData*>(StoredSessions->Items[Index]);
            if (AnsiSameText(AData->Name, UserPart))
            {
                P = UserPart.Length() + 1;
                break;
            }
            else if (AnsiSameText(AData->Name + "/", UserPart.SubString(1, AData->Name.Length() + 1)))
            {
                P = AData->Name.Length() + 1;
                break;
            }
        }

        if (P == 0)
        {
            P = UserPart.Pos("/");
            if (P == 0)
            {
                P = UserPart.Length() + 1;
            }
        }

        if (Path)
        {
            *Path = UserPart.SubString(P + 1, UserPart.Length() - P);
        }

        AnsiString AConnectInfo;
        AConnectInfo = UserPart.SubString(1, P - 1);
        if (ConnectInfo)
        {
            *ConnectInfo = AConnectInfo;
        }

        AnsiString UserInfo;
        AnsiString HostInfo;

        P = AConnectInfo.LastDelimiter("@");
        if (P > 0)
        {
            UserInfo = AConnectInfo.SubString(1, P - 1);
            HostInfo = AConnectInfo.SubString(P + 1, AConnectInfo.Length() - P);
        }
        else
        {
            HostInfo = AConnectInfo;
        }

        if (HostName)
        {
            *HostName = CutToChar(HostInfo, ':', true);
        }
        else
        {
            CutToChar(HostInfo, ':', true);
        }

        if (PortNumber)
        {
            *PortNumber = HostInfo.IsEmpty() ? -1 : StrToIntDef(HostInfo, -1);
        }

        if (UserName)
        {
            *UserName = CutToChar(UserInfo, ':', false);
        }
        else
        {
            CutToChar(UserInfo, ':', false);
        }

        if (Password)
        {
            *Password = UserInfo;
        }
    }
    else
    {
        if (HostName)
        {
            *HostName = "";
        }
        if (PortNumber)
        {
            *PortNumber = -1;
        }
        if (UserName)
        {
            *UserName = "";
        }
        if (Password)
        {
            *Password = "";
        }
        if (ConnectInfo)
        {
            *ConnectInfo = "";
        }
        if (Path)
        {
            *Path = UserPart;
        }
    }

    if (UnixPath && (Path != NULL) && !(*Path).IsEmpty())
    {
        (*Path).Insert("/", 1);
    }
}
//---------------------------------------------------------------------------
TSessionData* __fastcall CPluginInterfaceForFS::GetSessionData(
    int FSNameIndex, const AnsiString UserPart, AnsiString* Path)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForFS::GetSessionData(%d, %s, %p)",
                        FSNameIndex, UserPart.c_str(), Path);

    TSessionData* Result = new TSessionData("");
    try
    {
        bool DirectConnect = false;
        Result->Assign(StoredSessions->DefaultSettings);

        if (!UserPart.IsEmpty())
        {
            AnsiString ConnectInfo;
            AnsiString HostName;
            int PortNumber;
            AnsiString UserName;
            AnsiString Password;
            AnsiString APath;

            ParseUserPart(UserPart, false, &ConnectInfo, &HostName, &PortNumber,
                          &UserName, &Password, &APath);

            TSessionData* Duplicate;
            if (!ConnectInfo.IsEmpty() &&
                ((Duplicate = dynamic_cast<TSessionData*>(
                      StoredSessions->FindByName(ConnectInfo))) != NULL))
            {
                Result->Assign(Duplicate);
            }
            else
            {
                if (!HostName.IsEmpty())
                {
                    Result->HostName = HostName;
                }
                if (PortNumber >= 0)
                {
                    Result->PortNumber = PortNumber;
                }
                if (!UserName.IsEmpty())
                {
                    Result->UserName = UserName;
                }
                if (!Password.IsEmpty())
                {
                    Result->Password = Password;
                }
            }

            if (!APath.IsEmpty())
            {
                if (Path != NULL)
                {
                    Result->RemoteDirectory = "";
                    *Path = APath;
                }
                else
                {
                    Result->RemoteDirectory = APath;
                }
            }

            DirectConnect = Result->CanLogin;
        }

        switch (FSNameIndex)
        {
        case 1:
            Result->FSProtocol = fsSCPonly;
            break;

        case 2:
            Result->FSProtocol = fsSFTPonly;
            break;

        default:
            // leave default or stored session protocol
            break;
        }

        if (!DirectConnect && !DoLoginDialog(StoredSessions, Result, loNone))
        {
            SAFE_DESTROY(Result);
        }
    }
    catch (Exception& E)
    {
        delete Result;
        throw;
    }
    assert((Result == NULL) || (Result->CanLogin));
    return Result;
}
//---------------------------------------------------------------------------
void __fastcall CPluginInterfaceForFS::SetForceNewSession(bool value)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForFS::SetForceNewSession(%d)", value);

    FForceNewSesion = value;
}
//---------------------------------------------------------------------------
bool __fastcall CPluginInterfaceForFS::GetForceNewSession()
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForFS::GetForceNewSession()");

    return FForceNewSesion;
}
//---------------------------------------------------------------------------
void __fastcall CPluginInterfaceForFS::ConnectFileSystem(int Panel)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForFS::ConnectFileSystem(%d)", Panel);

    assert(!FSessionToOpen);
    try
    {
        SetParentWindow(SalamanderGeneral()->GetMainWindowHWND());
        // repaint Sally (e.g. area under the just-closed menu) before blocking work
        FlushThreadPaint();

        AnsiString Path;
        FSessionToOpen = GetSessionData();
        if (FSessionToOpen)
        {
            SetForceNewSession(true);
            { // try/__finally -> scope guard (bcc64x)
              FINALLY_GUARD([&]() {
                SetForceNewSession(false);
                SAFE_DESTROY(FSessionToOpen);
            });
                const std::wstring fsNameW = SalWide(FPlugin->GetFSName(FSessionToOpen->FSProtocol, false));
                SalamanderGeneral()->ChangePanelPathToPluginFS(Panel, fsNameW.c_str(), L"", NULL);
            }
        }
    }
    catch (Exception& E)
    {
        ShowExtendedException(&E);
    }
}
//---------------------------------------------------------------------------
void __fastcall CPluginInterfaceForFS::RecryptPasswords()
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForFS::RecryptPasswords");

    TFSs::const_iterator i = FFSs.begin();
    while (i != FFSs.end())
    {
        (*i)->RecryptPasswords();
        ++i;
    }
}
//---------------------------------------------------------------------------
int CPluginInterfaceForFS::SessionMenu(HWND Parent,
                                       int X, int Y, bool IsDetachedFS)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForFS::SessionMenu(%x, %d, %d, %d)",
                        Parent, X, Y, IsDetachedFS);

    char* MenuItems[4];
    memset(MenuItems, 0, sizeof(MenuItems));

    MenuItems[0] = SalLoadStr(SAL_DISCONNECT);
    MenuItems[1] = SalLoadStr(SAL_FULL_SYNCHRONIZE);
    MenuItems[2] = SalLoadStr(SAL_SYNCHRONIZE);

    HMENU Menu = CreatePopupMenu();
    MENUITEMINFO MI;
    memset(&MI, 0, sizeof(MI));
    MI.cbSize = sizeof(MI);
    MI.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
    MI.fType = MFT_STRING;
    for (int i = 0; MenuItems[i] != NULL; i++)
    {
        char* p = MenuItems[i];
        MI.wID = i + 1;
        MI.dwTypeData = p;
        MI.cch = strlen(p);
        if (((i == fsmSynchronize) || (i == fsmFullSynchronize)) && IsDetachedFS)
        {
            MI.fState = MFS_DISABLED;
        }
        else
        {
            MI.fState = MFS_ENABLED;
        }
        InsertMenuItem(Menu, i, TRUE, &MI);
    }

    int Result = TrackPopupMenuEx(Menu,
                                  TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, X, Y, Parent, NULL);
    return Result;
}
//---------------------------------------------------------------------------
CPluginFSInterfaceAbstract* WINAPI CPluginInterfaceForFS::OpenFS(
    const wchar_t* /*FSName*/, int /*FSNameIndex*/)
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForFS::OpenFS()");

    CPluginFSInterface* Result;
    try
    {
        Result = new CPluginFSInterface(FPlugin);
        Result->Connect(FSessionToOpen);
        SetForceNewSession(false);
        FFSs.insert(Result);
    }
    catch (Exception& E)
    {
        delete Result;
        Result = NULL;
        ShowExtendedException(&E);
    }
    return Result;
}
//---------------------------------------------------------------------------
void WINAPI CPluginInterfaceForFS::CloseFS(CPluginFSInterfaceAbstract* FS)
{
    try // PLUGIN_BOUNDARY: never let exceptions unwind into Sally (MSVC) frames
    {
        CALL_STACK_MESSAGE2("CPluginInterfaceForFS::CloseFS(%p)", FS);
    
        if (FS)
        {
            CPluginFSInterface* FSInterface = dynamic_cast<CPluginFSInterface*>(FS);
            assert(FSInterface);
            if (FSInterface)
            {
                FFSs.erase(FSInterface);
                delete FSInterface;
            }
            else
            {
                assert(false);
                delete FS;
            }
        }
    }
    catch (Exception& E)
    {
        PluginBoundaryException("CPluginInterfaceForFS::CloseFS", &E, false);
    }
    catch (...)
    {
        PluginBoundaryException("CPluginInterfaceForFS::CloseFS", NULL, false);
    }
}
//---------------------------------------------------------------------------
void WINAPI CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(int Panel)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(%d)", Panel);

    ConnectFileSystem(Panel);
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginInterfaceForFS::ChangeDriveMenuItemContextMenu(HWND Parent,
                                                                  int Panel, int X, int Y, CPluginFSInterfaceAbstract* PluginFS,
                                                                  const wchar_t* /*PluginFSName*/, int /*PluginFSNameIndex*/,
                                                                  BOOL IsDetachedFS, BOOL& RefreshMenu, BOOL& CloseMenu,
                                                                  int& PostCmd, void*& PostCmdParam)
{
    try // PLUGIN_BOUNDARY: never let exceptions unwind into Sally (MSVC) frames
    {
        CALL_STACK_MESSAGE7("CPluginInterfaceForFS::ChangeDriveMenuItemContextMenu(%x, %d, %d, %d, %p, %d)",
                            Parent, Panel, X, Y, PluginFS, IsDetachedFS);
    
        if (PluginFS == NULL)
        {
            return FALSE;
        }
    
        int Cmd = SessionMenu(Parent, X, Y, IsDetachedFS);
    
        bool Result = (Cmd != 0);
        if (Result)
        {
            PostCmd = Cmd | (IsDetachedFS ? fsmDetached : 0);
            PostCmdParam = PluginFS;
            RefreshMenu = FALSE;
            CloseMenu = TRUE;
        }
        return Result;
    }
    catch (Exception& E)
    {
        PluginBoundaryException("CPluginInterfaceForFS::ChangeDriveMenuItemContextMenu", &E, true);
    }
    catch (...)
    {
        PluginBoundaryException("CPluginInterfaceForFS::ChangeDriveMenuItemContextMenu", NULL, false);
    }
    return FALSE;
}
//---------------------------------------------------------------------------
void WINAPI CPluginInterfaceForFS::ExecuteChangeDrivePostCommand(int Panel, int PostCmd,
                                                                 void* PostCmdParam)
{
    try // PLUGIN_BOUNDARY: never let exceptions unwind into Sally (MSVC) frames
    {
        CALL_STACK_MESSAGE4("CPluginInterfaceForFS::ExecuteChangeDrivePostCommand(%d, %d, %p)",
                            Panel, PostCmd, PostCmdParam);
    
        int Cmd = (PostCmd & ~fsmDetached) - 1;
        bool Detached = PostCmd & fsmDetached;
        CPluginFSInterfaceAbstract* PluginFS =
            reinterpret_cast<CPluginFSInterfaceAbstract*>(PostCmdParam);
    
        CPluginFSInterface* FSInterface;
        FSInterface = dynamic_cast<CPluginFSInterface*>(PluginFS);
    
        if (Cmd == fsmClose)
        {
            if (Detached)
            {
                SalamanderGeneral()->CloseDetachedFS(
                    SalamanderGeneral()->GetMainWindowHWND(), PluginFS);
            }
            else
            {
                FSInterface->ForceClose();
                ChangePanelPathOutOfPlugin(PANEL_SOURCE);
            }
        }
        else if (Cmd == fsmFullSynchronize)
        {
            assert(!Detached);
            FSInterface->FullSynchronize(true);
        }
        else if (Cmd == fsmSynchronize)
        {
            assert(!Detached);
            FSInterface->Synchronize();
        }
        else
        {
            assert(false);
        }
    }
    catch (Exception& E)
    {
        PluginBoundaryException("CPluginInterfaceForFS::ExecuteChangeDrivePostCommand", &E, true);
    }
    catch (...)
    {
        PluginBoundaryException("CPluginInterfaceForFS::ExecuteChangeDrivePostCommand", NULL, false);
    }
}
//---------------------------------------------------------------------------
void WINAPI CPluginInterfaceForFS::ExecuteOnFS(int Panel,
                                               CPluginFSInterfaceAbstract* PluginFS, const wchar_t* PluginFSNameW,
                                               int /*PluginFSNameIndex*/, CFileData& File, int IsDir)
{
    try // PLUGIN_BOUNDARY: never let exceptions unwind into Sally (MSVC) frames
    {
        const AnsiString PluginFSName_A = SalA(PluginFSNameW);
        const char* PluginFSName = PluginFSName_A.c_str();
        CALL_STACK_MESSAGE5("CPluginInterfaceForFS::ExecuteOnFS(%d, %p, %s, %d)",
                            Panel, PluginFS, PluginFSName, IsDir);
    
        if (IsDir > 0)
        {
            CPluginFSInterface* FSInterface = dynamic_cast<CPluginFSInterface*>(PluginFS);
            assert(FSInterface != NULL);
            if (FSInterface != NULL)
            {
                AnsiString CurrentPath = FSInterface->CurrentPath();
    
                AnsiString NewPath;
                char* SuggestedFocusName;
    
                if (IsDir == 2)
                {
                    NewPath = UnixExtractFilePath(UnixExcludeTrailingBackslash(CurrentPath));
                    SuggestedFocusName = strrchr(CurrentPath.c_str(), '/');
                    if (SuggestedFocusName)
                    {
                        SuggestedFocusName++;
                    }
                }
                else
                {
                    NewPath = UnixIncludeTrailingBackslash(CurrentPath) + SalA(File.Name);
                    SuggestedFocusName = NULL;
                }
                AnsiString NewFullPath = FSInterface->FullPath(NewPath);
                const std::wstring SuggestedFocusNameW = SalW(SuggestedFocusName);
                SalamanderGeneral()->ChangePanelPathToPluginFS(Panel,
                                                               PluginFSNameW, SalW(NewFullPath.c_str()).c_str(), NULL, -1,
                                                               (SuggestedFocusName != NULL) ? SuggestedFocusNameW.c_str() : NULL);
            }
        }
    }
    catch (Exception& E)
    {
        PluginBoundaryException("CPluginInterfaceForFS::ExecuteOnFS", &E, true);
    }
    catch (...)
    {
        PluginBoundaryException("CPluginInterfaceForFS::ExecuteOnFS", NULL, false);
    }
}
//---------------------------------------------------------------------------
BOOL WINAPI CPluginInterfaceForFS::DisconnectFS(HWND Parent, BOOL IsInPanel,
                                                int Panel, CPluginFSInterfaceAbstract* PluginFS, const wchar_t* /*PluginFSName*/,
                                                int /*PluginFSNameIndex*/)
{
    try // PLUGIN_BOUNDARY: never let exceptions unwind into Sally (MSVC) frames
    {
        CALL_STACK_MESSAGE5("CPluginInterfaceForFS::DisconnectFS(%x, %d, %d, %p)",
                            Parent, IsInPanel, Panel, PluginFS);
    
        CPluginFSInterface* FSInterface;
        FSInterface = dynamic_cast<CPluginFSInterface*>(PluginFS);
        assert(FSInterface != NULL);
        FSInterface->ForceClose();
    
        BOOL Result;
        if (IsInPanel)
        {
            SalamanderGeneral()->DisconnectFSFromPanel(Parent, Panel);
            Result = (SalamanderGeneral()->GetPanelPluginFS(Panel) != PluginFS);
        }
        else
        {
            Result = SalamanderGeneral()->CloseDetachedFS(Parent, PluginFS);
        }
        return Result;
    }
    catch (Exception& E)
    {
        PluginBoundaryException("CPluginInterfaceForFS::DisconnectFS", &E, true);
    }
    catch (...)
    {
        PluginBoundaryException("CPluginInterfaceForFS::DisconnectFS", NULL, false);
    }
    return FALSE;
}
//---------------------------------------------------------------------------
