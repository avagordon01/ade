project('ade', 'cpp',
  default_options: [
    'prefix=' + meson.source_root() / 'packaged',
    'buildtype=debugoptimized',
    'warning_level=3',
    'werror=true',
    'cpp_std=c++17',
    'b_sanitize=address,undefined',
    'b_lundef=false',
  ]
)

add_project_arguments(
  '-Wno-deprecated-copy',
  '-Wno-unused-parameter',
  '-Wno-missing-field-initializers',
  language: 'cpp',
)

deps = [
  dependency('cairo'),
  dependency('cairo-xcb'),
  dependency('xcb'),
  dependency('xcb-icccm'),
  dependency('xcb-ewmh'),
  dependency('xcb-atom'),
  declare_dependency(
    include_directories: [
      'vendor/toml11',
    ]
  )
]

executable('bar', 'src/bar.cc', dependencies: deps, install: true)
