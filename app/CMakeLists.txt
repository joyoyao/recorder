# Sets the minimum version of CMake required to build the native
# library. You should either keep the default value or only pass a
# value of 3.4.0 or lower.

cmake_minimum_required(VERSION 3.4.1)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds it for you.
# Gradle automatically packages shared libraries with your APK.


add_library(lib_ffmpeg SHARED IMPORTED)
set_target_properties(lib_ffmpeg PROPERTIES IMPORTED_LOCATION
    ${CMAKE_SOURCE_DIR}/src/main/cpp/FFmpegLibs/lib/${ANDROID_ABI}/libffmpeg.so)

add_library( # Sets the name of the library.
             recoder-lib

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             # Associated headers in the same location as their source
             # file are automatically included.
             src/main/cpp/recoder/utils/AString.cpp
             src/main/cpp/recoder/com_abcew_videorecoder_ARecorder.cpp
             src/main/cpp/recoder/utils/ColorConverter.cpp
             src/main/cpp/recoder/utils/Common.cpp
             src/main/cpp/recoder/utils/MediaBuffer.cpp
             src/main/cpp/recoder/utils/MediaBufferGroup.cpp
             src/main/cpp/recoder/utils/MetaData.cpp
             src/main/cpp/recoder/utils/TimedEventQueue.cpp
             src/main/cpp/recoder/FFmpegAudioEncoder.cpp
             src/main/cpp/recoder/ffmpegEncoder.cpp
             src/main/cpp/recoder/FFMPEGer.cpp
             src/main/cpp/recoder/LMRSource.cpp
             src/main/cpp/recoder/LocalMediaRecorder.cpp
             src/main/cpp/recoder/MessageQueue.cpp
             src/main/cpp/recoder/MuxEngine.cpp
             src/main/cpp/recoder/OpenSLRecorder.cpp
             src/main/cpp/recoder/Prefetcher.cpp
             )

# Searches for a specified prebuilt library and stores the path as a
# variable. Because system libraries are included in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

target_include_directories(recoder-lib PRIVATE
                           ${CMAKE_SOURCE_DIR}/src/main/cpp/FFmpegLibs/include
                           ${CMAKE_SOURCE_DIR}/src/main/cpp/FFmpegLibs/recoder
                          )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in the
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
                       recoder-lib
                       lib_ffmpeg
                       android
                       OpenSLES
                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib} )
