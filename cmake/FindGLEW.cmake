if(NOT GLEW_FIND_VERSION)
	set(GLEW_FIND_VERSION 2.2.0)
endif()

set(GLEW_ARCHIVE_FILE_NAME "glew-${GLEW_FIND_VERSION}.zip")

if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/glew/${GLEW_ARCHIVE_FILE_NAME}")
	file(
			DOWNLOAD
			"https://github.com/nigels-com/glew/releases/download/glew-${GLEW_FIND_VERSION}/${GLEW_ARCHIVE_FILE_NAME}"
			"${CMAKE_CURRENT_BINARY_DIR}/glew/${GLEW_ARCHIVE_FILE_NAME}"
			SHOW_PROGRESS
	)
endif()

set(GLEW_ARCHIVE_DIR "${CMAKE_CURRENT_BINARY_DIR}/glew/glew-${GLEW_FIND_VERSION}")

file(MAKE_DIRECTORY "${GLEW_ARCHIVE_DIR}")
execute_process(
		COMMAND ${CMAKE_COMMAND} -E tar xzf "${CMAKE_CURRENT_BINARY_DIR}/glew/${GLEW_ARCHIVE_FILE_NAME}"
		WORKING_DIRECTORY "${GLEW_ARCHIVE_DIR}"
)

set(GLEW_DIR "${GLEW_ARCHIVE_DIR}/glew-${GLEW_FIND_VERSION}")

set(GLEW_INCLUDE_DIRS "${GLEW_DIR}/include")
set(GLEW_SRC "${GLEW_DIR}/src/glew.c")

add_library(GLEW::GLEW INTERFACE IMPORTED)
target_include_directories(GLEW::GLEW INTERFACE ${GLEW_INCLUDE_DIRS})
target_compile_definitions(GLEW::GLEW INTERFACE -DGLEW_STATIC)
target_sources(GLEW::GLEW INTERFACE ${GLEW_SRC})
