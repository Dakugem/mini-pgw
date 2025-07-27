cmake_minimum_required(VERSION 3.23)
project("io_utils")

#Проверить какой именно стандарт действительно нужен
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB Sources CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
	)

add_library(${PROJECT_NAME} STATIC ${Sources})

target_include_directories(${PROJECT_NAME} 
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PUBLIC quill::quill)
target_compile_options(${PROJECT_NAME} PRIVATE "-Werror" "-Wall" "-Wextra" "-Wpedantic" "-Wno-error=maybe-uninitialized")

if(BUILD_TESTING)
	file(GLOB Test_Sources CONFIGURE_DEPENDS
		${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
		)
		
	add_executable(${PROJECT_NAME}_test ${Test_Sources})
	
	target_link_libraries(${PROJECT_NAME}_test PRIVATE ${PROJECT_NAME} gtest_main gmock)
	
	add_test(NAME ${PROJECT_NAME}_TEST COMMAND ${PROJECT_NAME}_test)
endif()
