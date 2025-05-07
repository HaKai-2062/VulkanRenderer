cmake_minimum_required(VERSION 3.15)
project(libktx)

find_package(Vulkan REQUIRED)

set(KTX_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/lib/texture.c
	${CMAKE_CURRENT_SOURCE_DIR}/lib/hashlist.c
	${CMAKE_CURRENT_SOURCE_DIR}/lib/checkheader.c
	${CMAKE_CURRENT_SOURCE_DIR}/lib/swap.c
	${CMAKE_CURRENT_SOURCE_DIR}/lib/memstream.c
	${CMAKE_CURRENT_SOURCE_DIR}/lib/filestream.c
	${CMAKE_CURRENT_SOURCE_DIR}/lib/vkloader.c
)
set(KTX_INCLUDE
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}/lib
	${CMAKE_CURRENT_SOURCE_DIR}/other_include
)

add_library(libktx ${KTX_SOURCES})
target_include_directories(libktx PUBLIC ${KTX_INCLUDE} ${Vulkan_INCLUDE_DIRS})
target_link_libraries(libktx ${Vulkan_LIBRARIES})