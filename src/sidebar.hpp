#pragma once

#include <gtk/gtk.h>

struct BrowserTab;

GtkWidget* create_sidebar();

GtkWidget* sidebar_tab_container();

void sidebar_update_current_tab(BrowserTab* tab);