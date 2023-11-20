
set(SOURCES secamiz0r.c frei0r.h)
set(TARGET secamiz0r)

if(MSVC)
    set(SOURCES ${SOURCES} frei0r_1_0.def)
endif()

add_library(${TARGET} MODULE ${SOURCES})
target_link_libraries(${TARGET} PRIVATE libsecam)
set_target_properties(${TARGET} PROPERTIES PREFIX "")
install(TARGETS ${TARGET} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

