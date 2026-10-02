#include "tab_manager.hpp"

#include <algorithm>
#include <vector>
#include <string>

static GtkWidget* tab_stack = nullptr;
static GtkWidget* tab_sidebar = nullptr;

static std::vector<BrowserTab*> tabs;
static BrowserTab* current_tab = nullptr;
static BrowserTabChangedCallback changed_callback = nullptr;
static BrowserTabProgressCallback progress_callback = nullptr;
static unsigned int next_tab_id = 0;
static bool sidebar_compact = false;

static void on_tab_title_changed(GObject* object, GParamSpec* pspec, gpointer user_data)
{
    (void)pspec;
    auto* tab = static_cast<BrowserTab*>(user_data);
    const char* title = webkit_web_view_get_title(WEBKIT_WEB_VIEW(object));
    gtk_label_set_text(GTK_LABEL(tab->label), title && *title ? title : "New Tab");
    if (tab == current_tab && changed_callback)
        changed_callback(tab);
}

static void on_tab_uri_changed(GObject* object, GParamSpec* pspec, gpointer user_data)
{
    (void)object;
    (void)pspec;
    auto* tab = static_cast<BrowserTab*>(user_data);
    if (tab == current_tab && changed_callback)
        changed_callback(tab);
}

    static void on_tab_load_changed(GObject* object, GParamSpec* pspec, gpointer user_data)
    {
        (void)object;
        (void)pspec;
        auto* tab = static_cast<BrowserTab*>(user_data);
        if (tab == current_tab && progress_callback)
            progress_callback(tab);
    }

static void on_tab_close_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    browser_tab_close(static_cast<BrowserTab*>(user_data));
}

static void update_media_controls(BrowserTab* tab)
{
    WebKitWebView* view = WEBKIT_WEB_VIEW(tab->web_view);
    const bool playing = webkit_web_view_is_playing_audio(view);
    const bool muted = webkit_web_view_get_is_muted(view);

    gtk_button_set_label(GTK_BUTTON(tab->play_button), playing ? "Pause media" : "Play media");
    gtk_button_set_label(GTK_BUTTON(tab->mute_button), muted ? "Unmute tab audio" : "Mute tab audio");
    gtk_widget_set_tooltip_text(tab->media_button, playing ? "Media is playing" : "Media controls");
    gtk_image_set_from_icon_name(
        GTK_IMAGE(gtk_menu_button_get_child(GTK_MENU_BUTTON(tab->media_button))),
        muted ? "audio-volume-muted-symbolic" : "audio-volume-high-symbolic"
    );

    if (playing)
        gtk_widget_add_css_class(tab->button, "playing-audio");
    else
        gtk_widget_remove_css_class(tab->button, "playing-audio");
}

static void on_media_state_changed(GObject* object, GParamSpec* pspec, gpointer user_data)
{
    (void)object;
    (void)pspec;
    update_media_controls(static_cast<BrowserTab*>(user_data));
}

static void on_media_play_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    auto* tab = static_cast<BrowserTab*>(user_data);
    WebKitWebView* view = WEBKIT_WEB_VIEW(tab->web_view);
    const char* script = webkit_web_view_is_playing_audio(view)
        ? "document.querySelectorAll('audio,video').forEach(m => m.pause())"
        : "document.querySelectorAll('audio,video').forEach(m => m.play().catch(() => {}))";
    webkit_web_view_evaluate_javascript(view, script, -1, nullptr, nullptr, nullptr, nullptr, nullptr);
}

static void on_media_mute_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    auto* tab = static_cast<BrowserTab*>(user_data);
    WebKitWebView* view = WEBKIT_WEB_VIEW(tab->web_view);
    webkit_web_view_set_is_muted(view, !webkit_web_view_get_is_muted(view));
    update_media_controls(tab);
}

void tab_manager_init(
    GtkWidget* stack,
    GtkWidget* sidebar
)
{
    tab_stack = stack;
    tab_sidebar = sidebar;
}

void tab_manager_set_changed_callback(BrowserTabChangedCallback callback)
{
    changed_callback = callback;
}

void tab_manager_set_progress_callback(BrowserTabProgressCallback callback)
{
    progress_callback = callback;
}

void tab_manager_set_sidebar_compact(bool compact)
{
    sidebar_compact = compact;
    for (BrowserTab* tab : tabs)
    {
        gtk_widget_set_visible(tab->close_button, !compact);
        gtk_widget_set_visible(tab->media_button, !compact);
    }
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

    tab->label = gtk_label_new("New Tab");

    gtk_label_set_ellipsize(
        GTK_LABEL(tab->label),
        PANGO_ELLIPSIZE_END
    );

    tab->row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    tab->button = gtk_button_new();
    gtk_widget_set_hexpand(tab->button, TRUE);
    gtk_widget_add_css_class(tab->button, "tab-button");

    gtk_button_set_child(
        GTK_BUTTON(tab->button),
        tab->label
    );
    gtk_widget_set_halign(tab->label, GTK_ALIGN_START);

    g_signal_connect(
        tab->button,
        "clicked",
        G_CALLBACK(on_tab_clicked),
        tab
    );

    tab->media_button = gtk_menu_button_new();
    gtk_menu_button_set_child(
        GTK_MENU_BUTTON(tab->media_button),
        gtk_image_new_from_icon_name("audio-volume-high-symbolic")
    );
    gtk_widget_add_css_class(tab->media_button, "flat");
    gtk_widget_set_tooltip_text(tab->media_button, "Media controls");

    GtkWidget* media_popover = gtk_popover_new();
    GtkWidget* media_actions = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(media_actions, 8);
    gtk_widget_set_margin_bottom(media_actions, 8);
    gtk_widget_set_margin_start(media_actions, 8);
    gtk_widget_set_margin_end(media_actions, 8);
    tab->play_button = gtk_button_new_with_label("Play media");
    tab->mute_button = gtk_button_new_with_label("Mute tab audio");
    gtk_box_append(GTK_BOX(media_actions), tab->play_button);
    gtk_box_append(GTK_BOX(media_actions), tab->mute_button);
    gtk_popover_set_child(GTK_POPOVER(media_popover), media_actions);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(tab->media_button), media_popover);
    g_signal_connect(tab->play_button, "clicked", G_CALLBACK(on_media_play_clicked), tab);
    g_signal_connect(tab->mute_button, "clicked", G_CALLBACK(on_media_mute_clicked), tab);
    gtk_widget_set_visible(tab->media_button, !sidebar_compact);

    tab->close_button = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_set_visible(tab->close_button, !sidebar_compact);
    gtk_widget_set_tooltip_text(tab->close_button, "Close tab (Ctrl+W)");
    gtk_widget_add_css_class(tab->close_button, "flat");
    g_signal_connect(tab->close_button, "clicked", G_CALLBACK(on_tab_close_clicked), tab);
    gtk_box_append(GTK_BOX(tab->row), tab->button);
    gtk_box_append(GTK_BOX(tab->row), tab->media_button);
    gtk_box_append(GTK_BOX(tab->row), tab->close_button);

    const std::string page_name = "tab-" + std::to_string(next_tab_id++);
    gtk_stack_add_named(
        GTK_STACK(tab_stack),
        tab->web_view,
        page_name.c_str()
    );

    gtk_box_append(GTK_BOX(tab_sidebar), tab->row);

    g_signal_connect(tab->web_view, "notify::title", G_CALLBACK(on_tab_title_changed), tab);
    g_signal_connect(tab->web_view, "notify::uri", G_CALLBACK(on_tab_uri_changed), tab);
    g_signal_connect(tab->web_view, "notify::is-playing-audio", G_CALLBACK(on_media_state_changed), tab);
    g_signal_connect(tab->web_view, "notify::is-muted", G_CALLBACK(on_media_state_changed), tab);
    g_signal_connect(tab->web_view, "notify::estimated-load-progress", G_CALLBACK(on_tab_load_changed), tab);
    g_signal_connect(tab->web_view, "notify::is-loading", G_CALLBACK(on_tab_load_changed), tab);

    tabs.push_back(tab);
    update_media_controls(tab);

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
    if (!tab || tab == current_tab)
        return;

    if (current_tab)
        gtk_widget_remove_css_class(current_tab->button, "selected");

    current_tab = tab;
    gtk_widget_add_css_class(tab->button, "selected");

    gtk_stack_set_visible_child(
        GTK_STACK(tab_stack),
        tab->web_view
    );

    if (changed_callback)
        changed_callback(tab);
    if (progress_callback)
        progress_callback(tab);
}

void browser_tab_close(BrowserTab* tab)
{
    if (!tab)
        return;

    const auto found = std::find(tabs.begin(), tabs.end(), tab);
    if (found == tabs.end())
        return;

    const size_t index = static_cast<size_t>(found - tabs.begin());
    const bool was_current = tab == current_tab;
    g_signal_handlers_disconnect_by_data(tab->web_view, tab);
    gtk_box_remove(GTK_BOX(tab_sidebar), tab->row);
    gtk_stack_remove(GTK_STACK(tab_stack), tab->web_view);
    tabs.erase(found);

    if (was_current)
    {
        current_tab = nullptr;
        if (tabs.empty())
            browser_tab_create("https://example.com");
        else
            browser_tab_select(tabs[std::min(index, tabs.size() - 1)]);
    }

    delete tab;
}

void browser_tab_select_relative(int direction)
{
    if (tabs.empty() || direction == 0)
        return;

    const auto found = std::find(tabs.begin(), tabs.end(), current_tab);
    const int current = found == tabs.end() ? 0 : static_cast<int>(found - tabs.begin());
    const int count = static_cast<int>(tabs.size());
    const int next = (current + direction % count + count) % count;
    browser_tab_select(tabs[static_cast<size_t>(next)]);
}

BrowserTab* browser_tab_current()
{
    return current_tab;
}
