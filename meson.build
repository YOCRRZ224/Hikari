project(
  'yocrrz-browser',
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

executable(
  'yocrrz-browser',
  [
    'src/main.cpp',
    'src/browser_window.cpp',
    'src/sidebar.cpp',
    'src/browser_view.cpp',
  ],
  dependencies: [
    gtk,
    adwaita,
    webkit,
  ],
  install: true,
)
