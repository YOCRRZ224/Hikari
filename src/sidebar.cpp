#include "sidebar.hpp"
#include "tab_manager.hpp"

static GtkWidget* tab_list = nullptr;
static GtkWidget* sidebar_widget = nullptr;
static GtkWidget* new_tab_button = nullptr;
static GtkWidget* brand_label = nullptr;
static GtkWidget* section_label = nullptr;
static GtkWidget* section_revealer = nullptr;
static GtkWidget* section_toggle = nullptr;
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
    gtk_widget_set_visible(brand_label, !sidebar_compact);
    gtk_widget_set_visible(section_label, !sidebar_compact);
    tab_manager_set_sidebar_compact(sidebar_compact);
}

static void on_tab_section_toggled(GtkToggleButton* button, gpointer user_data)
{
    (void)user_data;
    const bool expanded = gtk_toggle_button_get_active(button);
    gtk_revealer_set_reveal_child(GTK_REVEALER(section_revealer), expanded);
    gtk_image_set_from_icon_name(
        GTK_IMAGE(gtk_button_get_child(GTK_BUTTON(section_toggle))),
        expanded ? "pan-down-symbolic" : "pan-end-symbolic"
    );
    gtk_widget_set_tooltip_text(section_toggle, expanded ? "Collapse tabs" : "Expand tabs");
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
    gtk_widget_set_tooltip_text(collapse, "Collapse sidebar width");
    g_signal_connect(collapse, "clicked", G_CALLBACK(on_sidebar_toggle_clicked), nullptr);
    gtk_box_append(GTK_BOX(header), collapse);

    brand_label = gtk_label_new("Yocrrz");
    gtk_widget_add_css_class(brand_label, "brand-name");
    gtk_widget_set_hexpand(brand_label, TRUE);
    gtk_widget_set_halign(brand_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(header), brand_label);

    new_tab_button = gtk_button_new_from_icon_name("list-add-symbolic");
    gtk_widget_set_tooltip_text(new_tab_button, "New tab (Ctrl+T)");
    g_signal_connect(new_tab_button, "clicked", G_CALLBACK(on_new_tab_clicked), nullptr);
    gtk_box_append(GTK_BOX(header), new_tab_button);

    GtkWidget* section_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_top(section_header, 8);
    gtk_widget_set_margin_start(section_header, 12);
    gtk_widget_set_margin_end(section_header, 8);
    gtk_box_append(GTK_BOX(sidebar_widget), section_header);

    section_label = gtk_label_new("TABS");
    gtk_widget_add_css_class(section_label, "section-label");
    gtk_widget_set_hexpand(section_label, TRUE);
    gtk_widget_set_halign(section_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(section_header), section_label);

    section_toggle = gtk_toggle_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(section_toggle), TRUE);
    gtk_button_set_child(GTK_BUTTON(section_toggle), gtk_image_new_from_icon_name("pan-down-symbolic"));
    gtk_widget_add_css_class(section_toggle, "flat");
    gtk_widget_set_tooltip_text(section_toggle, "Collapse tabs");
    g_signal_connect(section_toggle, "toggled", G_CALLBACK(on_tab_section_toggled), nullptr);
    gtk_box_append(GTK_BOX(section_header), section_toggle);

    section_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(section_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(GTK_REVEALER(section_revealer), 180);
    gtk_revealer_set_reveal_child(GTK_REVEALER(section_revealer), TRUE);
    gtk_widget_set_vexpand(section_revealer, TRUE);

    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);

    tab_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(tab_list, 8);
    gtk_widget_set_margin_end(tab_list, 8);
    gtk_widget_set_margin_bottom(tab_list, 8);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), tab_list);
    gtk_revealer_set_child(GTK_REVEALER(section_revealer), scroll);
    gtk_box_append(GTK_BOX(sidebar_widget), section_revealer);

    return sidebar_widget;
}

GtkWidget* sidebar_tab_container()
{
    return tab_list;
}
