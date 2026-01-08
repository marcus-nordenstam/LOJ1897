# CreatePublish.cmake - Script to create Publish folder only for Release builds

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # For multi-config generators, check if we're in a Release configuration
    # This script is called with -DCMAKE_BUILD_TYPE=$<CONFIG> which will be "Release" for Release builds
    
    message(STATUS "Creating Publish folder...")
    
    # Create Publish directory
    file(MAKE_DIRECTORY ${BINARY_DIR}/LOJ1897)
    
    # Copy GUI folder
    file(COPY ${SOURCE_DIR}/GUI DESTINATION ${BINARY_DIR}/LOJ1897)
    
    # Copy Shaders folder
    file(COPY "E:/Repos/WickedLOJ/WickedEngine/shaders" DESTINATION ${BINARY_DIR}/LOJ1897/WickedEngine)
    
    # Copy individual files with correct names
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIBDXCOMPILER_PATH} ${BINARY_DIR}/LOJ1897/dxcompiler.dll
    )
    
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NOESISGUI_BIN_DIR}/Noesis.dll ${BINARY_DIR}/LOJ1897/Noesis.dll
    )
    
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_DIR}/Logo.png ${BINARY_DIR}/LOJ1897/Logo.png
    )
    
    # Copy MainMenu.png to GUI folder so it can be found by the XAML
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_DIR}/MainMenu.png ${BINARY_DIR}/LOJ1897/GUI/MainMenu.png
    )
    
    # Copy lightsetup.wiscene if it exists
    if(EXISTS ${SOURCE_DIR}/lightsetup.wiscene)
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SOURCE_DIR}/lightsetup.wiscene ${BINARY_DIR}/LOJ1897/lightsetup.wiscene
        )
    endif()
    
    # Copy executable with correct name
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TARGET_FILE} ${BINARY_DIR}/LOJ1897/LOJ1897.exe
    )
    
    message(STATUS "Publish folder created successfully")
else()
    message(STATUS "Skipping Publish folder creation (not a Release build: ${CMAKE_BUILD_TYPE})")
endif()
