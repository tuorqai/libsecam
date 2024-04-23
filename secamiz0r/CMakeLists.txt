
set(SOURCES secamiz0r.c frei0r.h)
set(SOURCES_EX secamiz0r_ex.c frei0r.h)

set(TARGET secamiz0r)
set(TARGET_EX secamiz0r_ex)

if(MSVC)
    set(SOURCES ${SOURCES} frei0r_1_0.def)
    set(SOURCES_EX ${SOURCES_EX} frei0r_1_0.def)
endif()

add_library(${TARGET} MODULE ${SOURCES})
#add_library(${TARGET_EX} MODULE ${SOURCES_EX})
target_link_libraries(${TARGET} PRIVATE libsecam)
#target_link_libraries(${TARGET_EX} PRIVATE libsecam)
set_target_properties(${TARGET} PROPERTIES PREFIX "")
install(TARGETS ${TARGET} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
