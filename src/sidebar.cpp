#include "sidebar.hpp"
#include "tab_manager.hpp"

#include <algorithm>
#include <list>
#include <string>

static GtkWidget* tab_list = nullptr;
static GtkWidget* sidebar_widget = nullptr;
static GtkWidget* new_tab_button = nullptr;
static GtkWidget* brand_label = nullptr;
static GtkWidget* section_label = nullptr;
static GtkWidget* section_header = nullptr;
static GtkWidget* section_revealer = nullptr;
static GtkWidget* section_toggle = nullptr;
static GtkWidget* sidebar_toggle = nullptr;
static GtkWidget* bookmark_list = nullptr;
static GtkWidget* bookmark_empty_label = nullptr;
static GtkWidget* bookmark_button = nullptr;
static GtkWidget* bookmark_label = nullptr;
static GtkWidget* bookmark_section_header = nullptr;
static GtkWidget* bookmark_revealer = nullptr;
static GtkWidget* bookmark_section_toggle = nullptr;
static bool sidebar_compact = false;

struct Bookmark
{
    std::string title;
    std::string uri;
    GtkWidget* row;
};

static std::list<Bookmark> bookmarks;

static gchar* bookmark_file_path()
{
    gchar* directory = g_build_filename(g_get_user_config_dir(), "yocrrz-browser", nullptr);
    g_mkdir_with_parents(directory, 0700);
    gchar* path = g_build_filename(directory, "bookmarks.ini", nullptr);
    g_free(directory);
    return path;
}

static void save_bookmarks()
{
    GKeyFile* file = g_key_file_new();
    size_t index = 0;
    for (const Bookmark& bookmark : bookmarks)
    {
        const std::string group = "bookmark-" + std::to_string(index);
        g_key_file_set_string(file, group.c_str(), "title", bookmark.title.c_str());
        g_key_file_set_string(file, group.c_str(), "uri", bookmark.uri.c_str());
        ++index;
    }

    gsize length = 0;
    gchar* data = g_key_file_to_data(file, &length, nullptr);
    gchar* path = bookmark_file_path();
    g_file_set_contents(path, data, static_cast<gssize>(length), nullptr);
    g_free(path);
    g_free(data);
    g_key_file_unref(file);
}

static void update_bookmark_button()
{
    BrowserTab* tab = browser_tab_current();
    const char* uri = tab && !tab->is_home
        ? webkit_web_view_get_uri(WEBKIT_WEB_VIEW(tab->web_view))
        : nullptr;
    const bool can_bookmark = uri && *uri && g_strcmp0(uri, "about:blank") != 0;
    const bool already_saved = can_bookmark && std::any_of(bookmarks.begin(), bookmarks.end(), [uri](const Bookmark& item) {
        return item.uri == uri;
    });
    gtk_widget_set_sensitive(bookmark_button, can_bookmark && !already_saved);
    gtk_widget_set_tooltip_text(
        bookmark_button,
        !can_bookmark ? "Open a page to bookmark it" : already_saved ? "Already bookmarked" : "Bookmark current page"
    );
}

static void on_bookmark_open_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    auto* bookmark = static_cast<Bookmark*>(user_data);
    BrowserTab* tab = browser_tab_current();
    if (!tab)
        tab = browser_tab_create_home();

    tab->is_home = false;
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(tab->web_view), bookmark->uri.c_str());
}

static void on_bookmark_remove_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    auto* bookmark = static_cast<Bookmark*>(user_data);
    const auto found = std::find_if(bookmarks.begin(), bookmarks.end(), [bookmark](const Bookmark& item) {
        return &item == bookmark;
    });
    if (found == bookmarks.end())
        return;

    gtk_box_remove(GTK_BOX(bookmark_list), found->row);
    bookmarks.erase(found);
    save_bookmarks();
    gtk_widget_set_visible(bookmark_empty_label, bookmarks.empty());
    update_bookmark_button();
}

static void add_bookmark_row(const std::string& title, const std::string& uri)
{
    bookmarks.push_back({title, uri, nullptr});
    Bookmark* bookmark = &bookmarks.back();
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* open = gtk_button_new();
    GtkWidget* label = gtk_label_new(title.c_str());
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_button_set_child(GTK_BUTTON(open), label);
    gtk_widget_set_hexpand(open, TRUE);
    gtk_widget_set_halign(open, GTK_ALIGN_FILL);
    gtk_widget_set_tooltip_text(open, uri.c_str());
    gtk_widget_add_css_class(open, "bookmark-button");
    g_signal_connect(open, "clicked", G_CALLBACK(on_bookmark_open_clicked), bookmark);

    GtkWidget* remove = gtk_button_new_from_icon_name("user-trash-symbolic");
    gtk_widget_add_css_class(remove, "flat");
    gtk_widget_set_tooltip_text(remove, "Remove bookmark");
    g_signal_connect(remove, "clicked", G_CALLBACK(on_bookmark_remove_clicked), bookmark);
    gtk_box_append(GTK_BOX(row), open);
    gtk_box_append(GTK_BOX(row), remove);
    bookmark->row = row;
    gtk_box_append(GTK_BOX(bookmark_list), row);
    gtk_widget_set_visible(bookmark_empty_label, FALSE);
}

static void load_bookmarks()
{
    GKeyFile* file = g_key_file_new();
    gchar* path = bookmark_file_path();
    GError* error = nullptr;
    if (g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, &error))
    {
        gsize count = 0;
        gchar** groups = g_key_file_get_groups(file, &count);
        for (gsize index = 0; index < count; ++index)
        {
            gchar* title = g_key_file_get_string(file, groups[index], "title", nullptr);
            gchar* uri = g_key_file_get_string(file, groups[index], "uri", nullptr);
            if (title && uri && *uri)
                add_bookmark_row(title, uri);
            g_free(title);
            g_free(uri);
        }
        g_strfreev(groups);
    }
    g_clear_error(&error);
    g_free(path);
    g_key_file_unref(file);
}

static void on_new_tab_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    (void)user_data;
    browser_tab_create_home();
}

static void on_sidebar_toggle_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    (void)user_data;

    sidebar_compact = !sidebar_compact;
    if (sidebar_compact)
        gtk_widget_add_css_class(sidebar_widget, "compact");
    else
        gtk_widget_remove_css_class(sidebar_widget, "compact");
    gtk_widget_set_visible(new_tab_button, !sidebar_compact);
    gtk_widget_set_visible(brand_label, !sidebar_compact);
    gtk_widget_set_visible(section_label, !sidebar_compact);
    gtk_widget_set_visible(bookmark_label, !sidebar_compact);
    gtk_widget_set_visible(bookmark_section_header, !sidebar_compact);
    gtk_widget_set_visible(bookmark_revealer, !sidebar_compact);
    gtk_widget_set_margin_start(section_header, sidebar_compact ? 4 : 12);
    gtk_widget_set_margin_end(section_header, sidebar_compact ? 4 : 8);
    gtk_widget_set_size_request(section_toggle, sidebar_compact ? 32 : -1, sidebar_compact ? 32 : -1);
    gtk_image_set_from_icon_name(
        GTK_IMAGE(gtk_button_get_child(GTK_BUTTON(sidebar_toggle))),
        sidebar_compact ? "sidebar-show-symbolic" : "sidebar-hide-symbolic"
    );
    gtk_widget_set_tooltip_text(sidebar_toggle, sidebar_compact ? "Expand sidebar" : "Collapse sidebar");
    tab_manager_set_sidebar_compact(sidebar_compact);
}

static void on_tab_section_toggled(GtkToggleButton* button, gpointer user_data)
{
    (void)user_data;
    const bool expanded = gtk_toggle_button_get_active(button);
    gtk_revealer_set_reveal_child(GTK_REVEALER(section_revealer), expanded);
    gtk_widget_set_vexpand(section_revealer, expanded);
    gtk_image_set_from_icon_name(
        GTK_IMAGE(gtk_button_get_child(GTK_BUTTON(section_toggle))),
        expanded ? "pan-down-symbolic" : "pan-end-symbolic"
    );
    gtk_widget_set_tooltip_text(section_toggle, expanded ? "Collapse tabs" : "Expand tabs");
}

static void on_bookmark_section_toggled(GtkToggleButton* button, gpointer user_data)
{
    (void)user_data;
    const bool expanded = gtk_toggle_button_get_active(button);
    gtk_revealer_set_reveal_child(GTK_REVEALER(bookmark_revealer), expanded);
    gtk_image_set_from_icon_name(
        GTK_IMAGE(gtk_button_get_child(GTK_BUTTON(bookmark_section_toggle))),
        expanded ? "pan-down-symbolic" : "pan-end-symbolic"
    );
    gtk_widget_set_tooltip_text(bookmark_section_toggle, expanded ? "Collapse bookmarks" : "Expand bookmarks");
}

static void on_add_bookmark_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    (void)user_data;
    BrowserTab* tab = browser_tab_current();
    if (!tab || tab->is_home)
        return;

    WebKitWebView* view = WEBKIT_WEB_VIEW(tab->web_view);
    const char* uri = webkit_web_view_get_uri(view);
    if (!uri || !*uri || g_strcmp0(uri, "about:blank") == 0)
        return;

    for (const Bookmark& bookmark : bookmarks)
    {
        if (bookmark.uri == uri)
            return;
    }

    const char* title = webkit_web_view_get_title(view);
    add_bookmark_row(title && *title ? title : uri, uri);
    save_bookmarks();
    update_bookmark_button();
}

GtkWidget* create_sidebar()
{
    sidebar_widget =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            8
        );

    gtk_widget_set_hexpand(sidebar_widget, FALSE);
    gtk_widget_set_halign(sidebar_widget, GTK_ALIGN_START);
    gtk_widget_add_css_class(sidebar_widget, "sidebar");

    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_top(header, 8);
    gtk_widget_set_margin_start(header, 8);
    gtk_widget_set_margin_end(header, 8);
    gtk_box_append(GTK_BOX(sidebar_widget), header);

    sidebar_toggle = gtk_button_new_from_icon_name("sidebar-hide-symbolic");
    gtk_widget_add_css_class(sidebar_toggle, "sidebar-toggle");
    gtk_widget_set_size_request(sidebar_toggle, 32, 32);
    gtk_widget_set_tooltip_text(sidebar_toggle, "Collapse sidebar");
    g_signal_connect(sidebar_toggle, "clicked", G_CALLBACK(on_sidebar_toggle_clicked), nullptr);
    gtk_box_append(GTK_BOX(header), sidebar_toggle);

    brand_label = gtk_label_new("Yocrrz");
    gtk_widget_add_css_class(brand_label, "brand-name");
    gtk_widget_set_hexpand(brand_label, TRUE);
    gtk_widget_set_halign(brand_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(header), brand_label);

    new_tab_button = gtk_button_new_from_icon_name("list-add-symbolic");
    gtk_widget_set_tooltip_text(new_tab_button, "New tab (Ctrl+T)");
    g_signal_connect(new_tab_button, "clicked", G_CALLBACK(on_new_tab_clicked), nullptr);
    gtk_box_append(GTK_BOX(header), new_tab_button);

    section_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
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

    bookmark_section_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_top(bookmark_section_header, 6);
    gtk_widget_set_margin_start(bookmark_section_header, 12);
    gtk_widget_set_margin_end(bookmark_section_header, 8);
    gtk_box_append(GTK_BOX(sidebar_widget), bookmark_section_header);

    bookmark_label = gtk_label_new("BOOKMARKS");
    gtk_widget_add_css_class(bookmark_label, "section-label");
    gtk_widget_set_hexpand(bookmark_label, TRUE);
    gtk_widget_set_halign(bookmark_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(bookmark_section_header), bookmark_label);

    bookmark_button = gtk_button_new_from_icon_name("bookmark-new-symbolic");
    gtk_widget_add_css_class(bookmark_button, "flat");
    gtk_widget_set_tooltip_text(bookmark_button, "Bookmark current page");
    g_signal_connect(bookmark_button, "clicked", G_CALLBACK(on_add_bookmark_clicked), nullptr);
    gtk_box_append(GTK_BOX(bookmark_section_header), bookmark_button);

    bookmark_section_toggle = gtk_toggle_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bookmark_section_toggle), TRUE);
    gtk_button_set_child(GTK_BUTTON(bookmark_section_toggle), gtk_image_new_from_icon_name("pan-down-symbolic"));
    gtk_widget_add_css_class(bookmark_section_toggle, "flat");
    gtk_widget_set_tooltip_text(bookmark_section_toggle, "Collapse bookmarks");
    g_signal_connect(bookmark_section_toggle, "toggled", G_CALLBACK(on_bookmark_section_toggled), nullptr);
    gtk_box_append(GTK_BOX(bookmark_section_header), bookmark_section_toggle);

    bookmark_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(bookmark_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration(GTK_REVEALER(bookmark_revealer), 180);
    gtk_revealer_set_reveal_child(GTK_REVEALER(bookmark_revealer), TRUE);

    GtkWidget* bookmark_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bookmark_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(bookmark_scroll, -1, 150);
    gtk_widget_set_margin_start(bookmark_scroll, 8);
    gtk_widget_set_margin_end(bookmark_scroll, 8);
    bookmark_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    bookmark_empty_label = gtk_label_new("Saved pages will appear here.");
    gtk_label_set_wrap(GTK_LABEL(bookmark_empty_label), TRUE);
    gtk_widget_set_margin_top(bookmark_empty_label, 12);
    gtk_widget_set_margin_bottom(bookmark_empty_label, 12);
    gtk_widget_add_css_class(bookmark_empty_label, "dim-label");
    gtk_box_append(GTK_BOX(bookmark_list), bookmark_empty_label);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(bookmark_scroll), bookmark_list);
    gtk_revealer_set_child(GTK_REVEALER(bookmark_revealer), bookmark_scroll);
    gtk_box_append(GTK_BOX(sidebar_widget), bookmark_revealer);

    load_bookmarks();
    update_bookmark_button();

    return sidebar_widget;
}

GtkWidget* sidebar_tab_container()
{
    return tab_list;
}

void sidebar_update_current_tab(BrowserTab* tab)
{
    (void)tab;
    if (bookmark_button)
        update_bookmark_button();
}
