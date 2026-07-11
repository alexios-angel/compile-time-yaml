file(GLOB example-sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(source ${example-sources})
  get_filename_component(example "${source}" NAME_WE)
  add_executable(ctyaml-example-${example} "${source}")
  target_link_libraries(ctyaml-example-${example} PRIVATE ctyaml::ctyaml)
  if(CTYAML_PCH AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
    target_precompile_headers(ctyaml-example-${example} PRIVATE
      "${PROJECT_SOURCE_DIR}/include/ctyaml.hpp")
  endif()
  add_test(NAME example-${example} COMMAND ctyaml-example-${example})
endforeach()
