#include "tab_manager.hpp"

#include <vector>
#include <string>

static GtkWidget* tab_stack = nullptr;
static GtkWidget* tab_sidebar = nullptr;

static std::vector<BrowserTab*> tabs;
static BrowserTab* current_tab = nullptr;

void tab_manager_init(
    GtkWidget* stack,
    GtkWidget* sidebar
)
{
    tab_stack = stack;
    tab_sidebar = sidebar;
}

static void on_tab_clicked(
    GtkButton* button,
    gpointer user_data
)
{
    (void)button;

    BrowserTab* tab =
        static_cast<BrowserTab*>(user_data);

    browser_tab_select(tab);
}

BrowserTab* browser_tab_create(
    const char* uri
)
{
    auto* tab =
        new BrowserTab{};

    tab->web_view =
        webkit_web_view_new();

    gtk_widget_set_hexpand(
        tab->web_view,
        TRUE
    );

    gtk_widget_set_vexpand(
        tab->web_view,
        TRUE
    );

    tab->label =
        gtk_label_new("New Tab");

    gtk_label_set_ellipsize(
        GTK_LABEL(tab->label),
        PANGO_ELLIPSIZE_END
    );

    tab->button =
        gtk_button_new();

    gtk_button_set_child(
        GTK_BUTTON(tab->button),
        tab->label
    );

    g_signal_connect(
        tab->button,
        "clicked",
        G_CALLBACK(on_tab_clicked),
        tab
    );

    gtk_stack_add_named(
        GTK_STACK(tab_stack),
        tab->web_view,
        std::to_string(tabs.size()).c_str()
    );

    gtk_box_append(
        GTK_BOX(tab_sidebar),
        tab->button
    );

    tabs.push_back(tab);

    webkit_web_view_load_uri(
        WEBKIT_WEB_VIEW(tab->web_view),
        uri
    );

    browser_tab_select(tab);

    return tab;
}

void browser_tab_select(
    BrowserTab* tab
)
{
    current_tab = tab;

    gtk_stack_set_visible_child(
        GTK_STACK(tab_stack),
        tab->web_view
    );
}

BrowserTab* browser_tab_current()
{
    return current_tab;
}
