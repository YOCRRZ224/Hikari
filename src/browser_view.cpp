#include "browser_view.hpp"
#include "tab_manager.hpp"

static WebKitWebView* current_web_view()
{
    BrowserTab* tab = browser_tab_current();
    return tab ? WEBKIT_WEB_VIEW(tab->web_view) : nullptr;
}

GtkWidget* create_browser_view()
{
    BrowserTab* tab = browser_tab_create("https://example.com");
    return tab->web_view;
}

void browser_go_back()
{
    WebKitWebView* web_view = current_web_view();
    if (web_view && webkit_web_view_can_go_back(web_view))
    {
        webkit_web_view_go_back(web_view);
    }
}

void browser_go_forward()
{
    WebKitWebView* web_view = current_web_view();
    if (web_view && webkit_web_view_can_go_forward(web_view))
    {
        webkit_web_view_go_forward(web_view);
    }
}

void browser_reload()
{
    WebKitWebView* web_view = current_web_view();
    if (web_view)
        webkit_web_view_reload(web_view);
}

void browser_stop()
{
    WebKitWebView* web_view = current_web_view();
    if (web_view)
        webkit_web_view_stop_loading(web_view);
}

void browser_load_uri(const char* uri)
{
    WebKitWebView* web_view = current_web_view();
    if (web_view)
        webkit_web_view_load_uri(web_view, uri);
}
