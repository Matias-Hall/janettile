project('janettile', 'c')

cc = meson.get_compiler('c')

# wayland = cc.find_library('wayland-client')
wayland = dependency('wayland-client')

wl_scanner = find_program('wayland-scanner')

river_layout_c = custom_target(
  'gen-river-c',
  output : 'river-layout-v3.c',
  input : 'protocol/river-layout-v3.xml',
  feed : true,
  capture : true,
  command : [wl_scanner, 'private-code']
)

river_layout_h = custom_target(
  'gen-river-h',
  output : 'river-layout-v3.h',
  input : 'protocol/river-layout-v3.xml',
  feed : true,
  capture : true,
  command : [wl_scanner, 'client-header']
)

river_layout_v3 = static_library('river-layout-v3', [river_layout_c, river_layout_h])

river_layout_v3_dep = declare_dependency(
  sources : [river_layout_h],
  link_with : river_layout_v3)

janet_dep = dependency('janet')

janet = find_program('janet')
xxd = find_program('xxd')

process_janet_image = custom_target(
  'process_janet_image',
  output : 'process.jimage',
  # layout_utils.janet is not actually needed by the command.
  # However, process.janet reads this file, and hence any changes to layout_utils.janet
  # must cause this target to rerun.
  input : ['src/process.janet', 'src/layout_utils.janet'],
  command : [janet, '-c', '@INPUT0@', '@OUTPUT@']
)

process_janet_h = custom_target(
  'process_janet_h',
  output : 'process_janet.h',
  input : process_janet_image,
  command : [xxd, '-i', '@INPUT@', '@OUTPUT@']
)

c_source = files(
  'src/main.c',
  'src/wayland.c',
  'src/janet_processor.c'
)

inc_dir = include_directories('include')

executable(
  'janettile',
  [
    c_source,
    river_layout_h,
    process_janet_h,
  ],
  include_directories : inc_dir,
  dependencies : [wayland, janet_dep, river_layout_v3_dep]
)
