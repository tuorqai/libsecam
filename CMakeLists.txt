
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

