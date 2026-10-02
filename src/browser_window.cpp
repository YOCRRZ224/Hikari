#include "browser_window.hpp"
#include "sidebar.hpp"
#include "browser_view.hpp"
#include <string>
#include <webkit/webkit.h>

static GtkWidget* address_bar = nullptr;

static void on_back_clicked(
    GtkButton* button,
    gpointer user_data
)
{
    (void)button;
    (void)user_data;

    browser_go_back();
}

static void on_forward_clicked(
    GtkButton* button,
    gpointer user_data
)
{
    (void)button;
    (void)user_data;

    browser_go_forward();
}

static void on_reload_clicked(
    GtkButton* button,
    gpointer user_data
)
{
    (void)button;
    (void)user_data;

    browser_reload();
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

    browser_load_uri(uri.c_str());
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

gtk_editable_set_text(
    GTK_EDITABLE(address_bar),
    "https://example.com"
);

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

    GtkWidget* web_view =
        create_browser_view();

    gtk_box_append(
        GTK_BOX(browser),
        navigation
    );

    gtk_box_append(
        GTK_BOX(browser),
        web_view
    );

    gtk_box_append(
        GTK_BOX(root),
        browser
    );

    adw_application_window_set_content(
        window,
        root
    );

    gtk_window_present(
        GTK_WINDOW(window)
    );
}