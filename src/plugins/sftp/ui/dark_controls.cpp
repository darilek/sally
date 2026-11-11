#include "precomp.h"

#include <commctrl.h>
#include <string>

#include "dark_controls.h"
#include "plugindarkmode.h"

// A window subclass (SetWindowSubclass) is a hook in front of a control's own window procedure,
// roughly like overriding OnPaint in a derived WinForms control.

namespace
{
const UINT_PTR SUBCLASS_ID = 0x53465450; // any value unique for this subclass

std::wstring ReadHeaderText(HWND header, int index, UINT& format)
{
    wchar_t text[256] = {};
    HDITEMW item = {};
    item.mask = HDI_TEXT | HDI_FORMAT;
    item.pszText = text;
    item.cchTextMax = 256;
    format = 0;
    if (SendMessageW(header, HDM_GETITEMW, index, reinterpret_cast<LPARAM>(&item)))
        format = item.fmt;
    return text;
}

void PaintHeader(HWND header, HDC dc, const PluginDarkModeColors& colors)
{
    RECT client;
    GetClientRect(header, &client);
    PluginDarkMode_FillRect(dc, &client, colors.DialogBackground);

    SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(header, WM_GETFONT, 0, 0)));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, colors.DialogText);
    HPEN pen = CreatePen(PS_SOLID, 1, colors.Border);
    HGDIOBJ oldPen = SelectObject(dc, pen);

    const int count = static_cast<int>(SendMessageW(header, HDM_GETITEMCOUNT, 0, 0));
    for (int i = 0; i < count; i++)
    {
        RECT rc;
        if (!SendMessageW(header, HDM_GETITEMRECT, i, reinterpret_cast<LPARAM>(&rc)))
            continue;
        UINT format;
        const std::wstring text = ReadHeaderText(header, i, format);
        UINT flags = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX;
        flags |= (format & HDF_JUSTIFYMASK) == HDF_RIGHT    ? DT_RIGHT
                 : (format & HDF_JUSTIFYMASK) == HDF_CENTER ? DT_CENTER
                                                            : DT_LEFT;
        RECT textRect = rc;
        InflateRect(&textRect, -6, 0);
        DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &textRect, flags);

        // divider on the right edge and line under the item
        MoveToEx(dc, rc.right - 1, rc.top + 2, NULL);
        LineTo(dc, rc.right - 1, rc.bottom - 2);
        MoveToEx(dc, rc.left, rc.bottom - 1, NULL);
        LineTo(dc, rc.right, rc.bottom - 1);
    }
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void PaintTabs(HWND tabs, HDC dc, const PluginDarkModeColors& colors)
{
    RECT client;
    GetClientRect(tabs, &client);
    PluginDarkMode_FillRect(dc, &client, colors.DialogBackground);

    SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(tabs, WM_GETFONT, 0, 0)));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, colors.DialogText);
    HPEN pen = CreatePen(PS_SOLID, 1, colors.Border);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));

    const int selected = static_cast<int>(SendMessageW(tabs, TCM_GETCURSEL, 0, 0));
    const int count = static_cast<int>(SendMessageW(tabs, TCM_GETITEMCOUNT, 0, 0));

    // The frame around the page area: it starts under the row of tabs (the bottom of the first tab) and runs to the
    // edges of the control. The selected tab opens the top line below it (see the loop).
    RECT firstTab = {};
    if (count > 0)
        SendMessageW(tabs, TCM_GETITEMRECT, 0, reinterpret_cast<LPARAM>(&firstTab));
    const int frameTop = count > 0 ? firstTab.bottom : 0;
    Rectangle(dc, client.left, frameTop, client.right, client.bottom);

    for (int i = 0; i < count; i++)
    {
        RECT rc;
        if (!SendMessageW(tabs, TCM_GETITEMRECT, i, reinterpret_cast<LPARAM>(&rc)))
            continue;
        // like the light theme: the selected tab is raised (taller and wider than the others), the others sit lower
        if (i == selected)
        {
            rc.left -= 2;
            rc.right += 2;
            rc.top -= 2;
        }
        else
        {
            rc.top += 1;
        }
        wchar_t text[128] = {};
        TCITEMW item = {};
        item.mask = TCIF_TEXT;
        item.pszText = text;
        item.cchTextMax = 128;
        SendMessageW(tabs, TCM_GETITEMW, i, reinterpret_cast<LPARAM>(&item));

        if (i == selected)
        {
            // the selected tab stands out slightly and is open at the bottom, joined with the page area
            RECT fill = {rc.left, rc.top, rc.right, rc.bottom + 1};
            PluginDarkMode_FillRect(dc, &fill, colors.InputBackground);
            MoveToEx(dc, rc.left, rc.bottom, NULL);
            LineTo(dc, rc.left, rc.top);
            LineTo(dc, rc.right - 1, rc.top);
            LineTo(dc, rc.right - 1, rc.bottom);
        }
        else
        {
            Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
        }
        DrawTextW(dc, text, -1, &rc, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
    }
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

enum class Kind
{
    Header,
    Tabs
};

LRESULT CALLBACK DarkSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data)
{
    try
    {
        if (msg == WM_NCDESTROY)
            RemoveWindowSubclass(hwnd, DarkSubclassProc, id);
        else if (msg == WM_ERASEBKGND)
            return 1; // WM_PAINT fills everything
        else if (msg == WM_PAINT || msg == WM_PRINTCLIENT)
        {
            PluginDarkModeColors colors;
            if (PluginDarkMode_GetColors(&colors))
            {
                PAINTSTRUCT ps;
                HDC dc = msg == WM_PAINT ? BeginPaint(hwnd, &ps) : reinterpret_cast<HDC>(wParam);
                if (static_cast<Kind>(data) == Kind::Header)
                    PaintHeader(hwnd, dc, colors);
                else
                    PaintTabs(hwnd, dc, colors);
                if (msg == WM_PAINT)
                    EndPaint(hwnd, &ps);
                return 0;
            }
        }
    }
    catch (...)
    {
        TRACE_E("DarkSubclassProc: unexpected exception.");
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}
} // namespace

void ApplyDarkListHeader(HWND listView)
{
    if (!PluginDarkMode_ShouldUseDark() || listView == NULL)
        return;
    HWND header = ListView_GetHeader(listView);
    if (header != NULL)
        SetWindowSubclass(header, DarkSubclassProc, SUBCLASS_ID, static_cast<DWORD_PTR>(Kind::Header));
}

void ApplyDarkTabControl(HWND tabControl)
{
    if (!PluginDarkMode_ShouldUseDark() || tabControl == NULL)
        return;
    SetWindowSubclass(tabControl, DarkSubclassProc, SUBCLASS_ID, static_cast<DWORD_PTR>(Kind::Tabs));
    InvalidateRect(tabControl, NULL, TRUE);
}
