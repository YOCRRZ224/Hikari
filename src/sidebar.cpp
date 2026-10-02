#include "sidebar.hpp"
#include "tab_manager.hpp"

static GtkWidget* tab_list = nullptr;
static GtkWidget* sidebar_widget = nullptr;
static GtkWidget* new_tab_button = nullptr;
static bool sidebar_compact = false;

static void on_new_tab_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    (void)user_data;
    browser_tab_create("https://example.com");
}

static void on_sidebar_toggle_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    (void)user_data;

    sidebar_compact = !sidebar_compact;
    gtk_widget_set_size_request(sidebar_widget, sidebar_compact ? 56 : 240, -1);
    gtk_widget_set_visible(new_tab_button, !sidebar_compact);
    tab_manager_set_sidebar_compact(sidebar_compact);
}

GtkWidget* create_sidebar()
{
    sidebar_widget =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            8
        );

    gtk_widget_set_size_request(
        sidebar_widget,
        240,
        -1
    );
    gtk_widget_add_css_class(sidebar_widget, "sidebar");

    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_top(header, 8);
    gtk_widget_set_margin_start(header, 8);
    gtk_widget_set_margin_end(header, 8);
    gtk_box_append(GTK_BOX(sidebar_widget), header);

    GtkWidget* collapse = gtk_button_new_from_icon_name("sidebar-show-symbolic");
    gtk_widget_set_tooltip_text(collapse, "Collapse sidebar");
    g_signal_connect(collapse, "clicked", G_CALLBACK(on_sidebar_toggle_clicked), nullptr);
    gtk_box_append(GTK_BOX(header), collapse);

    new_tab_button = gtk_button_new_from_icon_name("list-add-symbolic");
    gtk_widget_set_tooltip_text(new_tab_button, "New tab (Ctrl+T)");
    gtk_widget_set_hexpand(new_tab_button, TRUE);
    g_signal_connect(new_tab_button, "clicked", G_CALLBACK(on_new_tab_clicked), nullptr);
    gtk_box_append(GTK_BOX(header), new_tab_button);

    tab_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_vexpand(tab_list, TRUE);
    gtk_widget_set_margin_start(tab_list, 8);
    gtk_widget_set_margin_end(tab_list, 8);
    gtk_box_append(GTK_BOX(sidebar_widget), tab_list);

    return sidebar_widget;
}

GtkWidget* sidebar_tab_container()
{
    return tab_list;
}
