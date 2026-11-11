#pragma once

// Dark mode painting for common controls that the shared dialog helper leaves light.
// Every function does nothing in light mode. The dialog applies the theme at creation (no live theme change).

// Paints the header of a list view (report mode) dark.
void ApplyDarkListHeader(HWND listView);

// Paints a tab control (the tab strip only; the pages are child dialogs) dark.
void ApplyDarkTabControl(HWND tabControl);
