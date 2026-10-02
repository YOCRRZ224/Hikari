project(
  'Hikari',
  'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3'
  ]
)

gtk = dependency('gtk4')
adwaita = dependency('libadwaita-1')
webkit = dependency('webkitgtk-6.0')
soup = dependency('libsoup-3.0')

executable(
  'hikari',
  [
    'src/main.cpp',
    'src/browser_window.cpp',
    'src/sidebar.cpp',
    'src/tab_manager.cpp',
    'src/browser_view.cpp',
    'src/userscript_manager.cpp',
  ],
  dependencies: [
    gtk,
    adwaita,
    webkit,
    soup,
  ],
  install: true,
)

install_data(
  'data/hikari.desktop',
  install_dir: get_option('datadir') / 'applications',
)

install_data(
  'data/icons/hicolor/scalable/apps/hikari.svg',
  install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps',
)
