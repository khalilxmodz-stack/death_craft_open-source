cmake_minimum_required(VERSION 3.22.1)
project(deathcraft LANGUAGES C CXX)

add_library(android_native_app_glue STATIC
    "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c"
)
target_include_directories(android_native_app_glue PUBLIC
    "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue"
)

add_library(deathcraft SHARED main.cpp)
target_compile_features(deathcraft PRIVATE cxx_std_20)
target_compile_options(deathcraft PRIVATE -O2 -Wall -Wextra)
target_link_libraries(deathcraft
    android_native_app_glue
    android
    log
    EGL
    GLESv3
)
