project('ade', 'cpp',
  default_options: [
    'prefix=' + meson.source_root() / 'packaged',
    'buildtype=debugoptimized',
    'warning_level=3',
    'werror=true',
    'cpp_std=c++17',
  ]
)

add_project_arguments(
  '-Wno-deprecated-copy',
  '-Wno-unused-parameter',
  '-Wno-missing-field-initializers',
  language: 'cpp',
)

cmake = import('cmake')
opt_var = cmake.subproject_options()
opt_var.append_compile_args('cpp', '-Wno-c99-extensions')
opt_var.set_install(false)
sdbus_cpp = cmake.subproject('sdbus-cpp', options: opt_var)

deps = [
  dependency('tomlplusplus'),
  dependency('pangomm-2.48'),
  dependency('cairomm-1.16'),
  dependency('fmt'),
  dependency('cairo'),
  dependency('cairo-xcb'),
  dependency('xcb'),
  dependency('xcb-icccm'),
  dependency('xcb-ewmh'),
  dependency('xcb-atom'),
  dependency('threads'),
  declare_dependency(
    include_directories: [
      'subprojects/toml11',
    ]
  ),
  sdbus_cpp.dependency('sdbus-c++'),
]

executable('ade', 'src/ade.cc', dependencies: deps, install: true)

executable('t', 'test.cc', dependencies: deps, install: true)
