#include "browser_view.hpp"

static WebKitWebView* web_view = nullptr;

GtkWidget* create_browser_view()
{
    web_view = WEBKIT_WEB_VIEW(
        webkit_web_view_new()
    );

    gtk_widget_set_hexpand(
        GTK_WIDGET(web_view),
        TRUE
    );

    gtk_widget_set_vexpand(
        GTK_WIDGET(web_view),
        TRUE
    );

    webkit_web_view_load_uri(
        web_view,
        "https://example.com"
    );

    return GTK_WIDGET(web_view);
}

void browser_go_back()
{
    if (web_view &&
        webkit_web_view_can_go_back(web_view))
    {
        webkit_web_view_go_back(web_view);
    }
}

void browser_go_forward()
{
    if (web_view &&
        webkit_web_view_can_go_forward(web_view))
    {
        webkit_web_view_go_forward(web_view);
    }
}

void browser_reload()
{
    if (web_view)
        webkit_web_view_reload(web_view);
}

void browser_stop()
{
    if (web_view)
        webkit_web_view_stop_loading(web_view);
}

void browser_load_uri(const char* uri)
{
    if (web_view)
        webkit_web_view_load_uri(web_view, uri);
}
