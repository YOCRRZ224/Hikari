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

static const char* home_page_html = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>New Tab | Yocrrz</title>
<style>
:root { color-scheme: light dark; font-family: system-ui, sans-serif; }
* { box-sizing: border-box; }
body { margin: 0; min-height: 100vh; color: #252b32; background: #f4f6f5; }
main { width: min(780px, calc(100% - 40px)); margin: 0 auto; padding: 12vh 0 64px; }
.brand { color: #286b61; font-size: 13px; font-weight: 700; }
h1 { margin: 12px 0 6px; font-size: 34px; font-weight: 650; }
.date { margin: 0 0 30px; color: #66716f; }
.search { display: flex; gap: 10px; padding: 8px; border: 1px solid #d6dedb; border-radius: 14px; background: #fff; box-shadow: 0 8px 24px #172e2910; }
.search input { flex: 1; min-width: 0; padding: 10px 12px; border: 0; outline: 0; background: transparent; font: inherit; }
.search button { padding: 0 18px; border: 0; border-radius: 10px; color: white; background: #286b61; font: inherit; cursor: pointer; }
.widgets { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 14px; margin-top: 28px; }
.widget { min-height: 130px; padding: 18px; border: 1px solid #dce3e0; border-radius: 12px; background: #fff; }
.widget h2 { margin: 0 0 14px; color: #66716f; font-size: 12px; font-weight: 700; }
.time { color: #286b61; font-size: 30px; font-variant-numeric: tabular-nums; }
.links { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 8px; }
.links a { padding: 10px; border-radius: 8px; color: inherit; background: #f2f5f3; text-decoration: none; }
.links a:hover { background: #e6efec; }
@media (prefers-color-scheme: dark) { body { color: #e6ebe9; background: #202624; } .date, .widget h2 { color: #aebbb7; } .search, .widget { border-color: #3d4945; background: #29312e; } .links a { background: #343e3a; } .links a:hover { background: #3c4b45; } }
@media (max-width: 560px) { main { padding-top: 9vh; } .widgets { grid-template-columns: 1fr; } h1 { font-size: 28px; } }
</style>
</head>
<body>
<main>
<div class="brand">YOCRRZ BROWSER</div>
<h1>Where to next?</h1>
<p class="date" id="date"></p>
<form class="search" action="https://www.google.com/search" method="get">
<input name="q" type="search" placeholder="Search the web or enter an address" autofocus aria-label="Search the web">
<button type="submit">Search</button>
</form>
<section class="widgets" aria-label="Widgets">
<article class="widget"><h2>LOCAL TIME</h2><div class="time" id="time"></div><div id="timezone"></div></article>
<article class="widget"><h2>QUICK LINKS</h2><nav class="links"><a href="https://www.wikipedia.org">Wikipedia</a><a href="https://github.com">GitHub</a><a href="https://www.youtube.com">YouTube</a><a href="https://news.ycombinator.com">Hacker News</a></nav></article>
</section>
</main>
<script>
const updateClock = () => { const now = new Date(); document.getElementById('time').textContent = now.toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'}); document.getElementById('date').textContent = now.toLocaleDateString([], {weekday: 'long', month: 'long', day: 'numeric'}); document.getElementById('timezone').textContent = Intl.DateTimeFormat().resolvedOptions().timeZone; };
updateClock(); setInterval(updateClock, 30000);
</script>
</body>
</html>
)HTML";

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
    (void)pspec;
    auto* tab = static_cast<BrowserTab*>(user_data);
    const char* uri = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(object));
    if (tab->is_home && uri && g_strcmp0(uri, "about:blank") != 0)
        tab->is_home = false;
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

static BrowserTab* create_tab(const char* uri, bool is_home)
{
    auto* tab =
        new BrowserTab{};
    tab->is_home = is_home;

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

    if (is_home)
        webkit_web_view_load_html(WEBKIT_WEB_VIEW(tab->web_view), home_page_html, "about:blank");
    else
        webkit_web_view_load_uri(WEBKIT_WEB_VIEW(tab->web_view), uri);

    browser_tab_select(tab);

    return tab;
}

BrowserTab* browser_tab_create(const char* uri)
{
    return create_tab(uri, false);
}

BrowserTab* browser_tab_create_home()
{
    return create_tab(nullptr, true);
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
            browser_tab_create_home();
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
