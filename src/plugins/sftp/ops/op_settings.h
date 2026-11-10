// What one operation (a copy or move run) is asked to do. Plain data, shared by COperation and its workers.
#pragma once

#include <string>

#include "op_types.h"
#include "site.h"

enum class TransferDirection
{
    Download, // remote -> local
    Upload    // local -> remote
};

struct COperationSettings
{
    TransferDirection direction = TransferDirection::Download;
    bool move = false;                           // delete the source after a successful transfer
    ConflictPolicy policy = ConflictPolicy::Ask; // used from step 4b on
    bool preserveTime = true;                    // the target gets the source modification time
    bool safeUpload = true;                      // used from step 4b on
    bool setUploadPermissions = false;           // chmod uploaded files to uploadPermissions
    unsigned uploadPermissions = 0644;
    int workers = 2;                             // parallel connections, 1..8
    std::wstring localRoot;                      // where a download puts the selection / where an upload reads from
    std::string remoteRoot;                      // raw bytes: where an upload puts the selection
    // (the source paths themselves are the root items of the queue)
};

// The transfer options of a site (workers, safe upload, preserve time, upload permissions) copied into new settings.
COperationSettings SettingsFromSite(const CSite& site, TransferDirection direction, bool move);
