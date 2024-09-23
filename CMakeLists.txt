cmake_minimum_required(VERSION 3.28...3.30)

add_library(lainstream MODULE)
add_library(OBS::lainstream ALIAS lainstream)

target_sources(lainstream
  PRIVATE lainstream.c)

target_sources(lainstream
  PRIVATE data/flag-shader.effect)

target_link_libraries(lainstream PRIVATE OBS::libobs)

set_target_properties_obs(lainstream PROPERTIES FOLDER plugins PREFIX "")
