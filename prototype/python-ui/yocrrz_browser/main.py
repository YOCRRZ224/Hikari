import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")

from gi.repository import Gtk, Adw, Gio


class BrowserWindow(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app)

        self.set_title("YOCRRZ Browser")
        self.set_default_size(1280, 800)

        # =========================
        # Root
        # =========================

        root = Gtk.Box(
            orientation=Gtk.Orientation.HORIZONTAL
        )

        self.set_content(root)

        # =========================
        # Sidebar
        # =========================

        sidebar = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL,
            spacing=6
        )

        sidebar.set_size_request(72, -1)
        sidebar.set_margin_top(8)
        sidebar.set_margin_bottom(8)
        sidebar.set_margin_start(8)
        sidebar.set_margin_end(8)

        root.append(sidebar)

        # New tab
        new_tab = Gtk.Button()
        new_tab.set_icon_name("list-add-symbolic")
        new_tab.set_tooltip_text("New Tab")
        new_tab.add_css_class("circular")

        new_tab.connect("clicked", self.new_tab)

        sidebar.append(new_tab)

        # Separator
        separator = Gtk.Separator(
            orientation=Gtk.Orientation.HORIZONTAL
        )

        sidebar.append(separator)

        # Tabs
        self.tab_list = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL,
            spacing=6
        )

        self.tab_list.set_vexpand(True)

        sidebar.append(self.tab_list)

        # Settings
        settings = Gtk.Button()
        settings.set_icon_name("emblem-system-symbolic")
        settings.set_tooltip_text("Settings")
        settings.add_css_class("circular")

        sidebar.append(settings)

        # =========================
        # Main area
        # =========================

        main = Gtk.Box(
            orientation=Gtk.Orientation.VERTICAL
        )

        main.set_hexpand(True)
        main.set_vexpand(True)

        root.append(main)

        # =========================
        # Navigation bar
        # =========================

        navigation = Gtk.Box(
            orientation=Gtk.Orientation.HORIZONTAL,
            spacing=6
        )

        navigation.set_margin_top(8)
        navigation.set_margin_bottom(8)
        navigation.set_margin_start(8)
        navigation.set_margin_end(8)

        main.append(navigation)

        # Back
        back = Gtk.Button()
        back.set_icon_name("go-previous-symbolic")
        back.set_tooltip_text("Back")

        navigation.append(back)

        # Forward
        forward = Gtk.Button()
        forward.set_icon_name("go-next-symbolic")
        forward.set_tooltip_text("Forward")

        navigation.append(forward)

        # Reload
        reload = Gtk.Button()
        reload.set_icon_name("view-refresh-symbolic")
        reload.set_tooltip_text("Reload")

        navigation.append(reload)

        # Address bar
        self.address = Gtk.Entry()

        self.address.set_hexpand(True)
        self.address.set_placeholder_text(
            "Search or enter address"
        )

        self.address.set_input_purpose(
            Gtk.InputPurpose.URL
        )

        self.address.connect(
            "activate",
            self.navigate
        )

        navigation.append(self.address)

        # =========================
        # Content
        # =========================

        self.content = Gtk.Stack()

        self.content.set_hexpand(True)
        self.content.set_vexpand(True)

        main.append(self.content)

        # =========================
        # Initial tab
        # =========================

        self.new_tab(None)

    # =============================
    # Tabs
    # =============================

    def new_tab(self, button):
        tab_number = len(
            self.tab_list.observe_children()
        ) + 1

        tab_button = Gtk.Button(
            label=str(tab_number)
        )

        tab_button.set_tooltip_text(
            f"Tab {tab_number}"
        )

        tab_button.add_css_class("circular")

        tab_button.connect(
            "clicked",
            self.select_tab,
            tab_number
        )

        self.tab_list.append(tab_button)

        page = Gtk.Label(
            label=f"New Tab {tab_number}"
        )

        page.set_hexpand(True)
        page.set_vexpand(True)

        page_name = f"tab-{tab_number}"

        self.content.add_named(
            page,
            page_name
        )

        self.content.set_visible_child_name(
            page_name
        )

    def select_tab(self, button, tab_number):
        self.content.set_visible_child_name(
            f"tab-{tab_number}"
        )

    # =============================
    # Navigation
    # =============================

    def navigate(self, entry):
        url = entry.get_text().strip()

        if not url:
            return

        if not (
            url.startswith("http://")
            or url.startswith("https://")
        ):
            url = "https://" + url

        print(f"Navigate: {url}")


class BrowserApplication(Adw.Application):
    def __init__(self):
        super().__init__(
            application_id="is.a.dev.yocrrz.Browser",
            flags=Gio.ApplicationFlags.DEFAULT_FLAGS
        )

    def do_activate(self):
        window = self.props.active_window

        if window is None:
            window = BrowserWindow(self)

        window.present()


def main():
    app = BrowserApplication()
    return app.run()


if __name__ == "__main__":
    main()