#pragma once

#include <gtk/gtk.h>
#include <webkit/webkit.h>

struct BrowserTab
{
    GtkWidget* row;
    GtkWidget* button;
    GtkWidget* close_button;
    GtkWidget* media_button;
    GtkWidget* play_button;
    GtkWidget* mute_button;
    GtkWidget* web_view;
    WebKitNetworkSession* network_session;
    WebKitUserContentManager* user_content_manager;
    GtkWidget* content;
    GtkWidget* favicon;
    GtkWidget* label;
    bool is_home;
    bool is_private;
};

using BrowserTabChangedCallback = void (*)(BrowserTab* tab);
using BrowserTabProgressCallback = void (*)(BrowserTab* tab);

void tab_manager_init(
    GtkWidget* stack,
    GtkWidget* sidebar
);

void tab_manager_set_changed_callback(
    BrowserTabChangedCallback callback
);

void tab_manager_set_progress_callback(
    BrowserTabProgressCallback callback
);

void tab_manager_set_sidebar_compact(
    bool compact
);

BrowserTab* browser_tab_create(
    const char* uri
);

BrowserTab* browser_tab_create_home();
BrowserTab* browser_tab_create_private();

void browser_tab_select(
    BrowserTab* tab
);

void browser_tab_close(
    BrowserTab* tab
);

void browser_tab_select_relative(
    int direction
);

BrowserTab* browser_tab_current();
