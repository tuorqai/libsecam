
cmake_minimum_required(VERSION 3.1)
project(libsecam)

add_library(libsecam INTERFACE)
target_include_directories(libsecam INTERFACE ${CMAKE_SOURCE_DIR})

find_library(LIBM m)

if(LIBM)
    target_link_libraries(libsecam INTERFACE ${LIBM})
endif()

include(GNUInstallDirs)
install(FILES libsecam.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

add_subdirectory(secamiz0r)
add_subdirectory(ueit)

option(LIBSECAM_USE_THREADS "Enable multithreading" ON)

if(LIBSECAM_USE_THREADS)
    find_package(Threads REQUIRED)
    target_compile_definitions(libsecam INTERFACE LIBSECAM_USE_THREADS)
    target_link_libraries(libsecam INTERFACE Threads::Threads)
endif()
