# Find Live2D Cubism Native Framework
#
#  LIVE2D_FOUND
#  LIVE2D_INCLUDE_DIRS
#  LIVE2D_LIBRARIES

set(_LIVE2D_HINTS
    "${PROJECT_SOURCE_DIR}/third_party/CubismNativeFramework"
    "${CMAKE_SOURCE_DIR}/third_party/CubismNativeFramework"
)

find_path(LIVE2D_INCLUDE_DIR
    NAMES CubismFramework.hpp
    HINTS ${_LIVE2D_HINTS}
    PATH_SUFFIXES
        Framework/src
        src
)

# Live2D Core is distributed separately from the framework in many setups.
# For now, we only require framework headers and allow wrapper-only linking.
set(LIVE2D_LIBRARY "")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Live2D
    REQUIRED_VARS LIVE2D_INCLUDE_DIR
)

if(LIVE2D_FOUND)
    set(LIVE2D_INCLUDE_DIRS ${LIVE2D_INCLUDE_DIR})
    set(LIVE2D_LIBRARIES ${LIVE2D_LIBRARY})
endif()

mark_as_advanced(LIVE2D_INCLUDE_DIR LIVE2D_LIBRARY)
