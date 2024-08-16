project(lainstream)

add_library(lainstream MODULE)

target_sources(lainstream
  PRIVATE lainstream.c)
target_sources(lainstream
  PRIVATE data/flag-shader.effect)
target_link_libraries(lainstream PRIVATE OBS::libobs)
target_compile_options(lainstream PRIVATE -Wno-error)

setup_plugin_target(lainstream)
