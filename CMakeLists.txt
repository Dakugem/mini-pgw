cmake_minimum_required(VERSION 3.23)
project("mini_pgw")

#Проверить какой именно стандарт действительно нужен
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
include(FetchContent)

set(QUILL_ENABLE_INSTALL ON)
FetchContent_Declare(quill
	GIT_REPOSITORY 	https://github.com/odygrd/quill.git
	SOURCE_DIR 	${CMAKE_CURRENT_SOURCE_DIR}/libs/external/quill
	GIT_TAG		v10.0.1
	)

FetchContent_Declare(json_headers
	URL 				https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
	SOURCE_DIR 			${CMAKE_CURRENT_SOURCE_DIR}/libs/external/nlohmann_json
	DOWNLOAD_EXTRACT_TIMESTAMP 	true
	)

FetchContent_Declare(picohttpparser_headers
	GIT_REPOSITORY 	https://github.com/h2o/picohttpparser.git
	SOURCE_DIR 	${CMAKE_CURRENT_SOURCE_DIR}/libs/external/picohttpparser
	GIT_TAG		master
	)	
	
FetchContent_MakeAvailable(quill json_headers)

add_library(json INTERFACE)
target_include_directories(json INTERFACE ${json_headers_SOURCE_DIR}/single_include)

if(BUILD_TESTING)
	message(STATUS "TESTS ON")

	FetchContent_Declare(googletest
		GIT_REPOSITORY 	https://github.com/google/googletest.git
		SOURCE_DIR 	${CMAKE_CURRENT_SOURCE_DIR}/libs/external/google/googletest
		GIT_TAG 	v1.17.0
		)
	FetchContent_MakeAvailable(googletest)
else()
	message(STATUS "TESTS OFF")
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/io_utils)

set(Libs io_utils json)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/pgw_server)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/pgw_client)
