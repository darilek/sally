#include "precomp.h"

#include <commctrl.h>

#include "dark_controls.h"
#include "plugindarkmode.h"

// A window subclass (SetWindowSubclass) is a hook in front of a control's own window procedure,
// roughly like overriding OnPaint in a derived WinForms control.

namespace
{
const UINT_PTR SUBCLASS_ID = 0x53465450; // any value unique for this subclass

// Fixed buffers instead of std::wstring: nothing here may throw while a paint is open (see DarkSubclassProc).
UINT ReadHeaderText(HWND header, int index, wchar_t* text, int textSize)
{
    text[0] = 0;
    HDITEMW item = {};
    item.mask = HDI_TEXT | HDI_FORMAT;
    item.pszText = text;
    item.cchTextMax = textSize;
    return SendMessageW(header, HDM_GETITEMW, index, reinterpret_cast<LPARAM>(&item)) ? item.fmt : 0;
}

// A pen selected into a DC for the length of a function (like a using block in C#). Windows cannot delete a pen that
// is still selected into a DC (the call fails and the handle leaks), so the old pen is selected back first.
class CSelectedPen
{
public:
    CSelectedPen(HDC dc, COLORREF color) : dc_(dc), pen_(CreatePen(PS_SOLID, 1, color))
    {
        oldPen_ = SelectObject(dc_, pen_);
    }
    ~CSelectedPen()
    {
        SelectObject(dc_, oldPen_);
        DeleteObject(pen_);
    }
    CSelectedPen(const CSelectedPen&) = delete;
    CSelectedPen& operator=(const CSelectedPen&) = delete;

private:
    HDC dc_;
    HPEN pen_;
    HGDIOBJ oldPen_;
};

void PaintHeader(HWND header, HDC dc, const PluginDarkModeColors& colors)
{
    RECT client;
    GetClientRect(header, &client);
    PluginDarkMode_FillRect(dc, &client, colors.DialogBackground);

    SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(header, WM_GETFONT, 0, 0)));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, colors.DialogText);
    CSelectedPen pen(dc, colors.Border);

    const int count = static_cast<int>(SendMessageW(header, HDM_GETITEMCOUNT, 0, 0));
    for (int i = 0; i < count; i++)
    {
        RECT rc;
        if (!SendMessageW(header, HDM_GETITEMRECT, i, reinterpret_cast<LPARAM>(&rc)))
            continue;
        wchar_t text[256];
        const UINT format = ReadHeaderText(header, i, text, 256);
        UINT flags = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX;
        flags |= (format & HDF_JUSTIFYMASK) == HDF_RIGHT    ? DT_RIGHT
                 : (format & HDF_JUSTIFYMASK) == HDF_CENTER ? DT_CENTER
                                                            : DT_LEFT;
        RECT textRect = rc;
        InflateRect(&textRect, -6, 0);
        DrawTextW(dc, text, -1, &textRect, flags);

        // divider on the right edge and line under the item
        MoveToEx(dc, rc.right - 1, rc.top + 2, NULL);
        LineTo(dc, rc.right - 1, rc.bottom - 2);
        MoveToEx(dc, rc.left, rc.bottom - 1, NULL);
        LineTo(dc, rc.right, rc.bottom - 1);
    }
}

void PaintTabs(HWND tabs, HDC dc, const PluginDarkModeColors& colors)
{
    RECT client;
    GetClientRect(tabs, &client);
    PluginDarkMode_FillRect(dc, &client, colors.DialogBackground);

    SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(tabs, WM_GETFONT, 0, 0)));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, colors.DialogText);
    CSelectedPen pen(dc, colors.Border);
    SelectObject(dc, GetStockObject(NULL_BRUSH));

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
        wchar_t text[256] = {};
        TCITEMW item = {};
        item.mask = TCIF_TEXT;
        item.pszText = text;
        item.cchTextMax = 256;
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
}

// One paint: WM_PAINT gets its DC from BeginPaint (and must call EndPaint), WM_PRINTCLIENT gets a DC from the caller.
// The destructor always ends the paint and gives the DC back as it was received, also when drawing throws.
class CPaintScope
{
public:
    CPaintScope(HWND hwnd, UINT msg, WPARAM wParam) : hwnd_(hwnd), ownsPaint_(msg == WM_PAINT)
    {
        dc_ = ownsPaint_ ? BeginPaint(hwnd, &ps_) : reinterpret_cast<HDC>(wParam);
        saved_ = dc_ != NULL ? SaveDC(dc_) : 0;
    }
    ~CPaintScope()
    {
        if (saved_ != 0)
            RestoreDC(dc_, saved_);
        if (ownsPaint_)
            EndPaint(hwnd_, &ps_);
    }
    CPaintScope(const CPaintScope&) = delete;
    CPaintScope& operator=(const CPaintScope&) = delete;
    HDC Dc() const { return dc_; }

private:
    HWND hwnd_;
    bool ownsPaint_;
    PAINTSTRUCT ps_ = {};
    HDC dc_ = NULL;
    int saved_ = 0;
};

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
                CPaintScope scope(hwnd, msg, wParam);
                if (scope.Dc() != NULL)
                {
                    if (static_cast<Kind>(data) == Kind::Header)
                        PaintHeader(hwnd, scope.Dc(), colors);
                    else
                        PaintTabs(hwnd, scope.Dc(), colors);
                }
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
