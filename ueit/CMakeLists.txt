
find_package(SDL2 CONFIG REQUIRED)

add_executable(ueit ueit.c)
target_link_libraries(ueit PRIVATE SDL2::SDL2 libsecam)

install(TARGETS ueit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

file(GLOB BITMAPS *.bmp)

foreach(BITMAP IN LISTS BITMAPS)
    add_custom_command(TARGET ueit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${BITMAP} $<TARGET_FILE_DIR:ueit>
        COMMENT "Copying bitmap: ${BITMAP}")
    install(FILES ${BITMAP} DESTINATION ${CMAKE_INSTALL_DATADIR}/ueit)
endforeach()

