cmake_minimum_required(VERSION 3.4.1)

# file(GLOB SOURCE src/main/cpp/*.cpp)
file(GLOB SOURCE ${CMAKE_SOURCE_DIR}/*.cpp)
add_library( # Sets the name of the library.
        neplayer
        SHARED
        ${SOURCE})

find_library( # Sets the name of the path variable.
        log-lib
        log)

# include_directories(src/main/cpp/include)
include_directories(${CMAKE_SOURCE_DIR}/include)
set(my_lib_path ${CMAKE_SOURCE_DIR}/../../../libs/${CMAKE_ANDROID_ARCH_ABI})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${my_lib_path}")

# message(WARNING "cmake_source_dir = ${CMAKE_SOURCE_DIR}") #E:/AndroidWangYiCloud/NDKWorkspace/NeVideoDecodeSync/app/src/main/cpp

target_link_libraries( # Specifies the target library.
        neplayer
        # avcodec avfilter avformat avutil swresample swscale
        avfilter avformat avcodec avutil swresample swscale
        ${log-lib}
        android #系统库,在 D:\AndroidDev\AndroidStudio\sdk\ndk-bundle\platforms\android-21\arch-arm\usr\lib
        z
        OpenSLES)