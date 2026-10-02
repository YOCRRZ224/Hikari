#pragma once

#include <gtk/gtk.h>
#include <webkit/webkit.h>

struct BrowserTab
{
    GtkWidget* button;
    GtkWidget* web_view;
    GtkWidget* label;
};

void tab_manager_init(
    GtkWidget* stack,
    GtkWidget* sidebar
);

BrowserTab* browser_tab_create(
    const char* uri
);

void browser_tab_select(
    BrowserTab* tab
);

BrowserTab* browser_tab_current();
