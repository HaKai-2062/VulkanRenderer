# CMakeList.txt : CMake project for VulkanRenderer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
else()
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

project ("VulkanRenderer")

file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
add_executable ("${CMAKE_PROJECT_NAME}")
target_sources("${CMAKE_PROJECT_NAME}" PRIVATE ${MY_SOURCES})
set_property(TARGET "${CMAKE_PROJECT_NAME}" PROPERTY CXX_STANDARD 20)

if(MSVC) # If using the VS compiler...

	set_target_properties(${BUILD_TARGET} PROPERTIES LINK_FLAGS "/PROFILE") # for profiling

	#target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC _CRT_SECURE_NO_WARNINGS)		# secure function warnings

	#set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup") #no console
	
	#set_property(TARGET "${CMAKE_PROJECT_NAME}" PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebug<$<CONFIG:Debug>:Debug>")

endif()

target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC 
"C:/VulkanSDK/1.3.283.0/Include/"
"${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/glfw-3.4.bin.WIN64/include/"
"${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/stb/"
"${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/glm/")

target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE 
"C:/VulkanSDK/1.3.283.0/Lib/vulkan-1.lib" 
"${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/glfw-3.4.bin.WIN64/lib-vc2022/glfw3.lib")