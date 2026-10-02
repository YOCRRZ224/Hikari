#pragma once

#include <gtk/gtk.h>
#include <webkit/webkit.h>

struct BrowserTab
{
    GtkWidget* row;
    GtkWidget* button;
    GtkWidget* close_button;
    GtkWidget* web_view;
    GtkWidget* label;
};

using BrowserTabChangedCallback = void (*)(BrowserTab* tab);

void tab_manager_init(
    GtkWidget* stack,
    GtkWidget* sidebar
);

void tab_manager_set_changed_callback(
    BrowserTabChangedCallback callback
);

void tab_manager_set_sidebar_compact(
    bool compact
);

BrowserTab* browser_tab_create(
    const char* uri
);

void browser_tab_select(
    BrowserTab* tab
);

void browser_tab_close(
    BrowserTab* tab
);

BrowserTab* browser_tab_current();
