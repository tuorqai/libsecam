
find_package(SDL2 CONFIG REQUIRED)

add_executable(ueit ueit.c)
target_link_libraries(ueit PRIVATE SDL2::SDL2 SDL2::SDL2main libsecam)

if(WIN32)
    set(UEIT_INSTALL_DIR "ueit")
else()
    set(UEIT_INSTALL_DIR "${CMAKE_INSTALL_LIBEXECDIR}/ueit")
endif()

install(TARGETS ueit RUNTIME DESTINATION ${UEIT_INSTALL_DIR})

file(GLOB BITMAPS *.bmp)

foreach(BITMAP IN LISTS BITMAPS)
    add_custom_command(TARGET ueit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${BITMAP} $<TARGET_FILE_DIR:ueit>
        COMMENT "Copying bitmap: ${BITMAP}")
    install(FILES ${BITMAP} DESTINATION ${UEIT_INSTALL_DIR})
endforeach()

if(WIN32 AND (${CMAKE_VERSION} VERSION_GREATER "3.20"))
    add_custom_command(TARGET ueit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:ueit> $<TARGET_FILE_DIR:ueit>
        COMMAND_EXPAND_LISTS)
    install(FILES $<TARGET_RUNTIME_DLLS:ueit> DESTINATION ${UEIT_INSTALL_DIR})
endif()
