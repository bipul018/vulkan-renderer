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

set(CoreSourceFiles common-stuff.c
		   render-stuff.c
		   window-stuff.c
		   device-mem-stuff.c
		   LooplessSizeMove.c
		   stuff.c
		   pipe1.c
		   pipeline-helpers.c
		   compute-test.c
		   compute-interface.c)
set(CoreHeaderFiles common-stuff.h
		     render-stuff.h
		     window-stuff.h
		     device-mem-stuff.h
		     LooplessSizeMove.h
		     stuff.h
		     pipe1.h
		     pipeline-helpers.h
		     compute-interface.h
		     compute-test.h)

set(CoreIncludePaths ${PROJECT_SOURCE_DIR}
			       "${PROJECT_SOURCE_DIR}/../utilities"
			       "${PROJECT_SOURCE_DIR}/../stb"
			       "${PROJECT_SOURCE_DIR}/../VulkanSDK/1.3.280.0/Include")

set(CoreLibraryPaths "d:/prgming/VulkanSDK/1.3.280.0/Lib")

set(CoreStaticLibraries dwmapi.lib vulkan-1.lib user32.lib)

add_executable("${Project}")
target_sources(${Project} PRIVATE
		   main.c ${CoreSourceFiles})

# TODO:: Maybe later this can be better solved using INTERFACE
target_sources(${Project} PRIVATE ${CoreHeaderFiles})
target_include_directories(${Project} PRIVATE ${CoreIncludePaths})
target_link_directories(${Project} PRIVATE ${CoreLibraryPaths})

target_link_libraries(${Project} PRIVATE ${CoreStaticLibraries})

#The dll of everything except main.c
add_library(${Project}-dll SHARED)
set_property(TARGET ${Project}-dll PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_sources(${Project}-dll PRIVATE ${CoreSourceFiles})
# TODO:: Maybe later this can be better solved using INTERFACE
target_sources(${Project}-dll PRIVATE ${CoreHeaderFiles})
target_include_directories(${Project}-dll PRIVATE ${CoreIncludePaths})
target_link_directories(${Project}-dll PRIVATE ${CoreLibraryPaths})
target_link_libraries(${Project}-dll PRIVATE ${CoreStaticLibraries})


#The hot reloadable dll for quick testing and configuring
add_library(dll-test SHARED)
target_sources(dll-test PRIVATE dll-test.c vectors.h)

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
add_shader_step(shaders "shaders/naccl.comp" "shaders/glsl.h")
add_shader_step(shaders "shaders/nbody.comp" "shaders/glsl.h")
target_sources(${Project} PRIVATE "shaders/glsl.h")

add_dependencies(${Project} shaders)


#copy over resources

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/res)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/res DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

install(TARGETS ${Project})
		   

