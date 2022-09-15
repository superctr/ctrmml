# TODO: set static build flags on windows - best option?

cmake_minimum_required(VERSION 3.10)
project(ctrmml)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CPPUNIT cppunit)

add_library(ctrmml
	src/track.cpp
	src/song.cpp
	src/input.cpp
	src/mml_input.cpp
	src/player.cpp
	src/stringf.cpp
	src/vgm.cpp
	src/driver.cpp
	src/wave.cpp
	src/riff.cpp
	src/conf.cpp
	src/optimizer.cpp
	src/platform/md.cpp
	src/platform/mdsdrv.cpp)
target_include_directories(ctrmml PUBLIC src)

add_executable(mmlc src/mmlc.cpp)
target_link_libraries(mmlc ctrmml)

add_executable(mdslink src/platform/mdslink.cpp)
target_link_libraries(mdslink ctrmml)

if(CPPUNIT_FOUND)
	add_executable(ctrmml_unittest
		src/unittest/test_track.cpp
		src/unittest/test_song.cpp
		src/unittest/test_input.cpp
		src/unittest/test_mml_input.cpp
		src/unittest/test_player.cpp
		src/unittest/test_vgm.cpp
		src/unittest/test_riff.cpp
		src/unittest/test_conf.cpp
		src/unittest/test_mdsdrv.cpp
		src/unittest/test_misc.cpp
		src/unittest/main.cpp)
	target_link_libraries(ctrmml_unittest ctrmml)
	target_link_libraries(ctrmml_unittest ${CPPUNIT_LIBRARIES})
	enable_testing()
	add_test(NAME run_ctrmml_unittest COMMAND ctrmml_unittest WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
