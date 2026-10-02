#pragma once

#include <gtk/gtk.h>
#include <webkit/webkit.h>

GtkWidget* create_browser_view();

void browser_go_back();
void browser_go_forward();
void browser_reload();
void browser_stop();
void browser_load_uri(const char* uri);
