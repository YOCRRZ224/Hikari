#include "userscript_manager.hpp"

#include <libsoup/soup.h>

#include <algorithm>
#include <list>
#include <sstream>
#include <string>
#include <vector>

#include <glib/gstdio.h>

struct UserScript
{
    std::string id;
    std::string name;
    std::string source;
    std::string code;
    std::string run_at = "document-end";
    std::vector<std::string> matches;
    std::vector<std::string> excludes;
    bool enabled = true;
};

struct EditorForm
{
    UserScript* existing;
    GtkWidget* name_entry;
    GtkWidget* matches_view;
    GtkWidget* excludes_view;
    GtkWidget* run_at_entry;
    GtkWidget* code_view;
    GtkWidget* enabled_check;
};

struct DownloadData
{
    SoupMessage* message;
    std::string source;
};

static std::list<UserScript> scripts;
static std::vector<WebKitUserContentManager*> content_managers;
static GtkWindow* settings_parent = nullptr;
static GtkWidget* scripts_list = nullptr;
static GtkWidget* status_label = nullptr;
static SoupSession* download_session = nullptr;
static bool scripts_loaded = false;

static std::string trim(std::string value)
{
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

static gchar* scripts_directory()
{
    gchar* directory = g_build_filename(g_get_user_config_dir(), "yocrrz-browser", "userscripts", nullptr);
    g_mkdir_with_parents(directory, 0700);
    return directory;
}

static gchar* script_path(const std::string& id)
{
    gchar* directory = scripts_directory();
    gchar* filename = g_strconcat(id.c_str(), ".ini", nullptr);
    gchar* path = g_build_filename(directory, filename, nullptr);
    g_free(filename);
    g_free(directory);
    return path;
}

static std::vector<std::string> read_string_list(GKeyFile* file, const char* key)
{
    std::vector<std::string> values;
    gsize count = 0;
    gchar** items = g_key_file_get_string_list(file, "userscript", key, &count, nullptr);
    for (gsize index = 0; items && index < count; ++index)
        values.emplace_back(items[index]);
    g_strfreev(items);
    return values;
}

static void write_string_list(GKeyFile* file, const char* key, const std::vector<std::string>& values)
{
    std::vector<const gchar*> items;
    items.reserve(values.size());
    for (const std::string& value : values)
        items.push_back(value.c_str());
    g_key_file_set_string_list(file, "userscript", key, items.data(), items.size());
}

static bool save_script(const UserScript& script)
{
    GKeyFile* file = g_key_file_new();
    g_key_file_set_string(file, "userscript", "name", script.name.c_str());
    g_key_file_set_string(file, "userscript", "source", script.source.c_str());
    g_key_file_set_string(file, "userscript", "code", script.code.c_str());
    g_key_file_set_string(file, "userscript", "run-at", script.run_at.c_str());
    g_key_file_set_boolean(file, "userscript", "enabled", script.enabled);
    write_string_list(file, "matches", script.matches);
    write_string_list(file, "excludes", script.excludes);

    gsize length = 0;
    gchar* contents = g_key_file_to_data(file, &length, nullptr);
    gchar* path = script_path(script.id);
    const bool saved = g_file_set_contents(path, contents, static_cast<gssize>(length), nullptr);
    g_free(path);
    g_free(contents);
    g_key_file_unref(file);
    return saved;
}

static void load_scripts()
{
    if (scripts_loaded)
        return;
    scripts_loaded = true;

    gchar* directory = scripts_directory();
    GError* error = nullptr;
    GDir* dir = g_dir_open(directory, 0, &error);
    if (!dir)
    {
        g_clear_error(&error);
        g_free(directory);
        return;
    }

    const gchar* filename = nullptr;
    while ((filename = g_dir_read_name(dir)))
    {
        if (!g_str_has_suffix(filename, ".ini"))
            continue;

        gchar* path = g_build_filename(directory, filename, nullptr);
        GKeyFile* file = g_key_file_new();
        if (g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, nullptr))
        {
            UserScript script;
            script.id = filename;
            script.id.resize(script.id.size() - 4);
            gchar* name = g_key_file_get_string(file, "userscript", "name", nullptr);
            gchar* source = g_key_file_get_string(file, "userscript", "source", nullptr);
            gchar* code = g_key_file_get_string(file, "userscript", "code", nullptr);
            gchar* run_at = g_key_file_get_string(file, "userscript", "run-at", nullptr);
            script.name = name ? name : "Userscript";
            script.source = source ? source : "";
            script.code = code ? code : "";
            script.run_at = run_at ? run_at : "document-end";
            script.enabled = g_key_file_get_boolean(file, "userscript", "enabled", nullptr);
            script.matches = read_string_list(file, "matches");
            script.excludes = read_string_list(file, "excludes");
            if (script.matches.empty())
                script.matches.emplace_back("*://*/*");
            scripts.push_back(std::move(script));
            g_free(name);
            g_free(source);
            g_free(code);
            g_free(run_at);
        }
        g_key_file_unref(file);
        g_free(path);
    }

    g_dir_close(dir);
    g_free(directory);
}

static std::vector<std::string> split_lines(const std::string& text)
{
    std::vector<std::string> values;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line))
    {
        line = trim(line);
        if (!line.empty())
            values.push_back(std::move(line));
    }
    return values;
}

static std::string join_lines(const std::vector<std::string>& values)
{
    std::string result;
    for (const std::string& value : values)
    {
        if (!result.empty())
            result += '\n';
        result += value;
    }
    return result;
}

static void parse_metadata(UserScript& script)
{
    std::istringstream stream(script.code);
    std::string line;
    bool has_match = false;
    while (std::getline(stream, line))
    {
        line = trim(line);
        if (line.rfind("//", 0) != 0)
            continue;
        line = trim(line.substr(2));
        if (line.empty() || line[0] != '@')
            continue;

        const size_t split = line.find_first_of(" \t");
        const std::string key = line.substr(1, split == std::string::npos ? split : split - 1);
        const std::string value = split == std::string::npos ? "" : trim(line.substr(split + 1));
        if (key == "name" && !value.empty())
            script.name = value;
        else if ((key == "match" || key == "include") && !value.empty())
        {
            if (!has_match)
                script.matches.clear();
            script.matches.push_back(value);
            has_match = true;
        }
        else if (key == "exclude" && !value.empty())
            script.excludes.push_back(value);
        else if (key == "run-at" && !value.empty())
            script.run_at = value;
    }
}

static void sync_content_manager(WebKitUserContentManager* manager)
{
    webkit_user_content_manager_remove_all_scripts(manager);
    for (const UserScript& script : scripts)
    {
        if (!script.enabled || script.code.empty())
            continue;

        std::vector<const gchar*> allow_list;
        std::vector<const gchar*> block_list;
        for (const std::string& pattern : script.matches)
            allow_list.push_back(pattern.c_str());
        for (const std::string& pattern : script.excludes)
            block_list.push_back(pattern.c_str());
        if (allow_list.empty())
            allow_list.push_back("*://*/*");
        allow_list.push_back(nullptr);
        block_list.push_back(nullptr);

        const WebKitUserScriptInjectionTime injection_time =
            script.run_at == "document-start"
                ? WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START
                : WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END;
        WebKitUserScript* user_script = webkit_user_script_new(
            script.code.c_str(),
            WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
            injection_time,
            allow_list.data(),
            block_list.data()
        );
        webkit_user_content_manager_add_script(manager, user_script);
        webkit_user_script_unref(user_script);
    }
}

WebKitUserContentManager* userscript_manager_create_content_manager()
{
    load_scripts();
    WebKitUserContentManager* manager = webkit_user_content_manager_new();
    sync_content_manager(manager);
    content_managers.push_back(manager);
    return manager;
}

void userscript_manager_release_content_manager(WebKitUserContentManager* manager)
{
    const auto found = std::find(content_managers.begin(), content_managers.end(), manager);
    if (found == content_managers.end())
        return;
    content_managers.erase(found);
    g_object_unref(manager);
}

static void sync_all_content_managers()
{
    for (WebKitUserContentManager* manager : content_managers)
        sync_content_manager(manager);
}

static std::string text_view_contents(GtkWidget* view)
{
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gchar* text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    std::string result = text ? text : "";
    g_free(text);
    return result;
}

static GtkWidget* create_multiline_input(const char* value, int height)
{
    GtkWidget* view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)), value, -1);
    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(scroll, -1, height);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), view);
    return scroll;
}

static GtkWidget* text_input_child(GtkWidget* scroll)
{
    return gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(scroll));
}

static void refresh_script_list();

static void on_editor_response(GtkDialog* dialog, int response, gpointer user_data)
{
    auto* form = static_cast<EditorForm*>(user_data);
    if (response == GTK_RESPONSE_ACCEPT)
    {
        UserScript* script = form->existing;
        if (!script)
        {
            scripts.emplace_back();
            script = &scripts.back();
            gchar* id = g_uuid_string_random();
            script->id = id;
            g_free(id);
        }

        const char* name = gtk_editable_get_text(GTK_EDITABLE(form->name_entry));
        script->name = name && *name ? name : "Userscript";
        script->matches = split_lines(text_view_contents(text_input_child(form->matches_view)));
        script->excludes = split_lines(text_view_contents(text_input_child(form->excludes_view)));
        const char* run_at = gtk_editable_get_text(GTK_EDITABLE(form->run_at_entry));
        script->run_at = run_at && g_strcmp0(run_at, "document-start") == 0
            ? "document-start"
            : "document-end";
        script->code = text_view_contents(form->code_view);
        script->enabled = gtk_check_button_get_active(GTK_CHECK_BUTTON(form->enabled_check));
        if (script->source.empty())
            script->source = "Created in Yocrrz";
        if (script->matches.empty())
            script->matches.emplace_back("*://*/*");

        if (!save_script(*script) && status_label)
            gtk_label_set_text(GTK_LABEL(status_label), "Could not save the userscript.");
        sync_all_content_managers();
        refresh_script_list();
    }

    gtk_window_destroy(GTK_WINDOW(dialog));
    delete form;
}

static void show_script_editor(UserScript* existing, const std::string& code, const std::string& source)
{
    UserScript draft;
    draft.name = "New userscript";
    draft.source = source;
    draft.code = code;
    draft.matches.emplace_back("*://*/*");
    parse_metadata(draft);
    if (draft.name.empty())
        draft.name = "New userscript";

    GtkWidget* dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), existing ? "Edit userscript" : "Review userscript");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), settings_parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 760, 680);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Save", GTK_RESPONSE_ACCEPT);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content), 8);
    gtk_widget_set_margin_top(content, 16);
    gtk_widget_set_margin_bottom(content, 16);
    gtk_widget_set_margin_start(content, 16);
    gtk_widget_set_margin_end(content, 16);

    auto* form = new EditorForm{};
    form->existing = existing;
    form->name_entry = gtk_entry_new();
    form->matches_view = create_multiline_input("", 72);
    form->excludes_view = create_multiline_input("", 56);
    form->run_at_entry = gtk_entry_new();
    form->code_view = gtk_text_view_new();
    form->enabled_check = gtk_check_button_new_with_label("Enabled");

    const UserScript& initial = existing ? *existing : draft;
    gtk_editable_set_text(GTK_EDITABLE(form->name_entry), initial.name.c_str());
    gtk_text_buffer_set_text(
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_input_child(form->matches_view))),
        join_lines(initial.matches).c_str(),
        -1
    );
    gtk_text_buffer_set_text(
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_input_child(form->excludes_view))),
        join_lines(initial.excludes).c_str(),
        -1
    );
    gtk_editable_set_text(GTK_EDITABLE(form->run_at_entry), initial.run_at.c_str());
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(form->code_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(form->code_view), GTK_WRAP_NONE);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(form->code_view)), initial.code.c_str(), -1);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(form->enabled_check), initial.enabled);

    GtkWidget* name_label = gtk_label_new("Name");
    gtk_widget_set_halign(name_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content), name_label);
    gtk_box_append(GTK_BOX(content), form->name_entry);
    GtkWidget* match_label = gtk_label_new("@match patterns, one per line");
    gtk_widget_set_halign(match_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content), match_label);
    gtk_box_append(GTK_BOX(content), form->matches_view);
    GtkWidget* exclude_label = gtk_label_new("@exclude patterns, one per line");
    gtk_widget_set_halign(exclude_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content), exclude_label);
    gtk_box_append(GTK_BOX(content), form->excludes_view);
    GtkWidget* run_at_label = gtk_label_new("Run at (document-start or document-end)");
    gtk_widget_set_halign(run_at_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content), run_at_label);
    gtk_box_append(GTK_BOX(content), form->run_at_entry);
    GtkWidget* code_label = gtk_label_new("JavaScript");
    gtk_widget_set_halign(code_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content), code_label);
    GtkWidget* code_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(code_scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(code_scroll), form->code_view);
    gtk_box_append(GTK_BOX(content), code_scroll);
    gtk_box_append(GTK_BOX(content), form->enabled_check);

    g_signal_connect(dialog, "response", G_CALLBACK(on_editor_response), form);
    gtk_window_present(GTK_WINDOW(dialog));
}

static void on_new_script_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    (void)user_data;
    show_script_editor(nullptr, "", "Created in Yocrrz");
}

static void on_import_response(GtkNativeDialog* dialog, int response, gpointer user_data)
{
    (void)user_data;
    if (response == GTK_RESPONSE_ACCEPT)
    {
        GFile* file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
        gchar* contents = nullptr;
        gsize length = 0;
        GError* error = nullptr;
        if (file && g_file_load_contents(file, nullptr, &contents, &length, nullptr, &error))
        {
            gchar* valid_contents = g_utf8_make_valid(contents, static_cast<gssize>(length));
            gchar* basename = g_file_get_basename(file);
            show_script_editor(nullptr, valid_contents, basename ? basename : "Imported file");
            g_free(basename);
            g_free(valid_contents);
        }
        else if (status_label)
        {
            gtk_label_set_text(GTK_LABEL(status_label), error ? error->message : "Could not read that file.");
        }
        g_clear_error(&error);
        g_free(contents);
        g_clear_object(&file);
    }
    gtk_native_dialog_destroy(dialog);
    g_object_unref(dialog);
}

static void on_import_file_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    (void)user_data;
    GtkFileChooserNative* chooser = gtk_file_chooser_native_new(
        "Import userscript",
        settings_parent,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "Open",
        "Cancel"
    );
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "JavaScript userscripts");
    gtk_file_filter_add_pattern(filter, "*.js");
    gtk_file_filter_add_pattern(filter, "*.user.js");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
    g_signal_connect(chooser, "response", G_CALLBACK(on_import_response), nullptr);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(chooser));
}

static void on_download_finished(GObject* object, GAsyncResult* result, gpointer user_data)
{
    auto* download = static_cast<DownloadData*>(user_data);
    GError* error = nullptr;
    GBytes* bytes = soup_session_send_and_read_finish(SOUP_SESSION(object), result, &error);
    if (bytes && soup_message_get_status(download->message) >= 200 && soup_message_get_status(download->message) < 300)
    {
        gsize length = 0;
        const char* data = static_cast<const char*>(g_bytes_get_data(bytes, &length));
        gchar* code = g_utf8_make_valid(data, static_cast<gssize>(length));
        show_script_editor(nullptr, code, download->source);
        g_free(code);
        if (status_label)
            gtk_label_set_text(GTK_LABEL(status_label), "Review the downloaded code, then save to install it.");
    }
    else if (status_label)
    {
        const std::string message = error ? error->message : "The server did not return a successful response.";
        gtk_label_set_text(GTK_LABEL(status_label), message.c_str());
    }

    g_clear_error(&error);
    g_clear_pointer(&bytes, g_bytes_unref);
    g_object_unref(download->message);
    delete download;
}

static void on_install_url_response(GtkDialog* dialog, int response, gpointer user_data)
{
    (void)user_data;
    if (response == GTK_RESPONSE_ACCEPT)
    {
        GtkWidget* entry = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "userscript-url-entry"));
        const char* entered_url = gtk_editable_get_text(GTK_EDITABLE(entry));
        std::string url = entered_url ? trim(entered_url) : "";
        if (url.find("://") == std::string::npos)
            url = "https://" + url;

        if (!(g_str_has_prefix(url.c_str(), "https://") || g_str_has_prefix(url.c_str(), "http://")))
        {
            if (status_label)
                gtk_label_set_text(GTK_LABEL(status_label), "Enter an HTTP or HTTPS userscript URL.");
        }
        else
        {
            SoupMessage* message = soup_message_new("GET", url.c_str());
            if (!message)
            {
                if (status_label)
                    gtk_label_set_text(GTK_LABEL(status_label), "That URL is not valid.");
            }
            else
            {
                auto* download = new DownloadData{SOUP_MESSAGE(g_object_ref(message)), url};
                if (!download_session)
                    download_session = soup_session_new();
                soup_session_send_and_read_async(
                    download_session,
                    message,
                    G_PRIORITY_DEFAULT,
                    nullptr,
                    on_download_finished,
                    download
                );
                g_object_unref(message);
                if (status_label)
                    gtk_label_set_text(GTK_LABEL(status_label), "Downloading userscript...");
            }
        }
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_install_url_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    (void)user_data;
    GtkWidget* dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Install from URL");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), settings_parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Download", GTK_RESPONSE_ACCEPT);
    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_widget_set_margin_top(content, 16);
    gtk_widget_set_margin_bottom(content, 16);
    gtk_widget_set_margin_start(content, 16);
    gtk_widget_set_margin_end(content, 16);
    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "https://example.com/script.user.js");
    gtk_box_append(GTK_BOX(content), entry);
    g_object_set_data(G_OBJECT(dialog), "userscript-url-entry", entry);
    g_signal_connect(dialog, "response", G_CALLBACK(on_install_url_response), nullptr);
    gtk_window_present(GTK_WINDOW(dialog));
}

static void on_script_enabled_changed(GObject* object, GParamSpec* pspec, gpointer user_data)
{
    (void)pspec;
    auto* script = static_cast<UserScript*>(user_data);
    script->enabled = gtk_switch_get_active(GTK_SWITCH(object));
    save_script(*script);
    sync_all_content_managers();
}

static void on_edit_script_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    auto* script = static_cast<UserScript*>(user_data);
    show_script_editor(script, script->code, script->source);
}

static void on_delete_response(GtkDialog* dialog, int response, gpointer user_data)
{
    auto* script = static_cast<UserScript*>(user_data);
    if (response == GTK_RESPONSE_ACCEPT)
    {
        gchar* path = script_path(script->id);
        g_remove(path);
        g_free(path);
        scripts.remove_if([script](const UserScript& item) { return &item == script; });
        sync_all_content_managers();
        refresh_script_list();
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_delete_script_clicked(GtkButton* button, gpointer user_data)
{
    (void)button;
    auto* script = static_cast<UserScript*>(user_data);
    GtkWidget* dialog = gtk_message_dialog_new(
        settings_parent,
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_NONE,
        "Delete %s?",
        script->name.c_str()
    );
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "This removes the userscript from this browser profile.");
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Delete", GTK_RESPONSE_ACCEPT);
    g_signal_connect(dialog, "response", G_CALLBACK(on_delete_response), script);
    gtk_window_present(GTK_WINDOW(dialog));
}

static void refresh_script_list()
{
    if (!scripts_list)
        return;
    GtkWidget* child = nullptr;
    while ((child = gtk_widget_get_first_child(scripts_list)))
        gtk_box_remove(GTK_BOX(scripts_list), child);

    if (scripts.empty())
    {
        GtkWidget* empty = gtk_label_new("No userscripts installed.");
        gtk_widget_add_css_class(empty, "dim-label");
        gtk_widget_set_margin_top(empty, 24);
        gtk_widget_set_margin_bottom(empty, 24);
        gtk_box_append(GTK_BOX(scripts_list), empty);
        return;
    }

    for (UserScript& script : scripts)
    {
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_margin_top(row, 8);
        gtk_widget_set_margin_bottom(row, 8);
        GtkWidget* toggle = gtk_switch_new();
        gtk_switch_set_active(GTK_SWITCH(toggle), script.enabled);
        gtk_widget_set_valign(toggle, GTK_ALIGN_CENTER);
        g_signal_connect(toggle, "notify::active", G_CALLBACK(on_script_enabled_changed), &script);

        GtkWidget* details = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_hexpand(details, TRUE);
        GtkWidget* title = gtk_label_new(script.name.c_str());
        gtk_widget_set_halign(title, GTK_ALIGN_START);
        gtk_widget_add_css_class(title, "heading");
        std::string summary = script.source;
        if (!script.matches.empty())
            summary += "  ·  " + script.matches.front();
        GtkWidget* subtitle = gtk_label_new(summary.c_str());
        gtk_label_set_ellipsize(GTK_LABEL(subtitle), PANGO_ELLIPSIZE_MIDDLE);
        gtk_widget_set_halign(subtitle, GTK_ALIGN_START);
        gtk_widget_add_css_class(subtitle, "dim-label");
        gtk_box_append(GTK_BOX(details), title);
        gtk_box_append(GTK_BOX(details), subtitle);

        GtkWidget* edit = gtk_button_new_from_icon_name("document-edit-symbolic");
        gtk_widget_add_css_class(edit, "flat");
        gtk_widget_set_tooltip_text(edit, "Edit userscript");
        g_signal_connect(edit, "clicked", G_CALLBACK(on_edit_script_clicked), &script);
        GtkWidget* remove = gtk_button_new_from_icon_name("user-trash-symbolic");
        gtk_widget_add_css_class(remove, "flat");
        gtk_widget_set_tooltip_text(remove, "Delete userscript");
        g_signal_connect(remove, "clicked", G_CALLBACK(on_delete_script_clicked), &script);

        gtk_box_append(GTK_BOX(row), toggle);
        gtk_box_append(GTK_BOX(row), details);
        gtk_box_append(GTK_BOX(row), edit);
        gtk_box_append(GTK_BOX(row), remove);
        gtk_box_append(GTK_BOX(scripts_list), row);
        if (&script != &scripts.back())
            gtk_box_append(GTK_BOX(scripts_list), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    }
}

GtkWidget* userscript_manager_create_settings_page(GtkWindow* parent)
{
    load_scripts();
    settings_parent = parent;

    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(page, 24);
    gtk_widget_set_margin_bottom(page, 24);
    gtk_widget_set_margin_start(page, 28);
    gtk_widget_set_margin_end(page, 28);

    GtkWidget* heading = gtk_label_new("Userscripts");
    gtk_widget_add_css_class(heading, "title-1");
    gtk_widget_set_halign(heading, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(page), heading);
    GtkWidget* compatibility = gtk_label_new("Supports @match, @exclude, and @run-at. GM_* APIs and @require are not supported.");
    gtk_widget_add_css_class(compatibility, "dim-label");
    gtk_widget_set_halign(compatibility, GTK_ALIGN_START);
    gtk_label_set_wrap(GTK_LABEL(compatibility), TRUE);
    gtk_box_append(GTK_BOX(page), compatibility);

    GtkWidget* actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* add_code = gtk_button_new_with_label("New script");
    g_signal_connect(add_code, "clicked", G_CALLBACK(on_new_script_clicked), nullptr);
    GtkWidget* install_url = gtk_button_new_with_label("Install from URL");
    g_signal_connect(install_url, "clicked", G_CALLBACK(on_install_url_clicked), nullptr);
    GtkWidget* import_file = gtk_button_new_with_label("Import file");
    g_signal_connect(import_file, "clicked", G_CALLBACK(on_import_file_clicked), nullptr);
    gtk_box_append(GTK_BOX(actions), add_code);
    gtk_box_append(GTK_BOX(actions), install_url);
    gtk_box_append(GTK_BOX(actions), import_file);
    gtk_box_append(GTK_BOX(page), actions);

    status_label = gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(status_label), TRUE);
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(page), status_label);

    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    scripts_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), scripts_list);
    gtk_box_append(GTK_BOX(page), scroll);
    refresh_script_list();
    return page;
}