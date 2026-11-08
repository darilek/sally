// Generated: prints Sally SDK struct layouts. Build with MSVC and bcc64x and diff the output.
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <commctrl.h>
#include <stdio.h>
#include <stddef.h>
#include <string>
#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_fs.h"
#include "spl_arc.h"
#include "spl_menu.h"
#include "spl_view.h"
#include "spl_thum.h"
#include "spl_gui.h"
#include "spl_crypt.h"
#include "spl_bzip2.h"
#include "spl_zlib.h"
#include "spl_file.h"
#include "spl_buffer.h"
#define P_SIZE(T) printf("sizeof(%s) = %u\n", #T, (unsigned)sizeof(T))
#define P_OFF(T, M) printf("  offsetof(%s, %s) = %u\n", #T, #M, (unsigned)offsetof(T, M))
int main()
{
    printf("pointer size = %u\n", (unsigned)sizeof(void*));
    P_SIZE(CSalamanderStringBuffer);
    P_OFF(CSalamanderStringBuffer, Data);
    P_OFF(CSalamanderStringBuffer, Length);
    P_OFF(CSalamanderStringBuffer, Capacity);
    P_OFF(CSalamanderStringBuffer, OwnerContext);
    P_OFF(CSalamanderStringBuffer, Reserve);
    P_SIZE(CSalamanderTextRange);
    P_OFF(CSalamanderTextRange, Offset);
    P_OFF(CSalamanderTextRange, Length);
    P_SIZE(CSalamanderTextRangeBuffer);
    P_OFF(CSalamanderTextRangeBuffer, Data);
    P_OFF(CSalamanderTextRangeBuffer, Count);
    P_OFF(CSalamanderTextRangeBuffer, Capacity);
    P_OFF(CSalamanderTextRangeBuffer, OwnerContext);
    P_OFF(CSalamanderTextRangeBuffer, Reserve);
    P_SIZE(CSalamanderStringBufferOwner);
    P_SIZE(CSalamanderTextRangeBufferOwner);
    P_SIZE(CSalBZIP2);
    P_OFF(CSalBZIP2, next_in);
    P_OFF(CSalBZIP2, avail_in);
    P_OFF(CSalBZIP2, total_in);
    P_OFF(CSalBZIP2, next_out);
    P_OFF(CSalBZIP2, avail_out);
    P_OFF(CSalBZIP2, total_out);
    P_OFF(CSalBZIP2, internal);
    P_SIZE(CQuadWord);
    P_SIZE(CFileData);
    P_OFF(CFileData, Name);
    P_OFF(CFileData, Ext);
    P_OFF(CFileData, Size);
    P_OFF(CFileData, Attr);
    P_OFF(CFileData, LastWrite);
    P_OFF(CFileData, DosName);
    P_OFF(CFileData, PluginData);
    P_OFF(CFileData, NameLen);
    P_SIZE(CColumn);
    P_OFF(CColumn, Name);
    P_OFF(CColumn, Description);
    P_OFF(CColumn, GetText);
    P_OFF(CColumn, CustomData);
    P_SIZE(CSalAES);
    P_OFF(CSalAES, nonce);
    P_OFF(CSalAES, encr_bfr);
    P_OFF(CSalAES, encr_ctx);
    P_OFF(CSalAES, auth_ctx);
    P_OFF(CSalAES, encr_pos);
    P_OFF(CSalAES, pwd_len);
    P_OFF(CSalAES, mode);
    P_SIZE(CSalSHA1);
    P_OFF(CSalSHA1, state);
    P_OFF(CSalSHA1, count);
    P_OFF(CSalSHA1, buffer);
    P_SIZE(SAFE_FILE);
    P_OFF(SAFE_FILE, HFile);
    P_OFF(SAFE_FILE, FileName);
    P_OFF(SAFE_FILE, HParentWnd);
    P_OFF(SAFE_FILE, dwDesiredAccess);
    P_OFF(SAFE_FILE, dwShareMode);
    P_OFF(SAFE_FILE, dwCreationDisposition);
    P_OFF(SAFE_FILE, dwFlagsAndAttributes);
    P_OFF(SAFE_FILE, WholeFileAllocated);
    P_SIZE(MSGBOXEX_PARAMS);
    P_OFF(MSGBOXEX_PARAMS, HParent);
    P_OFF(MSGBOXEX_PARAMS, Text);
    P_OFF(MSGBOXEX_PARAMS, Caption);
    P_OFF(MSGBOXEX_PARAMS, Flags);
    P_OFF(MSGBOXEX_PARAMS, HIcon);
    P_OFF(MSGBOXEX_PARAMS, ContextHelpId);
    P_OFF(MSGBOXEX_PARAMS, HelpCallback);
    P_OFF(MSGBOXEX_PARAMS, CheckBoxText);
    P_OFF(MSGBOXEX_PARAMS, CheckBoxValue);
    P_OFF(MSGBOXEX_PARAMS, AliasBtnNames);
    P_OFF(MSGBOXEX_PARAMS, URL);
    P_OFF(MSGBOXEX_PARAMS, URLText);
    P_SIZE(CSalamanderThemeInfo);
    P_OFF(CSalamanderThemeInfo, Size);
    P_OFF(CSalamanderThemeInfo, ThemeMode);
    P_OFF(CSalamanderThemeInfo, UseDarkColors);
    P_SIZE(CSalamanderVarStrEntry);
    P_OFF(CSalamanderVarStrEntry, Name);
    P_OFF(CSalamanderVarStrEntry, Execute);
    P_SIZE(CSalamanderPluginViewerData);
    P_OFF(CSalamanderPluginViewerData, Size);
    P_OFF(CSalamanderPluginViewerData, FileName);
    P_SIZE(CSalamanderPluginInternalViewerData);
    P_OFF(CSalamanderPluginInternalViewerData, Mode);
    P_OFF(CSalamanderPluginInternalViewerData, Caption);
    P_OFF(CSalamanderPluginInternalViewerData, WholeCaption);
    P_SIZE(MENU_TEMPLATE_ITEM);
    P_OFF(MENU_TEMPLATE_ITEM, RowType);
    P_OFF(MENU_TEMPLATE_ITEM, TextResID);
    P_OFF(MENU_TEMPLATE_ITEM, SkillLevel);
    P_OFF(MENU_TEMPLATE_ITEM, ID);
    P_OFF(MENU_TEMPLATE_ITEM, ImageIndex);
    P_OFF(MENU_TEMPLATE_ITEM, State);
    P_OFF(MENU_TEMPLATE_ITEM, Enabler);
    P_SIZE(MENU_ITEM_INFO);
    P_OFF(MENU_ITEM_INFO, Mask);
    P_OFF(MENU_ITEM_INFO, Type);
    P_OFF(MENU_ITEM_INFO, State);
    P_OFF(MENU_ITEM_INFO, ID);
    P_OFF(MENU_ITEM_INFO, SubMenu);
    P_OFF(MENU_ITEM_INFO, HBmpChecked);
    P_OFF(MENU_ITEM_INFO, HBmpUnchecked);
    P_OFF(MENU_ITEM_INFO, HBmpItem);
    P_OFF(MENU_ITEM_INFO, String);
    P_OFF(MENU_ITEM_INFO, StringLen);
    P_OFF(MENU_ITEM_INFO, ImageIndex);
    P_OFF(MENU_ITEM_INFO, HIcon);
    P_OFF(MENU_ITEM_INFO, HOverlay);
    P_OFF(MENU_ITEM_INFO, CustomData);
    P_OFF(MENU_ITEM_INFO, SkillLevel);
    P_OFF(MENU_ITEM_INFO, Enabler);
    P_OFF(MENU_ITEM_INFO, Flags);
    P_SIZE(TLBI_ITEM_INFO2);
    P_OFF(TLBI_ITEM_INFO2, Mask);
    P_OFF(TLBI_ITEM_INFO2, Style);
    P_OFF(TLBI_ITEM_INFO2, State);
    P_OFF(TLBI_ITEM_INFO2, ID);
    P_OFF(TLBI_ITEM_INFO2, Text);
    P_OFF(TLBI_ITEM_INFO2, TextLen);
    P_OFF(TLBI_ITEM_INFO2, Width);
    P_OFF(TLBI_ITEM_INFO2, ImageIndex);
    P_OFF(TLBI_ITEM_INFO2, HIcon);
    P_OFF(TLBI_ITEM_INFO2, HOverlay);
    P_OFF(TLBI_ITEM_INFO2, CustomData);
    P_OFF(TLBI_ITEM_INFO2, Enabler);
    P_OFF(TLBI_ITEM_INFO2, Index);
    P_OFF(TLBI_ITEM_INFO2, Name);
    P_OFF(TLBI_ITEM_INFO2, NameLen);
    P_SIZE(TOOLBAR_PADDING);
    P_OFF(TOOLBAR_PADDING, ToolBarVertical);
    P_OFF(TOOLBAR_PADDING, ButtonIconText);
    P_OFF(TOOLBAR_PADDING, IconLeft);
    P_OFF(TOOLBAR_PADDING, IconRight);
    P_OFF(TOOLBAR_PADDING, TextLeft);
    P_OFF(TOOLBAR_PADDING, TextRight);
    P_SIZE(TOOLBAR_TOOLTIP);
    P_OFF(TOOLBAR_TOOLTIP, HToolBar);
    P_OFF(TOOLBAR_TOOLTIP, ID);
    P_OFF(TOOLBAR_TOOLTIP, Index);
    P_OFF(TOOLBAR_TOOLTIP, CustomData);
    P_OFF(TOOLBAR_TOOLTIP, Buffer);
    P_SIZE(CSalZLIB);
    P_OFF(CSalZLIB, next_in);
    P_OFF(CSalZLIB, avail_in);
    P_OFF(CSalZLIB, total_in);
    P_OFF(CSalZLIB, next_out);
    P_OFF(CSalZLIB, avail_out);
    P_OFF(CSalZLIB, total_out);
    P_OFF(CSalZLIB, internal);
    return 0;
}
