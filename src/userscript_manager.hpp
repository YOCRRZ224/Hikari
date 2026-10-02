#pragma once

#include <gtk/gtk.h>
#include <webkit/webkit.h>

GtkWidget* userscript_manager_create_settings_page(GtkWindow* parent);

WebKitUserContentManager* userscript_manager_create_content_manager();
void userscript_manager_release_content_manager(WebKitUserContentManager* manager);