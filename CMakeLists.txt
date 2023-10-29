cmake_minimum_required(VERSION 3.0.0)
project(project)
set(CMAKE_MODULE_PATH "/home/jslucena/rpclib/cmake")
find_package(rpclib REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Cryptopp REQUIRED IMPORTED_TARGET libcrypto++)
include_directories(${RPCLIB_INCLUDE_DIR})

add_executable(server server.cc)
#target_link_libraries(server PUBLIC PkgConfig::Cryptopp)
target_link_libraries(server PUBLIC ${RPCLIB_LIBS} ${CMAKE_THREAD_LIBS_INIT} PkgConfig::Cryptopp)
set_target_properties(
        server
        PROPERTIES
        CXX_STANDARD 14
        COMPILE_FLAGS "${CMAKE_CXX_FLAGS} ${RPCLIB_EXTRA_FLAGS}")
target_compile_definitions(server PUBLIC ${RPCLIB_COMPILE_DEFINITIONS})

add_executable(client client.cc)
target_link_libraries(client ${RPCLIB_LIBS} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(
        client
        PROPERTIES
        CXX_STANDARD 14
        COMPILE_FLAGS "${CMAKE_CXX_FLAGS} ${RPCLIB_EXTRA_FLAGS}")
target_compile_definitions(client PUBLIC ${RPCLIB_COMPILE_DEFINITIONS})
