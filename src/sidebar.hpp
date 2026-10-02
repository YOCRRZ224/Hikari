#pragma once

#include <gtk/gtk.h>

struct BrowserTab;
using SidebarSettingsCallback = void (*)();

GtkWidget* create_sidebar();
void sidebar_set_settings_callback(SidebarSettingsCallback callback);
void sidebar_toggle_compact();

GtkWidget* sidebar_tab_container();

void sidebar_update_current_tab(BrowserTab* tab);