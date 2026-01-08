cmake_minimum_required(VERSION 3.19)
project(LOJ1897)
set(CMAKE_CXX_STANDARD 23)

# Source files
set(SOURCE_FILES
    main.cpp
    NoesisRenderPath.h
)

# Resource files
set(RESOURCE_FILES
    app.rc
)

# Set WickedEngine paths
set(WICKED_ROOT_DIR "E:/Repos/WickedLOJ" CACHE PATH "Path to WickedLOJ repository root")
set(WICKED_ENGINE_DIR "${WICKED_ROOT_DIR}/WickedEngine")
set(WICKED_BUILD_DIR_RELEASE "${WICKED_ROOT_DIR}/cmake-build-release-visual-studio/WickedEngine" CACHE PATH "Path to WickedEngine Release build directory")
set(WICKED_BUILD_DIR_DEBUG "${WICKED_ROOT_DIR}/cmake-build-debug-visual-studio/WickedEngine" CACHE PATH "Path to WickedEngine Debug build directory")

# Set NoesisGUI paths
set(NOESISGUI_ROOT_DIR "E:/3rdparty/NoesisGUI-NativeSDK-win-3.2.10-Indie" CACHE PATH "Path to NoesisGUI root directory")
set(NOESISGUI_INCLUDE_DIR "${NOESISGUI_ROOT_DIR}/Include")
set(NOESISGUI_LIB_DIR "${NOESISGUI_ROOT_DIR}/Lib/windows_x86_64")
set(NOESISGUI_BIN_DIR "${NOESISGUI_ROOT_DIR}/Bin/windows_x86_64")
set(NOESISGUI_DATA_DIR "${NOESISGUI_ROOT_DIR}/Data")

# Add NoesisGUI D3D12 render device source files
set(NOESIS_D3D12_SOURCE_FILES
    ${NOESISGUI_ROOT_DIR}/Src/Packages/Render/D3D12RenderDevice/Src/D3D12RenderDevice.cpp
    ${NOESISGUI_ROOT_DIR}/Src/Packages/Render/D3D12RenderDevice/Src/Render.D3D12RenderDevice.cpp
)

# Add NoesisGUI Providers source files (FastLZ is needed by D3D12 render device)
# LocalFontProvider is needed for font loading
# LocalXamlProvider is needed for loading theme XAML files
# LocalTextureProvider and FileTextureProvider are needed for loading images
# Find.cpp provides platform-specific file system functions for LocalFontProvider
set(NOESIS_PROVIDERS_SOURCE_FILES
    ${NOESISGUI_ROOT_DIR}/Src/Packages/App/Providers/Src/FastLZ.cpp
    ${NOESISGUI_ROOT_DIR}/Src/Packages/App/Providers/Src/LocalFontProvider.cpp
    ${NOESISGUI_ROOT_DIR}/Src/Packages/App/Providers/Src/LocalXamlProvider.cpp
    ${NOESISGUI_ROOT_DIR}/Src/Packages/App/Providers/Src/FileTextureProvider.cpp
    ${NOESISGUI_ROOT_DIR}/Src/Packages/App/Providers/Src/LocalTextureProvider.cpp
    ${NOESISGUI_ROOT_DIR}/Src/Packages/App/Providers/Src/Find.cpp
    ${NOESISGUI_ROOT_DIR}/Src/Packages/App/Providers/Src/App.Providers.cpp
)

# Create imported targets for WickedEngine dependencies
add_library(Jolt STATIC IMPORTED)
set_target_properties(Jolt PROPERTIES
    IMPORTED_LOCATION_RELEASE "${WICKED_BUILD_DIR_RELEASE}/Jolt.lib"
    IMPORTED_LOCATION_DEBUG "${WICKED_BUILD_DIR_DEBUG}/Jolt.lib"
    IMPORTED_LOCATION "${WICKED_BUILD_DIR_RELEASE}/Jolt.lib"
)

add_library(LUA STATIC IMPORTED)
set_target_properties(LUA PROPERTIES
    IMPORTED_LOCATION_RELEASE "${WICKED_BUILD_DIR_RELEASE}/LUA/LUA.lib"
    IMPORTED_LOCATION_DEBUG "${WICKED_BUILD_DIR_DEBUG}/LUA/LUA.lib"
    IMPORTED_LOCATION "${WICKED_BUILD_DIR_RELEASE}/LUA/LUA.lib"
)

add_library(Utility STATIC IMPORTED)
set_target_properties(Utility PROPERTIES
    IMPORTED_LOCATION_RELEASE "${WICKED_BUILD_DIR_RELEASE}/Utility/Utility.lib"
    IMPORTED_LOCATION_DEBUG "${WICKED_BUILD_DIR_DEBUG}/Utility/Utility.lib"
    IMPORTED_LOCATION "${WICKED_BUILD_DIR_RELEASE}/Utility/Utility.lib"
)

# Create imported target for WickedEngine with configuration-specific libraries
add_library(WickedEngine STATIC IMPORTED)
set_target_properties(WickedEngine PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${WICKED_ENGINE_DIR}
    IMPORTED_LOCATION_RELEASE "${WICKED_BUILD_DIR_RELEASE}/WickedEngine.lib"
    IMPORTED_LOCATION_DEBUG "${WICKED_BUILD_DIR_DEBUG}/WickedEngine.lib"
    IMPORTED_LOCATION "${WICKED_BUILD_DIR_RELEASE}/WickedEngine.lib"  # Default to Release
    INTERFACE_LINK_LIBRARIES "Jolt;LUA;Utility"
)

# Create imported target for NoesisGUI
add_library(NoesisGUI SHARED IMPORTED)
set_target_properties(NoesisGUI PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${NOESISGUI_INCLUDE_DIR}
    IMPORTED_LOCATION "${NOESISGUI_BIN_DIR}/Noesis.dll"
    IMPORTED_IMPLIB "${NOESISGUI_LIB_DIR}/Noesis.lib"
)

set(LIB_DXCOMPILER "dxcompiler.dll")

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${RESOURCE_FILES} ${NOESIS_D3D12_SOURCE_FILES} ${NOESIS_PROVIDERS_SOURCE_FILES})

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${WICKED_ENGINE_DIR}
    ${NOESISGUI_INCLUDE_DIR}
    ${NOESISGUI_ROOT_DIR}/Src/Packages/Render/D3D12RenderDevice/Include
    ${NOESISGUI_ROOT_DIR}/Src/Packages/Render/D3D12RenderContext/Src
    ${NOESISGUI_ROOT_DIR}/Src/Packages/Render/D3D12RenderDevice/Src
    ${NOESISGUI_ROOT_DIR}/Src/Packages/Render/RenderContext/Include
    ${NOESISGUI_ROOT_DIR}/Src/Packages/App/Providers/Include
    ${NOESISGUI_ROOT_DIR}/Src/Packages/App/MediaElement/Include
)

# Link against WickedEngine library and its dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC
    WickedEngine
    Jolt
    LUA
    Utility
    NoesisGUI
    d3d12.lib
    dxgi.lib
)

if(MSVC)
    add_compile_options(/MP)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
WIN32_EXECUTABLE YES
)

if(WICKED_ENABLE_IPO)
set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION ON
    INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF
)
endif()

set(PLATFORM "Windows")
add_compile_definitions(WIN32 _WINDOWS WICKED_CMAKE_BUILD UNICODE _UNICODE)

# Define D3D12 render device API as static (not DLL import/export) since we're compiling it into the executable
# This overrides the default NS_DLL_IMPORT to make the functions available statically
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    NS_RENDER_D3D12RENDERDEVICE_API=
    NS_APP_PROVIDERS_API=
)

set(LIBDXCOMPILER_PATH "${WICKED_ENGINE_DIR}/${LIB_DXCOMPILER}")
set(STARTUP_LUA "${WICKED_ROOT_DIR}/Editor/startup.lua")
message(STATUS "WickedEngine directory: ${WICKED_ENGINE_DIR}")
message(STATUS "WickedEngine Release build directory: ${WICKED_BUILD_DIR_RELEASE}")
message(STATUS "WickedEngine Debug build directory: ${WICKED_BUILD_DIR_DEBUG}")

# Check if Debug library exists
if(NOT EXISTS "${WICKED_BUILD_DIR_DEBUG}/WickedEngine.lib")
    message(WARNING "WickedEngine Debug library not found at ${WICKED_BUILD_DIR_DEBUG}/WickedEngine.lib")
    message(WARNING "You need to build WickedEngine in Debug mode to match your Debug build configuration.")
    message(WARNING "Alternatively, build LOJ1897 in Release mode to match the existing Release WickedEngine library.")
endif()

message(STATUS "libdxcompiler found at ${LIBDXCOMPILER_PATH}")
message(STATUS "startup lua found at ${STARTUP_LUA}")
message(STATUS "NoesisGUI directory: ${NOESISGUI_ROOT_DIR}")
message(STATUS "NoesisGUI include: ${NOESISGUI_INCLUDE_DIR}")
message(STATUS "NoesisGUI library: ${NOESISGUI_LIB_DIR}/Noesis.lib")
message(STATUS "NoesisGUI DLL: ${NOESISGUI_BIN_DIR}/Noesis.dll")

# Copy required files
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIBDXCOMPILER_PATH} ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${STARTUP_LUA} ${CMAKE_CURRENT_BINARY_DIR}/startup.lua
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NOESISGUI_BIN_DIR}/Noesis.dll ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/GUI ${CMAKE_CURRENT_BINARY_DIR}/GUI
    COMMAND ${CMAKE_COMMAND} -E copy_directory "E:/Repos/WickedLOJ/WickedEngine/shaders" ${CMAKE_CURRENT_BINARY_DIR}/WickedEngine/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/Logo.png ${CMAKE_CURRENT_BINARY_DIR}/Logo.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/MainMenu.png ${CMAKE_CURRENT_BINARY_DIR}/GUI/MainMenu.png
)

# Copy lightsetup.wiscene if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lightsetup.wiscene)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/lightsetup.wiscene ${CMAKE_CURRENT_BINARY_DIR}/lightsetup.wiscene
    )
endif()

# Create Publish folder for Release builds only
# Add POST_BUILD command that conditionally creates Publish folder
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} 
        -DCMAKE_BUILD_TYPE=$<CONFIG>
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -DLIBDXCOMPILER_PATH=${LIBDXCOMPILER_PATH}
        -DNOESISGUI_BIN_DIR=${NOESISGUI_BIN_DIR}
        -DTARGET_FILE=$<TARGET_FILE:${PROJECT_NAME}>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CreatePublish.cmake
    COMMENT "Creating Publish folder for Release build"
)
