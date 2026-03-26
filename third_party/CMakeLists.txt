# All third party libraries
add_subdirectory(lua-5.3.5)
include(CheckCXXCompilerFlag)

#cpr test programs
option(CPR_BUILD_TESTS "Set to ON to build cpr tests." OFF)
set(CPR_ENABLE_SSL ON)
#TODO: Don't have two curl builds in the game build
#if(MSVC)
#    set(CPR_FORCE_USE_SYSTEM_CURL ON) #curl should be available through vcpkg
#endif(MSVC)
if(USE_SYSTEM_CPR)
    find_package(cpr REQUIRED)
else()
    add_subdirectory(cpr)
    target_include_directories(cpr PUBLIC ${CPR_INCLUDE_DIRS})
endif(USE_SYSTEM_CPR)

#discord example program
set(CLANG_FORMAT_SUFFIX "ignore") #try to disable clang formatter
option(BUILD_EXAMPLES "Build example apps" OFF)
add_subdirectory(discord-rpc)
target_include_directories(discord-rpc PUBLIC discord-rpc/include)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # GCC 15 reports a template-body warning in discord-rpc's bundled rapidjson 1.1.0.
    # Treat that specific warning as non-fatal only when this flag is supported by the toolchain.
    check_cxx_compiler_flag("-Wno-error=template-body" USC_HAS_WNO_ERROR_TEMPLATE_BODY)
    if(USC_HAS_WNO_ERROR_TEMPLATE_BODY)
        target_compile_options(discord-rpc PRIVATE -Wno-error=template-body)
    endif()
endif()

#nanovg
add_library(nanovg nanovg/src/nanovg.c)
target_include_directories(nanovg PUBLIC nanovg/src)
target_link_libraries(nanovg ${FREETYPE_LIBRARIES})
target_include_directories(nanovg PRIVATE ${FREETYPE_INCLUDE_DIRS})
target_compile_definitions(nanovg PRIVATE -DFONS_USE_FREETYPE)

#miniaudio
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio)

#json for modern c++
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json)

#sqlite
add_library(sqlite3
    sqlite3/sqlite3.c
    sqlite3/sqlite3.h
    sqlite3/sqlite3ext.h
)
target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3)

#rapidjson -- add as interface library to prevent building it
add_library(rapidjson INTERFACE)
target_include_directories(rapidjson INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/rapidjson)

#minimp3
add_library(minimp3
    minimp3/minimp3.c
    minimp3/minimp3.h
    minimp3/libc.h
)
target_include_directories(minimp3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/minimp3)

#soundtouch
add_library(soundtouch
    soundtouch/src/AAFilter.cpp
    soundtouch/src/AAFilter.h
    soundtouch/src/BPMDetect.cpp
    soundtouch/include/BPMDetect.h
    soundtouch/src/cpu_detect_x86.cpp
    soundtouch/src/cpu_detect.h
    soundtouch/src/FIFOSampleBuffer.cpp
    soundtouch/include/FIFOSampleBuffer.h
    soundtouch/include/FIFOSamplePipe.h
    soundtouch/src/FIRFilter.cpp
    soundtouch/src/FIRFilter.h
    soundtouch/src/InterpolateCubic.cpp
    soundtouch/src/InterpolateCubic.h
    soundtouch/src/InterpolateLinear.cpp
    soundtouch/src/InterpolateLinear.h
    soundtouch/src/InterpolateShannon.cpp
    soundtouch/src/InterpolateShannon.h
    soundtouch/src/mmx_optimized.cpp
    soundtouch/src/PeakFinder.cpp
    soundtouch/src/PeakFinder.h
    soundtouch/src/RateTransposer.cpp
    soundtouch/src/RateTransposer.cpp
    soundtouch/include/soundtouch_config.h
    soundtouch/src/SoundTouch.cpp
    soundtouch/include/SoundTouch.h
    soundtouch/include/STTypes.h
    soundtouch/src/sse_optimized.cpp
    soundtouch/src/TDStretch.cpp
    soundtouch/src/TDStretch.h
)
target_include_directories(soundtouch PUBLIC soundtouch/include)
target_include_directories(soundtouch PRIVATE soundtouch/src)

#GLEW
add_library(GLEW
    glew/include/GL/glew.h
    glew/include/GL/glxew.h
    glew/include/GL/wglew.h
    glew/src/glew.c
)
# GLEW is included statically and also doesn't need GLU(Which doesn't even exist on linux)
target_compile_definitions(GLEW PUBLIC -DGLEW_NO_GLU -DGLEW_STATIC)
target_include_directories(GLEW PUBLIC glew/include)

# Live2D Cubism Native Framework (optional)
if(ENABLE_LIVE2D)
    # Import the prebuilt Cubism Core library
    add_library(Live2DCubismCore STATIC IMPORTED)
    set(CUBISM_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/CubismSdkCore)
    if(WIN32)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(_CUBISM_CORE_LIB_DIR ${CUBISM_CORE_DIR}/lib/windows/x86_64/143)
        else()
            set(_CUBISM_CORE_LIB_DIR ${CUBISM_CORE_DIR}/lib/windows/x86/143)
        endif()
        set_target_properties(Live2DCubismCore PROPERTIES
            IMPORTED_LOCATION ${_CUBISM_CORE_LIB_DIR}/Live2DCubismCore_MT.lib
            IMPORTED_LOCATION_DEBUG ${_CUBISM_CORE_LIB_DIR}/Live2DCubismCore_MTd.lib
        )
    elseif(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(_CUBISM_CORE_LIB_DIR ${CUBISM_CORE_DIR}/lib/macos/arm64)
        else()
            set(_CUBISM_CORE_LIB_DIR ${CUBISM_CORE_DIR}/lib/macos/x86_64)
        endif()
        set_target_properties(Live2DCubismCore PROPERTIES
            IMPORTED_LOCATION ${_CUBISM_CORE_LIB_DIR}/libLive2DCubismCore.a
        )
    else()
        set_target_properties(Live2DCubismCore PROPERTIES
            IMPORTED_LOCATION ${CUBISM_CORE_DIR}/lib/linux/x86_64/libLive2DCubismCore.a
        )
    endif()
    set_target_properties(Live2DCubismCore PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${CUBISM_CORE_DIR}/include
    )

    # Build the CubismNativeFramework with OpenGL renderer
    set(FRAMEWORK_SOURCE OpenGL CACHE STRING "" FORCE)
    set(FRAMEWORK_GLEW_PATH ${CMAKE_CURRENT_SOURCE_DIR}/glew/include CACHE STRING "" FORCE)
    add_subdirectory(CubismNativeFramework)
    target_include_directories(Framework PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/CubismNativeFramework/src
    )
    target_include_directories(Framework PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/glew/include
    )
    # Set the appropriate platform define for the OpenGL renderer
    if(WIN32)
        target_compile_definitions(Framework PUBLIC CSM_TARGET_WIN_GL)
    elseif(APPLE)
        target_compile_definitions(Framework PUBLIC CSM_TARGET_MAC_GL)
    else()
        target_compile_definitions(Framework PUBLIC CSM_TARGET_LINUX_GL)
    endif()
    target_link_libraries(Framework Live2DCubismCore)
endif()

# Enable multiprocess compiling
if(MSVC)
    target_compile_options(cpr PRIVATE /MP)
    target_compile_options(discord-rpc PRIVATE /MP)
    target_compile_options(GLEW PRIVATE /MP)
    target_compile_options(lua PRIVATE /MP)
    target_compile_options(minimp3 PRIVATE /MP)
    target_compile_options(nanovg PRIVATE /MP)
    target_compile_options(soundtouch PRIVATE /MP)
    target_compile_options(sqlite3 PRIVATE /MP)
    if(ENABLE_LIVE2D)
        target_compile_options(Framework PRIVATE /MP)
    endif()
endif(MSVC)
