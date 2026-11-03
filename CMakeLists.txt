cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME stm32f407vg-scientific-calculator)

set(ILI9341_RS_ADDRESS_LINE "18" CACHE STRING
    "FSMC address line connected to the ILI9341 register-select input")
set_property(CACHE ILI9341_RS_ADDRESS_LINE PROPERTY STRINGS 16 18)
option(DISPLAY_BUS_DIAGNOSTIC
       "Build a full-screen RGB display-bus diagnostic image" OFF)

if(NOT ILI9341_RS_ADDRESS_LINE MATCHES "^(16|18)$")
    message(FATAL_ERROR "ILI9341_RS_ADDRESS_LINE must be 16 or 18")
endif()

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/calculator_engine.c
    Core/Src/display_app.c
    Core/Src/display_text.c
    Core/Src/ili9341.c
    Core/Src/touch_controller.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    ILI9341_RS_ADDRESS_LINE=${ILI9341_RS_ADDRESS_LINE}
)

if(DISPLAY_BUS_DIAGNOSTIC)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        DISPLAY_BUS_DIAGNOSTIC=1
    )
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
        OUTPUT_NAME
        "stm32f407vg-scientific-calculator-diagnostic-a${ILI9341_RS_ADDRESS_LINE}"
    )
endif()

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    m

    # Add user defined libraries
)
