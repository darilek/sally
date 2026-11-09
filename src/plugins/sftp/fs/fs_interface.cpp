// SPDX-FileCopyrightText: 2026 Sally Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

CPluginFSInterfaceAbstract* WINAPI
CPluginInterfaceForFS::OpenFS(const wchar_t* fsName, int fsNameIndex)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForFS::OpenFS(%ls, %d)", fsName, fsNameIndex);
    // Not implemented yet. NULL is the documented failure result: Salamander treats the path
    // as unreachable (e.g. when the user types "sftp:" into the path field) and keeps the panel as it was.
    return NULL;
}

void WINAPI
CPluginInterfaceForFS::CloseFS(CPluginFSInterfaceAbstract* fs)
{
    // OpenFS never returns an object, so there is nothing to close
    CALL_STACK_MESSAGE1("CPluginInterfaceForFS::CloseFS()");
    if (fs != NULL)
        TRACE_E("CloseFS called for an FS object that was never opened.");
}

void WINAPI
CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(int panel)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(%d)", panel);
    try
    {
        SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(), LangStr(IDS_NOT_IMPLEMENTED).c_str(),
                                         LangStr(IDS_PLUGINNAME).c_str(), MB_OK | MB_ICONINFORMATION);
    }
    catch (...)
    {
        TRACE_E("ExecuteChangeDriveMenuItem: out of memory while showing the message.");
    }
}

BOOL WINAPI
CPluginInterfaceForFS::ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                                      CPluginFSInterfaceAbstract* pluginFS,
                                                      const wchar_t* pluginFSName, int pluginFSNameIndex,
                                                      BOOL isDetachedFS, BOOL& refreshMenu,
                                                      BOOL& closeMenu, int& postCmd, void*& postCmdParam)
{
    return FALSE; // no context menu
}

void WINAPI
CPluginInterfaceForFS::ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam)
{
}

void WINAPI
CPluginInterfaceForFS::ExecuteOnFS(int panel, CPluginFSInterfaceAbstract* pluginFS,
                                   const wchar_t* pluginFSName, int pluginFSNameIndex,
                                   CFileData& file, int isDir)
{
}

BOOL WINAPI
CPluginInterfaceForFS::DisconnectFS(HWND parent, BOOL isInPanel, int panel,
                                    CPluginFSInterfaceAbstract* pluginFS,
                                    const wchar_t* pluginFSName, int pluginFSNameIndex)
{
    return TRUE; // there is no FS to disconnect
}

BOOL WINAPI
CPluginInterfaceForFS::ConvertPathToInternal(const wchar_t* fsName, int fsNameIndex,
                                             CSalamanderStringBuffer* fsUserPart)
{
    return fsUserPart != NULL && sally::plugin_abi::IsValidStringBuffer(*fsUserPart);
}

BOOL WINAPI
CPluginInterfaceForFS::ConvertPathToExternal(const wchar_t* fsName, int fsNameIndex,
                                             CSalamanderStringBuffer* fsUserPart)
{
    return fsUserPart != NULL && sally::plugin_abi::IsValidStringBuffer(*fsUserPart);
}
