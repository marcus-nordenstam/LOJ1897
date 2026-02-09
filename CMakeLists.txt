cmake_minimum_required(VERSION 3.19)
project(LOJ1897)
set(CMAKE_CXX_STANDARD 23)

# Enable parallel builds by default
if (NOT DEFINED CMAKE_BUILD_PARALLEL_LEVEL)
    include(ProcessorCount)
    ProcessorCount(N)
    if (NOT N EQUAL 0)
        set(CMAKE_BUILD_PARALLEL_LEVEL ${N} CACHE STRING "Number of parallel build jobs")
        message(STATUS "Setting parallel build level to ${N} (number of processors)")
    endif ()
endif ()

# Add to CMakeLists.txt
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
endif ()

# Source files
set(SOURCE_FILES
        main.cc
        NoesisRenderPath.h
        NoesisRenderPath.cc
        DialogueMode.h
        DialogueMode.cc
        CaseboardMode.h
        CaseboardMode.cc
        PhotoMode.h
        PhotoMode.cc
        GameStartup.h
        GameStartup.cc
        MerlinLua.h
        MerlinLua.cc
)

# Resource files
set(RESOURCE_FILES
        app.rc
)

# Set GrymEngine paths
set(GRYM_ROOT_DIR "E:/Repos/GrymEngine" CACHE PATH "Path to GrymEngine repository root")
set(GRYM_ENGINE_DIR "${GRYM_ROOT_DIR}/Engine")
set(GRYM_BUILD_DIR_RELEASE "${GRYM_ROOT_DIR}/cmake-build-release-visual-studio/Engine" CACHE PATH "Path to GrymEngine Release build directory")
set(GRYM_BUILD_DIR_DEBUG "${GRYM_ROOT_DIR}/cmake-build-debug-visual-studio/Engine" CACHE PATH "Path to GrymEngine Debug build directory")
set(GRYM_SHADERS_DIR "${GRYM_ROOT_DIR}/cmake-build-release-visual-studio/Editor/shaders" CACHE PATH "Path to pre-compiled shaders")

# Set LuaJIT and Merlin paths
set(LUAJIT_ROOT "E:/Repos/luajit/src" CACHE PATH "Path to LuaJIT source/build")
set(MERLIN_ROOT "E:/Repos/Merlin" CACHE PATH "Path to Merlin repository root")
set(MERLIN_SOURCE_DIR "${MERLIN_ROOT}/Src/Lib")
set(MERLIN_BUILD_DIR_RELEASE "${MERLIN_ROOT}/cmake-build-release-visual-studio" CACHE PATH "Path to Merlin Release build output")
set(MERLIN_BUILD_DIR_DEBUG "${MERLIN_ROOT}/cmake-build-win-debug" CACHE PATH "Path to Merlin Debug build output")

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

# Create imported targets for GrymEngine dependencies
add_library(Jolt STATIC IMPORTED)
set_target_properties(Jolt PROPERTIES
        IMPORTED_LOCATION_RELEASE "${GRYM_BUILD_DIR_RELEASE}/Jolt.lib"
        IMPORTED_LOCATION_DEBUG "${GRYM_BUILD_DIR_DEBUG}/Jolt.lib"
        IMPORTED_LOCATION "${GRYM_BUILD_DIR_RELEASE}/Jolt.lib"
)

# LuaJIT for Merlin integration
add_library(LuaJIT STATIC IMPORTED)
set_target_properties(LuaJIT PROPERTIES
        IMPORTED_LOCATION "${LUAJIT_ROOT}/lua51.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${LUAJIT_ROOT}"
)

add_library(Utility STATIC IMPORTED)
set_target_properties(Utility PROPERTIES
        IMPORTED_LOCATION_RELEASE "${GRYM_BUILD_DIR_RELEASE}/Utility/Utility.lib"
        IMPORTED_LOCATION_DEBUG "${GRYM_BUILD_DIR_DEBUG}/Utility/Utility.lib"
        IMPORTED_LOCATION "${GRYM_BUILD_DIR_RELEASE}/Utility/Utility.lib"
)

# Create imported target for GrymEngine with configuration-specific libraries
add_library(GrymEngine STATIC IMPORTED)
set_target_properties(GrymEngine PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${GRYM_ENGINE_DIR}
        IMPORTED_LOCATION_RELEASE "${GRYM_BUILD_DIR_RELEASE}/GrymEngine_Windows.lib"
        IMPORTED_LOCATION_DEBUG "${GRYM_BUILD_DIR_DEBUG}/GrymEngine_Windows.lib"
        IMPORTED_LOCATION "${GRYM_BUILD_DIR_RELEASE}/GrymEngine_Windows.lib"  # Default to Release
        INTERFACE_LINK_LIBRARIES "Jolt;Utility"
)

# Create imported target for Merlin (source browsing + debugging)
# Merlin is loaded at runtime via LuaJIT FFI, not linked directly,
# but we import it so the IDE can browse its source under External Libraries.
# CInterface functions are loaded dynamically via GetProcAddress in MerlinLua.
add_library(MerlinLib INTERFACE IMPORTED)
set_target_properties(MerlinLib PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${MERLIN_SOURCE_DIR}"
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
        ${GRYM_ENGINE_DIR}
        ${GRYM_ROOT_DIR}
        ${NOESISGUI_INCLUDE_DIR}
        ${NOESISGUI_ROOT_DIR}/Src/Packages/Render/D3D12RenderDevice/Include
        ${NOESISGUI_ROOT_DIR}/Src/Packages/Render/D3D12RenderContext/Src
        ${NOESISGUI_ROOT_DIR}/Src/Packages/Render/D3D12RenderDevice/Src
        ${NOESISGUI_ROOT_DIR}/Src/Packages/Render/RenderContext/Include
        ${NOESISGUI_ROOT_DIR}/Src/Packages/App/Providers/Include
        ${NOESISGUI_ROOT_DIR}/Src/Packages/App/MediaElement/Include
)

# Link against GrymEngine library and its dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC
        GrymEngine
        Jolt
        Utility
        LuaJIT
        MerlinLib
        NoesisGUI
        d3d12.lib
        dxgi.lib
)

if (MSVC)
    # Enable multi-processor compilation with maximum available processors
    add_compile_options(/MP)

    # Enable parallel build at project level
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    # Set Visual Studio to build multiple projects in parallel
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    # Enable faster PDB generation for parallel builds
    add_compile_options(/FS)

    # Use faster linker
    add_link_options(/DEBUG:FASTLINK)

    # Enable /bigobj for large object files (needed for heavy template usage from GrymEngine, NoesisGUI, etc.)
    # Use target_compile_options to apply to the already-created target
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif ()

set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE YES
)

if (GRYM_ENABLE_IPO)
    set_target_properties(${PROJECT_NAME} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION ON
            INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF
    )
endif ()

set(PLATFORM "Windows")
add_compile_definitions(WIN32 _WINDOWS GRYM_CMAKE_BUILD UNICODE _UNICODE)

# Define D3D12 render device API as static (not DLL import/export) since we're compiling it into the executable
# This overrides the default NS_DLL_IMPORT to make the functions available statically
target_compile_definitions(${PROJECT_NAME} PRIVATE
        NS_RENDER_D3D12RENDERDEVICE_API=
        NS_APP_PROVIDERS_API=
)

set(LIBDXCOMPILER_PATH "${GRYM_ENGINE_DIR}/${LIB_DXCOMPILER}")
set(STARTUP_LUA "${GRYM_ROOT_DIR}/Editor/startup.lua")
message(STATUS "GrymEngine directory: ${GRYM_ENGINE_DIR}")
message(STATUS "GrymEngine Release build directory: ${GRYM_BUILD_DIR_RELEASE}")
message(STATUS "GrymEngine Debug build directory: ${GRYM_BUILD_DIR_DEBUG}")

# Check if Debug library exists
if (NOT EXISTS "${GRYM_BUILD_DIR_DEBUG}/GrymEngine_Windows.lib")
    message(WARNING "GrymEngine Debug library not found at ${GRYM_BUILD_DIR_DEBUG}/GrymEngine_Windows.lib")
    message(WARNING "You need to build GrymEngine in Debug mode to match your Debug build configuration.")
    message(WARNING "Alternatively, build LOJ1897 in Release mode to match the existing Release GrymEngine library.")
endif ()

message(STATUS "libdxcompiler found at ${LIBDXCOMPILER_PATH}")
message(STATUS "startup lua found at ${STARTUP_LUA}")
message(STATUS "Pre-compiled shaders: ${GRYM_SHADERS_DIR}")
message(STATUS "NoesisGUI directory: ${NOESISGUI_ROOT_DIR}")
message(STATUS "NoesisGUI include: ${NOESISGUI_INCLUDE_DIR}")
message(STATUS "NoesisGUI library: ${NOESISGUI_LIB_DIR}/Noesis.lib")
message(STATUS "NoesisGUI DLL: ${NOESISGUI_BIN_DIR}/Noesis.dll")

# Create custom command for copying Merlin DLL that depends on the DLL files
# This ensures the copy runs whenever the DLL changes, even without rebuilding the project
set(MERLIN_DLL_DEBUG_PATH "${MERLIN_BUILD_DIR_DEBUG}/Src/Lib/Merlin.dll")
set(MERLIN_DLL_RELEASE_PATH "${MERLIN_BUILD_DIR_RELEASE}/Src/Lib/Merlin.dll")

# Build list of DLL dependencies (only include files that exist)
set(MERLIN_DLL_DEPENDENCIES ${CMAKE_CURRENT_SOURCE_DIR}/CopyMerlinDll.cmake)
if(EXISTS "${MERLIN_DLL_DEBUG_PATH}")
    list(APPEND MERLIN_DLL_DEPENDENCIES "${MERLIN_DLL_DEBUG_PATH}")
endif()
if(EXISTS "${MERLIN_DLL_RELEASE_PATH}")
    list(APPEND MERLIN_DLL_DEPENDENCIES "${MERLIN_DLL_RELEASE_PATH}")
endif()

# Create a custom target that depends on the DLL files
# This target will be "built" (i.e., its command will run) whenever the DLLs change
# ALL ensures it's included in the default build, so it runs every time
add_custom_target(CopyMerlinDllTarget ALL
        DEPENDS ${MERLIN_DLL_DEPENDENCIES}
)

# Add commands to the custom target that copy the DLL and Merlin folder
# The DLL copy uses copy_if_different, so it only copies if the DLL has actually changed
# The Merlin folder is copied every time (regardless of changes)
add_custom_command(
        TARGET CopyMerlinDllTarget POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DDEBUG_DIR="${MERLIN_BUILD_DIR_DEBUG}/Src/Lib" -DRELEASE_DIR="${MERLIN_BUILD_DIR_RELEASE}/Src/Lib" -DOUTPUT_DIR="$<TARGET_FILE_DIR:${PROJECT_NAME}>" -DCONFIG=$<CONFIG> -P ${CMAKE_CURRENT_SOURCE_DIR}/CopyMerlinDll.cmake
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Merlin "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Merlin"
        COMMENT "Copying Merlin.dll and Merlin folder"
)

# Make the executable depend on the CopyMerlinDllTarget
# This ensures the DLL is copied before the executable is built/run
add_dependencies(${PROJECT_NAME} CopyMerlinDllTarget)

# Copy required files
add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIBDXCOMPILER_PATH} ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${STARTUP_LUA} ${CMAKE_CURRENT_BINARY_DIR}/startup.lua
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NOESISGUI_BIN_DIR}/Noesis.dll ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/GUI ${CMAKE_CURRENT_BINARY_DIR}/GUI
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${GRYM_SHADERS_DIR} ${CMAKE_CURRENT_BINARY_DIR}/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/Logo.png ${CMAKE_CURRENT_BINARY_DIR}/Logo.png
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/MainMenu.png ${CMAKE_CURRENT_BINARY_DIR}/GUI/MainMenu.png
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUAJIT_ROOT}/lua51.dll ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Lua/jit ${CMAKE_CURRENT_BINARY_DIR}/lua/jit
        COMMENT "Copying required runtime files to build directory"
)

# Copy config.ini if it exists
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/config.ini)
    add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/config.ini ${CMAKE_CURRENT_BINARY_DIR}/config.ini
            COMMENT "Copying config.ini to build directory"
    )
endif ()

# Copy lightsetup.wiscene if it exists
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lightsetup.wiscene)
    add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/lightsetup.wiscene ${CMAKE_CURRENT_BINARY_DIR}/lightsetup.wiscene
    )
endif ()

# Create Publish folder for Publish build configuration only
# Add POST_BUILD command that conditionally creates Publish folder
add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        -DCMAKE_BUILD_TYPE=$<CONFIG>
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -DLIBDXCOMPILER_PATH=${LIBDXCOMPILER_PATH}
        -DNOESISGUI_BIN_DIR=${NOESISGUI_BIN_DIR}
        -DGRYM_SHADERS_DIR=${GRYM_SHADERS_DIR}
        -DTARGET_FILE=$<TARGET_FILE:${PROJECT_NAME}>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CreatePublish.cmake
        COMMENT "Creating Publish folder if build configuration is Publish"
)
