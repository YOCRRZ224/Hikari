#include "browser_window.hpp"
#include "sidebar.hpp"
#include "tab_manager.hpp"
#include "userscript_manager.hpp"
#include <algorithm>
#include <string>
#include <webkit/webkit.h>

static GtkWidget* address_bar = nullptr;
static GtkWidget* load_progress = nullptr;
static GtkWidget* back_button = nullptr;
static GtkWidget* forward_button = nullptr;
static GtkWidget* reload_button = nullptr;
static GtkWindow* browser_window = nullptr;
static GtkStack* browser_stack = nullptr;
static bool window_is_fullscreen = false;

static void on_settings_clicked()
{
    if (browser_stack)
        gtk_stack_set_visible_child_name(browser_stack, "settings");
}

static void on_tab_progress(BrowserTab* tab)
{
    WebKitWebView* view = WEBKIT_WEB_VIEW(tab->web_view);
    const bool loading = webkit_web_view_is_loading(view);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(load_progress), webkit_web_view_get_estimated_load_progress(view));
    gtk_widget_set_visible(load_progress, loading);
    gtk_widget_set_sensitive(back_button, webkit_web_view_can_go_back(view));
    gtk_widget_set_sensitive(forward_button, webkit_web_view_can_go_forward(view));
    gtk_button_set_icon_name(GTK_BUTTON(reload_button), loading ? "process-stop-symbolic" : "view-refresh-symbolic");
    gtk_widget_set_tooltip_text(reload_button, loading ? "Stop loading" : "Reload");
}

static void on_tab_changed(BrowserTab* tab)
{
    const char* uri = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(tab->web_view));
    if (!gtk_widget_has_focus(address_bar))
        gtk_editable_set_text(GTK_EDITABLE(address_bar), tab->is_home ? "" : uri ? uri : "");
    const bool secure = uri && g_str_has_prefix(uri, "https://");
    gtk_entry_set_icon_from_icon_name(
        GTK_ENTRY(address_bar),
        GTK_ENTRY_ICON_PRIMARY,
        secure ? "changes-prevent-symbolic" : "system-search-symbolic"
    );
    gtk_entry_set_icon_tooltip_text(
        GTK_ENTRY(address_bar),
        GTK_ENTRY_ICON_PRIMARY,
        secure ? "Secure connection" : "Search or site address"
    );

    const char* title = webkit_web_view_get_title(WEBKIT_WEB_VIEW(tab->web_view));
    gtk_window_set_title(browser_window, title && *title ? title : "Hikari");
    sidebar_update_current_tab(tab);

}

static void on_back_clicked(
    GtkButton* button,
    gpointer user_data
)
{
    (void)button;
    (void)user_data;

    BrowserTab* tab = browser_tab_current();
    if (tab && webkit_web_view_can_go_back(WEBKIT_WEB_VIEW(tab->web_view)))
        webkit_web_view_go_back(WEBKIT_WEB_VIEW(tab->web_view));
}

static void on_forward_clicked(
    GtkButton* button,
    gpointer user_data
)
{
    (void)button;
    (void)user_data;

    BrowserTab* tab = browser_tab_current();
    if (tab && webkit_web_view_can_go_forward(WEBKIT_WEB_VIEW(tab->web_view)))
        webkit_web_view_go_forward(WEBKIT_WEB_VIEW(tab->web_view));
}

static void on_reload_clicked(
    GtkButton* button,
    gpointer user_data
)
{
    (void)button;
    (void)user_data;

    BrowserTab* tab = browser_tab_current();
    if (!tab)
        return;

    WebKitWebView* view = WEBKIT_WEB_VIEW(tab->web_view);
    if (webkit_web_view_is_loading(view))
        webkit_web_view_stop_loading(view);
    else
        webkit_web_view_reload(view);
}

static void on_minimize_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    (void)user_data;
    gtk_window_minimize(browser_window);
}

static void on_fullscreen_clicked(GtkButton* button, gpointer user_data)
{
    (void)user_data;
    window_is_fullscreen = !window_is_fullscreen;
    if (window_is_fullscreen)
        gtk_window_fullscreen(browser_window);
    else
        gtk_window_unfullscreen(browser_window);

    gtk_button_set_icon_name(
        button,
        window_is_fullscreen ? "view-restore-symbolic" : "view-fullscreen-symbolic"
    );
    gtk_widget_set_tooltip_text(
        GTK_WIDGET(button),
        window_is_fullscreen ? "Exit fullscreen" : "Enter fullscreen"
    );
}

static void on_close_window_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    (void)user_data;
    gtk_window_close(browser_window);
}

static void on_address_activate(
    GtkEntry* entry,
    gpointer user_data
)
{
    (void)user_data;

    const char* text =
        gtk_editable_get_text(
            GTK_EDITABLE(entry)
        );

    if (!text || !*text)
        return;

    gchar* trimmed_text = g_strdup(text);
    g_strstrip(trimmed_text);
    if (!*trimmed_text)
    {
        g_free(trimmed_text);
        return;
    }

    std::string uri = trimmed_text;
    g_free(trimmed_text);

    const bool looks_like_url =
        uri.find("://") != std::string::npos ||
        uri.find('/') != std::string::npos ||
        uri.find('.') != std::string::npos ||
        uri == "localhost" ||
        uri.rfind("localhost:", 0) == 0;

    if (!looks_like_url)
    {
        gchar* encoded = g_uri_escape_string(uri.c_str(), nullptr, FALSE);
        uri = "https://www.google.com/search?q=" + std::string(encoded ? encoded : "");
        g_free(encoded);
    }
    else if (uri.find("://") == std::string::npos)
    {
        uri = "https://" + uri;
    }

    BrowserTab* tab = browser_tab_current();
    if (tab)
        webkit_web_view_load_uri(WEBKIT_WEB_VIEW(tab->web_view), uri.c_str());
}

static gboolean on_key_pressed(
    GtkEventControllerKey* controller,
    guint keyval,
    guint keycode,
    GdkModifierType state,
    gpointer user_data
)
{
    (void)controller;
    (void)keycode;
    (void)user_data;

    if ((state & GDK_ALT_MASK) && !(state & GDK_CONTROL_MASK))
    {
        BrowserTab* tab = browser_tab_current();
        if (!tab)
            return TRUE;

        WebKitWebView* view = WEBKIT_WEB_VIEW(tab->web_view);
        if (keyval == GDK_KEY_Left && webkit_web_view_can_go_back(view))
            webkit_web_view_go_back(view);
        else if (keyval == GDK_KEY_Right && webkit_web_view_can_go_forward(view))
            webkit_web_view_go_forward(view);
        else
            return FALSE;
        return TRUE;
    }

    if (!(state & GDK_CONTROL_MASK))
        return FALSE;

    const guint lowered_keyval = gdk_keyval_to_lower(keyval);
    if (lowered_keyval == GDK_KEY_s && (state & GDK_SHIFT_MASK))
    {
        sidebar_toggle_compact();
        return TRUE;
    }
    if (lowered_keyval == GDK_KEY_n)
    {
        if (state & GDK_SHIFT_MASK)
            browser_tab_create_private();
        else
            browser_tab_create_home();
        return TRUE;
    }

    if (keyval == GDK_KEY_ISO_Left_Tab)
    {
        browser_tab_select_relative(-1);
        return TRUE;
    }

    switch (gdk_keyval_to_lower(keyval))
    {
        case GDK_KEY_t:
            browser_tab_create_home();
            return TRUE;
        case GDK_KEY_w:
            browser_tab_close(browser_tab_current());
            return TRUE;
        case GDK_KEY_l:
            gtk_widget_grab_focus(address_bar);
            gtk_editable_select_region(GTK_EDITABLE(address_bar), 0, -1);
            return TRUE;
        case GDK_KEY_Tab:
            browser_tab_select_relative(state & GDK_SHIFT_MASK ? -1 : 1);
            return TRUE;
        case GDK_KEY_r:
        {
            BrowserTab* tab = browser_tab_current();
            if (tab)
            {
                if (state & GDK_SHIFT_MASK)
                    webkit_web_view_reload_bypass_cache(WEBKIT_WEB_VIEW(tab->web_view));
                else
                    webkit_web_view_reload(WEBKIT_WEB_VIEW(tab->web_view));
            }
            return TRUE;
        }
        case GDK_KEY_plus:
        case GDK_KEY_equal:
        case GDK_KEY_minus:
        {
            BrowserTab* tab = browser_tab_current();
            if (!tab)
                return TRUE;
            WebKitWebView* view = WEBKIT_WEB_VIEW(tab->web_view);
            const double delta = gdk_keyval_to_lower(keyval) == GDK_KEY_minus ? -0.1 : 0.1;
            webkit_web_view_set_zoom_level(view, std::clamp(webkit_web_view_get_zoom_level(view) + delta, 0.5, 3.0));
            return TRUE;
        }
        case GDK_KEY_0:
        {
            BrowserTab* tab = browser_tab_current();
            if (tab)
                webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(tab->web_view), 1.0);
            return TRUE;
        }
        default:
            return FALSE;
    }
}

void on_application_activate(
    GApplication* application,
    gpointer user_data
)
{
    (void)user_data;

    GtkApplication* app =
        GTK_APPLICATION(application);

    AdwApplicationWindow* window =
        ADW_APPLICATION_WINDOW(
            adw_application_window_new(app)
        );

    browser_window = GTK_WINDOW(window);

    gtk_window_set_default_size(
        GTK_WINDOW(window),
        1200,
        800
    );

    gtk_window_set_title(
        GTK_WINDOW(window),
        "Hikari"
    );

    GtkWidget* root =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            0
        );

    /*
     * Sidebar
     */

    GtkWidget* sidebar =
        create_sidebar();

    GtkWidget* tab_stack = gtk_stack_new();
    browser_stack = GTK_STACK(tab_stack);
    gtk_widget_set_hexpand(tab_stack, TRUE);
    gtk_widget_set_vexpand(tab_stack, TRUE);
    tab_manager_init(tab_stack, sidebar_tab_container());
    tab_manager_set_changed_callback(on_tab_changed);
    tab_manager_set_progress_callback(on_tab_progress);
    sidebar_set_settings_callback(on_settings_clicked);
    GtkWidget* settings_page = userscript_manager_create_settings_page(GTK_WINDOW(window));
    gtk_stack_add_named(GTK_STACK(tab_stack), settings_page, "settings");

    gtk_box_append(
        GTK_BOX(root),
        sidebar
    );

    /*
     * Main browser area
     */

    GtkWidget* browser =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            0
        );

    gtk_widget_set_hexpand(browser, TRUE);
    gtk_widget_set_vexpand(browser, TRUE);

    /*
     * Navigation bar
     */

    GtkWidget* navigation =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            6
        );

    gtk_widget_set_margin_top(
        navigation,
        8
    );

    gtk_widget_set_margin_bottom(
        navigation,
        8
    );

    gtk_widget_set_margin_start(
        navigation,
        8
    );

    gtk_widget_set_margin_end(
        navigation,
        8
    );
    gtk_widget_add_css_class(navigation, "navigation-bar");

    /*
     * Back
     */

    back_button =
        gtk_button_new_from_icon_name(
            "go-previous-symbolic"
        );

    gtk_widget_set_tooltip_text(
        back_button,
        "Back"
    );

    g_signal_connect(
        back_button,
        "clicked",
        G_CALLBACK(on_back_clicked),
        nullptr
    );

    gtk_box_append(
        GTK_BOX(navigation),
        back_button
    );

    /*
     * Forward
     */

    forward_button =
        gtk_button_new_from_icon_name(
            "go-next-symbolic"
        );

    gtk_widget_set_tooltip_text(
        forward_button,
        "Forward"
    );

    g_signal_connect(
        forward_button,
        "clicked",
        G_CALLBACK(on_forward_clicked),
        nullptr
    );

    gtk_box_append(
        GTK_BOX(navigation),
        forward_button
    );

    /*
     * Reload
     */

    reload_button =
        gtk_button_new_from_icon_name(
            "view-refresh-symbolic"
        );

    gtk_widget_set_tooltip_text(
        reload_button,
        "Reload"
    );

    g_signal_connect(
        reload_button,
        "clicked",
        G_CALLBACK(on_reload_clicked),
        nullptr
    );

    gtk_box_append(
        GTK_BOX(navigation),
        reload_button
    );

    /*
     * Address bar
     */

    address_bar =
    gtk_entry_new();

gtk_widget_set_hexpand(
    address_bar,
    TRUE
);

gtk_entry_set_placeholder_text(
    GTK_ENTRY(address_bar),
    "Search or enter address"
);
gtk_widget_add_css_class(address_bar, "address-entry");

g_signal_connect(
    address_bar,
    "activate",
    G_CALLBACK(on_address_activate),
    nullptr
);

gtk_box_append(
    GTK_BOX(navigation),
    address_bar
);

    GtkWidget* window_controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_add_css_class(window_controls, "window-controls");

    GtkWidget* minimize_button = gtk_button_new_from_icon_name("window-minimize-symbolic");
    gtk_widget_set_tooltip_text(minimize_button, "Minimize window");
    g_signal_connect(minimize_button, "clicked", G_CALLBACK(on_minimize_clicked), nullptr);
    gtk_box_append(GTK_BOX(window_controls), minimize_button);

    GtkWidget* fullscreen_button = gtk_button_new_from_icon_name("view-fullscreen-symbolic");
    gtk_widget_set_tooltip_text(fullscreen_button, "Enter fullscreen");
    g_signal_connect(fullscreen_button, "clicked", G_CALLBACK(on_fullscreen_clicked), nullptr);
    gtk_box_append(GTK_BOX(window_controls), fullscreen_button);

    GtkWidget* close_window_button = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_set_tooltip_text(close_window_button, "Close window");
    gtk_widget_add_css_class(close_window_button, "close-window-button");
    g_signal_connect(close_window_button, "clicked", G_CALLBACK(on_close_window_clicked), nullptr);
    gtk_box_append(GTK_BOX(window_controls), close_window_button);
    gtk_box_append(GTK_BOX(navigation), window_controls);

    /*
     * Browser
     */

    gtk_box_append(
        GTK_BOX(browser),
        navigation
    );

    load_progress = gtk_progress_bar_new();
    gtk_widget_add_css_class(load_progress, "load-progress");
    gtk_widget_set_visible(load_progress, FALSE);
    gtk_box_append(GTK_BOX(browser), load_progress);

    gtk_box_append(
        GTK_BOX(browser),
        tab_stack
    );

    gtk_box_append(
        GTK_BOX(root),
        browser
    );

    adw_application_window_set_content(
        window,
        root
    );

    GtkCssProvider* styles = gtk_css_provider_new();
    gtk_css_provider_load_from_string(
        styles,
        ".sidebar { min-width: 188px; max-width: 188px; background: alpha(@theme_fg_color, 0.04); border-right: 1px solid @borders; transition: min-width 180ms ease, max-width 180ms ease; }"
        ".sidebar.compact { min-width: 48px; max-width: 48px; }"
        ".sidebar.compact .sidebar-toggle { min-width: 32px; max-width: 32px; min-height: 32px; max-height: 32px; padding: 0; }"
        ".brand-name { font-weight: 700; }"
        ".section-label { opacity: 0.62; font-size: 10px; font-weight: 700; }"
        ".navigation-bar { background: @theme_bg_color; border-bottom: 1px solid @borders; padding: 2px 4px; }"
        ".navigation-bar button { border-radius: 9px; }"
        ".window-controls { opacity: 0; transition: opacity 150ms ease; }"
        ".window-controls:hover { opacity: 1; }"
        ".window-controls button { min-width: 32px; min-height: 32px; padding: 0; }"
        ".window-controls .close-window-button:hover { color: @error_color; }"
        ".address-entry { border-radius: 10px; min-height: 36px; }"
        ".address-entry:focus { border-color: @accent_color; }"
        ".load-progress { min-height: 2px; }"
        ".load-progress trough, .load-progress progress { min-height: 2px; }"
        ".tab-button { min-height: 38px; padding: 2px 8px; border-radius: 9px; }"
        ".tab-button.selected { background: alpha(@accent_color, 0.15); }"
        ".tab-button.playing-audio { color: @accent_color; }"
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(styles),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(styles);

    GtkEventController* keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
    g_signal_connect(keys, "key-pressed", G_CALLBACK(on_key_pressed), nullptr);
    gtk_widget_add_controller(GTK_WIDGET(window), keys);

    browser_tab_create_home();

    gtk_window_present(
        GTK_WINDOW(window)
    );
}