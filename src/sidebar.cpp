#include "sidebar.hpp"

static GtkWidget* tab_list = nullptr;

GtkWidget* create_sidebar()
{
    GtkWidget* sidebar =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            6
        );

    gtk_widget_set_size_request(
        sidebar,
        72,
        -1
    );

    /*
     * New tab button
     */

    GtkWidget* new_tab =
        gtk_button_new_from_icon_name(
            "list-add-symbolic"
        );

    gtk_widget_set_tooltip_text(
        new_tab,
        "New Tab"
    );

    gtk_box_append(
        GTK_BOX(sidebar),
        new_tab
    );

    /*
     * Tab list
     */

    tab_list =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            4
        );

    gtk_widget_set_vexpand(
        tab_list,
        TRUE
    );

    gtk_box_append(
        GTK_BOX(sidebar),
        tab_list
    );

    /*
     * Settings
     */

    GtkWidget* spacer =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            0
        );

    gtk_widget_set_vexpand(
        spacer,
        TRUE
    );

    gtk_box_append(
        GTK_BOX(sidebar),
        spacer
    );

    GtkWidget* settings =
        gtk_button_new_from_icon_name(
            "emblem-system-symbolic"
        );

    gtk_widget_set_tooltip_text(
        settings,
        "Settings"
    );

    gtk_box_append(
        GTK_BOX(sidebar),
        settings
    );

    return sidebar;
}

void sidebar_add_tab(
    BrowserTab* tab
)
{
    (void)tab;
}

void sidebar_select_tab(
    BrowserTab* tab
)
{
    (void)tab;
}