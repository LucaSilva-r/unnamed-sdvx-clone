# Graphics library

set(INCROOT ${CMAKE_CURRENT_SOURCE_DIR}/include/Graphics/)
set(SRCROOT ${CMAKE_CURRENT_SOURCE_DIR}/src/)
set(PCHROOT ${CMAKE_CURRENT_SOURCE_DIR}/)

file(GLOB INC "${INCROOT}/*.hpp")
source_group("Public Headers" FILES ${INC})

file(GLOB SRC "${SRCROOT}/*.cpp" "${SRCROOT}/*.hpp")
source_group("Sources" FILES ${SRC})

# Find files used for project
set(GRAPHICS_SRC ${SRC} ${INC})

set(PCH_SRC ${PCHROOT}/stdafx.cpp)
set(PCH_INC ${PCHROOT}/stdafx.h)
set(PCH_FILES ${PCH_SRC} ${PCH_INC})
source_group("" FILES ${PCH_FILES})

# Compiler stuff
enable_precompiled_headers("${GRAPHICS_SRC}" ${PCH_SRC})

add_library(Graphics ${GRAPHICS_SRC} ${PCH_FILES})
target_compile_features(Graphics PUBLIC cxx_std_17)
target_include_directories(Graphics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(Graphics PRIVATE
    ${INCROOT}
    ${SRCROOT}
    ${PCHROOT}
)

# Dependencies
## Local Dependencies

target_link_libraries(Graphics Shared)

## 3rd Party dependencies
target_include_directories(Graphics PUBLIC ${SDL2_INCLUDE_DIR})
target_include_directories(Graphics PRIVATE SYSTEM ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(Graphics ${FREETYPE_LIBRARIES})
target_link_libraries(Graphics ${SDL2_LIBRARY})
target_link_libraries(Graphics ${JPEG_LIBRARIES})
target_link_libraries(Graphics ${PNG_LIBRARIES})
target_link_libraries(Graphics GLEW)
if(UNIX)
    target_link_libraries(Graphics pthread) 
    target_link_libraries(Graphics dl)
    if(APPLE)
        find_library(OPEN_GL OpenGL)
        target_link_libraries(Graphics ${OPEN_GL})
    else(APPLE)
        target_link_libraries(Graphics GL)
        find_package(X11 REQUIRED)
        include_directories(${X11_INCLUDE_DIR})
    endif(APPLE)
endif(UNIX)


if(ENABLE_VIDEO)
    set(FFMPEG_COMPONENTS
        libavcodec
        libavformat
        libswscale
        libswresample
        libavutil
    )

    set(FFMPEG_LINKED FALSE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBAV QUIET IMPORTED_TARGET ${FFMPEG_COMPONENTS})
        if(LIBAV_FOUND)
            target_link_libraries(Graphics PkgConfig::LIBAV)
            set(FFMPEG_LINKED TRUE)
        endif()
    endif()

    if(NOT FFMPEG_LINKED)
        # Manual fallback for environments without pkg-config metadata
        find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
        find_library(AVCODEC_LIBRARY avcodec)
        find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
        find_library(AVFORMAT_LIBRARY avformat)
        find_path(SWSCALE_INCLUDE_DIR libswscale/swscale.h)
        find_library(SWSCALE_LIBRARY swscale)
        find_path(SWRESAMPLE_INCLUDE_DIR libswresample/swresample.h)
        find_library(SWRESAMPLE_LIBRARY swresample)
        find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
        find_library(AVUTIL_LIBRARY avutil)

        if(NOT AVCODEC_INCLUDE_DIR OR NOT AVFORMAT_INCLUDE_DIR OR NOT SWSCALE_INCLUDE_DIR
           OR NOT SWRESAMPLE_INCLUDE_DIR OR NOT AVUTIL_INCLUDE_DIR
           OR NOT AVCODEC_LIBRARY OR NOT AVFORMAT_LIBRARY OR NOT SWSCALE_LIBRARY
           OR NOT SWRESAMPLE_LIBRARY OR NOT AVUTIL_LIBRARY)
            message(FATAL_ERROR
                "ENABLE_VIDEO=ON but FFmpeg dependencies were not found. "
                "Install FFmpeg development packages (avcodec/avformat/swscale/swresample/avutil) "
                "or configure with -DENABLE_VIDEO=OFF.")
        endif()

        target_include_directories(Graphics PRIVATE
            ${AVCODEC_INCLUDE_DIR}
            ${AVFORMAT_INCLUDE_DIR}
            ${SWSCALE_INCLUDE_DIR}
            ${SWRESAMPLE_INCLUDE_DIR}
            ${AVUTIL_INCLUDE_DIR}
        )
        target_link_libraries(Graphics
            ${AVCODEC_LIBRARY}
            ${AVFORMAT_LIBRARY}
            ${SWSCALE_LIBRARY}
            ${SWRESAMPLE_LIBRARY}
            ${AVUTIL_LIBRARY}
        )
        set(FFMPEG_LINKED TRUE)
    endif()

    if(NOT FFMPEG_LINKED)
        message(FATAL_ERROR
            "ENABLE_VIDEO=ON but FFmpeg could not be configured for Graphics target.")
    endif()

    target_compile_definitions(Graphics PUBLIC ENABLE_VIDEO=1)
endif()

set_werror(Graphics)

target_link_libraries(Graphics cc-common)

# Enable multiprocess compiling
if(MSVC)
    target_compile_options(Graphics PRIVATE /MP)
endif(MSVC)
