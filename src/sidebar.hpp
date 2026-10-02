#pragma once

#include <gtk/gtk.h>

struct BrowserTab;

GtkWidget* create_sidebar();

void sidebar_add_tab(
    BrowserTab* tab
);

void sidebar_select_tab(
    BrowserTab* tab
);