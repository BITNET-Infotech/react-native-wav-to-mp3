cmake_minimum_required(VERSION 3.4.1)

# Add project name
project(wav_to_mp3)

# Set LAME library paths
set(LAME_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/include)
set(LAME_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})

# Add LAME library
add_library(lame SHARED IMPORTED)
set_target_properties(lame PROPERTIES IMPORTED_LOCATION
    ${LAME_LIB_DIR}/libmp3lame.so)

# Create wav-to-mp3 library
add_library(wav-to-mp3 SHARED
    wav_to_mp3.cpp)

# Include directories
target_include_directories(wav-to-mp3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LAME_INCLUDE_DIR})

# Link against required libraries
target_link_libraries(wav-to-mp3
    lame
    android
    log
    mediandk) 