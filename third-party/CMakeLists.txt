

set(INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/uthash/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tinyprintf
)

set(LIBS
        tinyprintf
)

add_library(tinyprintf tinyprintf/tinyprintf.c)

if (NOT CMAKE_CROSSCOMPILING)
    SET(cmocka_dir ${CMAKE_CURRENT_SOURCE_DIR}/../third-party/cmocka)
    include(CheckCCompilerFlag)
    include(${cmocka_dir}/ConfigureChecks.cmake)
    configure_file(${cmocka_dir}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    include_directories(${cmocka_dir}/include)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../third-party/uthash/src)
    if (CMAKE_COMPILER_IS_GNUCC)
        add_definitions(-Wno-format)
    endif()
    add_library(cmocka STATIC
            ${cmocka_dir}/src/cmocka.c)
    target_compile_definitions(cmocka PRIVATE -DHAVE_CONFIG_H=1)
    set(CMOCKA_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/cmocka/include CACHE INTERNAL "cmocka include" FORCE)
    set(CMOCKA_LIBS cmocka CACHE INTERNAL "cmocka lib" FORCE)
endif()

set(THIRD_PARTY_INCLUDES ${INCLUDES} PARENT_SCOPE)
set(THIRD_PARTY_LIBS ${LIBS} PARENT_SCOPE)
