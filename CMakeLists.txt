cmake_minimum_required(VERSION 3.22)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(Project "vulk-app")
project(${Project} LANGUAGES C)



#if (MSVC)
#    add_compile_options(/W4 /fsanitize=address)
#else()
#    add_compile_options(-Wall -Wextra -Wpedantic -fsanitize=address)
#endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/lib>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/lib>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/bin>)

add_executable("${Project}")

# set_target_properties(${Project} PROPERTIES EXPORT_COMPILE_COMMANDS ON)

target_sources(${Project} PRIVATE
		   main.c
		   common-stuff.c
		   render-stuff.c
		   window-stuff.c
		   device-mem-stuff.c
		   LooplessSizeMove.c
		   stuff.c
		   pipe1.c
		   pipeline-helpers.c
		   compute-test.c
		   compute-interface.c)

# TODO:: Maybe later this can be better solved using INTERFACE
target_sources(${Project} PRIVATE common-stuff.h
		     render-stuff.h
		     window-stuff.h
		     device-mem-stuff.h
		     LooplessSizeMove.h
		     stuff.h
		     pipe1.h
		     pipeline-helpers.h
		     compute-interface.h
		     compute-test.h)


target_include_directories(${Project} PRIVATE ${PROJECT_SOURCE_DIR}
			       "${PROJECT_SOURCE_DIR}/../utilities"
			       "${PROJECT_SOURCE_DIR}/../stb"
			       "${PROJECT_SOURCE_DIR}/../VulkanSDK/1.3.280.0/Include")

target_link_directories(${Project} PRIVATE "d:/prgming/VulkanSDK/1.3.280.0/Lib")
target_link_libraries(${Project} PRIVATE dwmapi.lib vulkan-1.lib user32.lib)

# Handle shaders

add_custom_target(shaders)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shaders)

# first file is the only shader file, rest are just dependencies
function (add_shader_step target-name input-file)
  set(xs "")
    foreach(arg IN LISTS ARGV)
      set( xs "${xs} ${arg}")
    endforeach()

  add_custom_command(TARGET ${target-name}
		   COMMAND glslc.exe ${input-file} -o "${CMAKE_CURRENT_BINARY_DIR}/${input-file}.spv"
		   MAIN_DEPENDENCY ${xs}
		   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		   BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/${input-file}.spv"
		   VERBATIM
		   COMMENT "GLSLC compiling ${CMAKE_CURRENT_BINARY_DIR}/${input-file}.spv")
endfunction()

add_shader_step(shaders "shaders/glsl.vert" "shaders/glsl.h")
add_shader_step(shaders "shaders/glsl.frag" "shaders/glsl.h")
add_shader_step(shaders "shaders/glsl.comp" "shaders/glsl.h")
target_sources(${Project} PRIVATE "shaders/glsl.h")

add_dependencies(${Project} shaders)


#copy over resources

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/res)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/res DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

install(TARGETS ${Project})
		   

