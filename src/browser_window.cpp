#include "browser_window.hpp"
#include "sidebar.hpp"
#include "tab_manager.hpp"
#include <string>
#include <webkit/webkit.h>

static GtkWidget* address_bar = nullptr;
static GtkWindow* browser_window = nullptr;

static void on_tab_changed(BrowserTab* tab)
{
    const char* uri = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(tab->web_view));
    gtk_editable_set_text(GTK_EDITABLE(address_bar), uri ? uri : "");

    const char* title = webkit_web_view_get_title(WEBKIT_WEB_VIEW(tab->web_view));
    gtk_window_set_title(browser_window, title && *title ? title : "Yocrrz Browser");
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
    if (tab)
        webkit_web_view_reload(WEBKIT_WEB_VIEW(tab->web_view));
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

    std::string uri = text;

    if (uri.find("://") == std::string::npos)
    {
        if (uri.find(' ') != std::string::npos)
        {
            uri =
                "https://www.google.com/search?q=" +
                uri;
        }
        else
        {
            uri = "https://" + uri;
        }
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

    if (!(state & GDK_CONTROL_MASK))
        return FALSE;

    switch (gdk_keyval_to_lower(keyval))
    {
        case GDK_KEY_t:
            browser_tab_create("https://example.com");
            return TRUE;
        case GDK_KEY_w:
            browser_tab_close(browser_tab_current());
            return TRUE;
        case GDK_KEY_l:
            gtk_widget_grab_focus(address_bar);
            gtk_editable_select_region(GTK_EDITABLE(address_bar), 0, -1);
            return TRUE;
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
        "Yocrrz Browser"
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
    gtk_widget_set_hexpand(tab_stack, TRUE);
    gtk_widget_set_vexpand(tab_stack, TRUE);
    tab_manager_init(tab_stack, sidebar_tab_container());
    tab_manager_set_changed_callback(on_tab_changed);

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

    GtkWidget* back =
        gtk_button_new_from_icon_name(
            "go-previous-symbolic"
        );

    gtk_widget_set_tooltip_text(
        back,
        "Back"
    );

    g_signal_connect(
        back,
        "clicked",
        G_CALLBACK(on_back_clicked),
        nullptr
    );

    gtk_box_append(
        GTK_BOX(navigation),
        back
    );

    /*
     * Forward
     */

    GtkWidget* forward =
        gtk_button_new_from_icon_name(
            "go-next-symbolic"
        );

    gtk_widget_set_tooltip_text(
        forward,
        "Forward"
    );

    g_signal_connect(
        forward,
        "clicked",
        G_CALLBACK(on_forward_clicked),
        nullptr
    );

    gtk_box_append(
        GTK_BOX(navigation),
        forward
    );

    /*
     * Reload
     */

    GtkWidget* reload =
        gtk_button_new_from_icon_name(
            "view-refresh-symbolic"
        );

    gtk_widget_set_tooltip_text(
        reload,
        "Reload"
    );

    g_signal_connect(
        reload,
        "clicked",
        G_CALLBACK(on_reload_clicked),
        nullptr
    );

    gtk_box_append(
        GTK_BOX(navigation),
        reload
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

    /*
     * Browser
     */

    gtk_box_append(
        GTK_BOX(browser),
        navigation
    );

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
    gtk_css_provider_load_from_data(
        styles,
        ".sidebar { background: alpha(@theme_fg_color, 0.04); border-right: 1px solid @borders; }"
        ".navigation-bar { background: @theme_bg_color; border-bottom: 1px solid @borders; }"
        ".address-entry { border-radius: 10px; min-height: 34px; }"
        ".tab-button { min-height: 36px; padding: 2px 8px; }"
        ".tab-button.selected { background: alpha(@accent_color, 0.15); }",
        -1
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(styles),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(styles);

    GtkEventController* keys = gtk_event_controller_key_new();
    g_signal_connect(keys, "key-pressed", G_CALLBACK(on_key_pressed), nullptr);
    gtk_widget_add_controller(GTK_WIDGET(window), keys);

    browser_tab_create("https://example.com");

    gtk_window_present(
        GTK_WINDOW(window)
    );
}