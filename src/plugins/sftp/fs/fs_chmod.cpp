// SPDX-FileCopyrightText: 2026 Sally Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// Ctrl+F2 (change attributes): the ChangeAttributes service of the SFTP file system.
//
// The plugin shows its own dialog (ui/chmod_dialog.*). Then one item without recursion is changed at once with a request
// on the panel session, in the safe wait window (ESC cancels), like F2/F7. Anything else (several items, or a recursive
// change of a directory) becomes a "chmod" operation: workers with their own connections walk the tree and change the
// items, and the operation window shows the progress, so the panel stays usable.
//
// Sally's protocol: TRUE = done, Sally deselects the items; FALSE = cancelled or failed, nothing is deselected.

#include "precomp.h"

#include "fs_object.h"

#include "../ui/chmod_dialog.h"
#include "fs_data.h"
#include "listing_format.h"
#include "operation.h"
#include "operations_registry.h"
#include "site_config.h"
#include "text_util.h"
#include "wait_for_request.h"

namespace
{
std::wstring DescribeError(const CSshError& error)
{
    const CSshErrorLabels labels = {LangStr(IDS_ERR_UNKNOWN), LangStr(IDS_ERR_CODE), LangStr(IDS_ERR_SFTP_STATUS)};
    return DescribeSshError(error, labels);
}

void ShowMessage(HWND parent, const std::wstring& text)
{
    SalamanderGeneral->SalMessageBox(parent, text.c_str(), LangStr(IDS_PLUGINNAME).c_str(),
                                     MB_OK | MB_ICONEXCLAMATION);
}

// "host:port" as the operation window names the server.
std::wstring HostTextOf(const CSite& site)
{
    const CNameCodec utf8 = CNameCodec::Utf8();
    return utf8.Decode(FormatHostPort(utf8.Encode(site.host), site.port));
}

// A real directory: a link to a directory is not (chmod on it would change its target, so it is never walked into).
bool IsRealDirectory(bool isDir, bool isLink)
{
    return isDir && !isLink;
}
} // namespace

BOOL WINAPI
CPluginFSInterface::ChangeAttributes(const wchar_t* fsName, HWND parent, int panel, int selectedFiles, int selectedDirs)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::ChangeAttributes(, , %d, %d, %d)", panel, selectedFiles, selectedDirs);
    CBusyGuard busy(this); // declared first: destroyed last, and may delete 'this' (see ChangePath)
    try
    {
        return DoChangeAttributes(parent, panel, selectedFiles, selectedDirs);
    }
    catch (...) // no exception may leave an SDK entry point
    {
        TRACE_E("ChangeAttributes: unexpected exception.");
        return FALSE;
    }
}

BOOL CPluginFSInterface::DoChangeAttributes(HWND parent, int panel, int selectedFiles, int selectedDirs)
{
    if (closeRequested_ || !connected_ || IsWaiting())
        return FALSE;
    std::vector<CSelectedItem> items;
    if (!CollectSelection(panel, selectedFiles, selectedDirs, items))
        return FALSE;
    if (!ResolveCurrentModes(parent, items))
        return FALSE; // cancelled with ESC while the modes were read

    std::vector<uint32_t> modes;
    bool anyDirectory = false;
    for (const CSelectedItem& item : items)
    {
        modes.push_back(PermissionBitsOf(item.mode));
        anyDirectory = anyDirectory || IsRealDirectory(item.isDir, item.isLink);
    }
    CChmodDialogInput input;
    input.name = codec_.Decode(items[0].rawName);
    input.itemCount = static_cast<int>(items.size());
    input.anyDirectory = anyDirectory;
    input.state = StateOfModes(modes);
    CChmodChoice choice;
    if (!ShowChmodDialog(parent, input, choice))
        return FALSE;
    if (closeRequested_ || !connected_) // the connection can go away while the dialog is open
        return FALSE;

    // one item, nothing to walk through: a plain request. A single directory without recursion is such an item too.
    const bool single = items.size() == 1 && !choice.recursive;
    const bool done = single ? ChmodOneOnSession(parent, items[0], choice.change)
                             : StartChmodOperation(items, choice.change, choice.recursive, choice.applyToFiles,
                                                   choice.applyToDirs);
    return done ? TRUE : FALSE;
}

// The listing has the mode of a link itself (always rwxrwxrwx) and some servers give no mode at all. For those items
// the real mode comes from a Stat request (links followed), so the dialog starts with what chmod would change.
// False: cancelled with ESC (or the FS was closed). A Stat that fails leaves the listed mode.
bool CPluginFSInterface::ResolveCurrentModes(HWND parent, std::vector<CSelectedItem>& items)
{
    for (CSelectedItem& item : items)
    {
        if (!item.isLink && item.mode != 0)
            continue;
        std::shared_ptr<CPanelRequest> request = session_->Stat(JoinRemotePath(curRaw_, item.rawName));
        if (!WaitForNameRequest(parent, request, LangStr(IDS_WAIT_CHMOD)))
            return false;
        if (request->Ok())
            item.mode = request->Attributes().permissions;
    }
    return true;
}

// Changes one item on the panel session and waits for it. True: done.
bool CPluginFSInterface::ChmodOneOnSession(HWND parent, const CSelectedItem& item, const CModeChange& change)
{
    const uint32_t mode = ApplyModeChange(PermissionBitsOf(item.mode), change);
    std::shared_ptr<CPanelRequest> request = session_->Chmod(JoinRemotePath(curRaw_, item.rawName), mode);
    if (!WaitForNameRequest(parent, request, LangStr(IDS_WAIT_CHMOD)))
    {
        SalamanderGeneral->PostRefreshPanelFS(this); // ESC: the server may have done it anyway
        return false;
    }
    if (!request->Ok())
        ShowMessage(parent, SPLFormatStringOwned(LangStr(IDS_ERR_CHMOD_ITEM).c_str(), codec_.Decode(item.rawName).c_str(),
                                                 DescribeError(request->Error()).c_str()));
    SalamanderGeneral->PostRefreshPanelFS(this); // also after a failure: the listing may have changed
    return request->Ok();
}

// Builds the queue from the selection and starts the operation (its window opens). The operation copies what it needs
// from this FS (D35) and never touches it again.
bool CPluginFSInterface::StartChmodOperation(const std::vector<CSelectedItem>& items, const CModeChange& change,
                                             bool recursive, bool applyToFiles, bool applyToDirs)
{
    CPanelConnectParams params;
    if (!MakeWorkerParams(identity_, params))
        return false;
    COperationSettings settings = SettingsFromSite(params.site, TransferDirection::Chmod, false);
    settings.remoteRoot = curRaw_;
    settings.modeChange = change;
    settings.chmodRecursive = recursive;
    settings.chmodFiles = applyToFiles;
    settings.chmodDirs = applyToDirs;

    std::vector<CRemoteSource> sources;
    for (const CSelectedItem& item : items)
    {
        CRemoteSource source;
        source.path = JoinRemotePath(curRaw_, item.rawName);
        source.isDir = IsRealDirectory(item.isDir, item.isLink);
        sources.push_back(std::move(source));
    }

    COperationLaunch launch;
    launch.source = FullFsPath(curRaw_);
    launch.hostName = HostTextOf(params.site);
    launch.changedPaths.push_back(launch.source); // the panel refreshes when a run of the operation ends
    std::vector<CQueueItem> roots = COperation::BuildChmodRoots(sources, settings, codec_);
    return OperationsRegistry.Launch(std::move(settings), std::move(params), std::move(roots), launch);
}
