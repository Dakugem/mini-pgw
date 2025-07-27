cmake_minimum_required(VERSION 3.23)
project("pgw_client")

file(GLOB Sources CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
	
list(REMOVE_ITEM Sources ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
	
add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

target_sources(${PROJECT_NAME} PRIVATE ${Sources})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_NAME} PRIVATE ${Libs})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}_config.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/log)
file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/test_log)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test_log)

if(BUILD_TESTING)
	file(GLOB Test_Sources CONFIGURE_DEPENDS
		${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
		)
		
	add_executable(${PROJECT_NAME}_test ${Test_Sources})
	
	target_sources(${PROJECT_NAME}_test PRIVATE ${Sources})
	target_include_directories(${PROJECT_NAME}_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
	
	target_link_libraries(${PROJECT_NAME}_test PRIVATE ${Libs} gtest_main gmock)
	
	add_test(NAME ${PROJECT_NAME}_TEST COMMAND ${PROJECT_NAME}_test)
endif()
