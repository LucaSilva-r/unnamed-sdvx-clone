# Find Live2D Cubism Native Framework + Core
#
#  LIVE2D_FOUND
#  LIVE2D_INCLUDE_DIRS
#  LIVE2D_LIBRARIES

# Framework headers (CubismNativeFramework submodule)
find_path(LIVE2D_FRAMEWORK_INCLUDE_DIR
    NAMES CubismFramework.hpp
    HINTS
        "${PROJECT_SOURCE_DIR}/third_party/CubismNativeFramework/src"
)

# Core header (extracted from Cubism SDK zip)
find_path(LIVE2D_CORE_INCLUDE_DIR
    NAMES Live2DCubismCore.h
    HINTS
        "${PROJECT_SOURCE_DIR}/third_party/CubismSdkCore/include"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Live2D
    REQUIRED_VARS LIVE2D_FRAMEWORK_INCLUDE_DIR LIVE2D_CORE_INCLUDE_DIR
)

if(LIVE2D_FOUND)
    set(LIVE2D_INCLUDE_DIRS
        ${LIVE2D_FRAMEWORK_INCLUDE_DIR}
        ${LIVE2D_CORE_INCLUDE_DIR}
    )
    # Linking is handled via the Framework and Live2DCubismCore targets
    # built in third_party/CMakeLists.txt
    set(LIVE2D_LIBRARIES Framework Live2DCubismCore)
endif()

mark_as_advanced(LIVE2D_FRAMEWORK_INCLUDE_DIR LIVE2D_CORE_INCLUDE_DIR)
