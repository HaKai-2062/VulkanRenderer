# CMakeList.txt : CMake project for VulkanRenderer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.15)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "x64 Debug")
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
else()
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVK_PROTOTYPES")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_PROTOTYPES")
#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) #link time optimization

project ("VulkanRenderer")

include(FetchContent)

function(CloneRepository repositoryURL branchName projectName sourceDir customCMakePath)
	message("Starting to clone ${projectName} into ${sourceDir}")
		
	FetchContent_Declare(
		"${projectName}"
		GIT_REPOSITORY "${repositoryURL}"
		SOURCE_DIR "${sourceDir}"
		GIT_TAG "origin/${branchName}"
		SOURCE_SUBDIR  "${customCMakePath}"
		CONFIGURE_COMMAND ""
		BUILD_COMMAND ""
		INSTALL_COMMAND ""
	)

	FetchContent_MakeAvailable(${projectName})

endfunction(CloneRepository)

CloneRepository("https://github.com/glfw/glfw.git" "master" "GLFW" "${CMAKE_CURRENT_SOURCE_DIR}/vendor/GLFW" "")
CloneRepository("https://github.com/ocornut/imgui.git" "docking" "imgui-docking" "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui-docking" "../CMake/imgui-docking")
CloneRepository("https://github.com/g-truc/glm.git" "master" "GLM" "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm" "")
CloneRepository("https://github.com/nothings/stb.git" "master" "STB" "${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb" "")
CloneRepository("https://github.com/tinyobjloader/tinyobjloader.git" "release" "tinyobjloader" "${CMAKE_CURRENT_SOURCE_DIR}/vendor/tinyobjloader" "")
CloneRepository("https://github.com/charles-lunarg/vk-bootstrap.git" "main" "vk-bootstrap" "${CMAKE_CURRENT_SOURCE_DIR}/vendor/vk-bootstrap" "")
CloneRepository("https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git" "master" "VulkanMemoryAllocator" "${CMAKE_CURRENT_SOURCE_DIR}/vendor/VMA" "")
CloneRepository("https://github.com/fmtlib/fmt.git" "master" "fmt" "${CMAKE_CURRENT_SOURCE_DIR}/vendor/fmt" "")
CloneRepository("https://github.com/spnda/fastgltf.git" "main" "fastgltf" "${CMAKE_CURRENT_SOURCE_DIR}/vendor/fastgltf" "")

option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
option(GLFW_DOCUMENT_INTERNALS "Include internals in documentation" OFF)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(vendor/GLFW)
add_subdirectory(vendor/glm)
add_subdirectory(vendor/tinyobjloader)
add_subdirectory(vendor/vk-bootstrap)
add_subdirectory(vendor/VMA)
add_subdirectory(vendor/fmt)
add_subdirectory(vendor/fastgltf)

# Dont specify dirs with custom CMakefiles.txt that arent in Git repo?
#add_subdirectory(vendor/CMake/imgui-docking)

file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui-docking/backends/imgui_impl_glfw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui-docking/backends/imgui_impl_vulkan.cpp"
)
source_group("Source Files" FILES ${MY_SOURCES})
file(GLOB_RECURSE MY_HEADERS CONFIGURE_DEPENDS 
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)
source_group("Header Files" FILES ${MY_HEADERS})

add_executable("${CMAKE_PROJECT_NAME}" ${MY_SOURCES} ${MY_HEADERS})
set_property(TARGET "${CMAKE_PROJECT_NAME}" PROPERTY CXX_STANDARD 20)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT VulkanRenderer)

target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC 
GLFW_INCLUDE_NONE=1)


if(MSVC) # If using the VS compiler...

	set_target_properties(${BUILD_TARGET} PROPERTIES LINK_FLAGS "/PROFILE") # for profiling

	#target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC _CRT_SECURE_NO_WARNINGS)		# secure function warnings

	#set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup") #no console
	
	#set_property(TARGET "${CMAKE_PROJECT_NAME}" PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDebug<$<CONFIG:Debug>:Debug>")

endif()

find_package(Vulkan REQUIRED)

target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC 
"${CMAKE_CURRENT_SOURCE_DIR}/include"
"${CMAKE_CURRENT_SOURCE_DIR}/vendor/GLFW/include"
"${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui-docking"
"${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui-docking/backends/"
"${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm/glm"
"${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb/"
"${CMAKE_CURRENT_SOURCE_DIR}/vendor/tinyobjloader"
"${CMAKE_CURRENT_SOURCE_DIR}/vendor/vk-bootstrap"
"${CMAKE_CURRENT_SOURCE_DIR}/vendor/VMA"
"${Vulkan_INCLUDE_DIRS}")

target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE 
glfw
imgui
glm
tinyobjloader
vk-bootstrap
VulkanMemoryAllocator
fmt
fastgltf
${Vulkan_LIBRARIES})