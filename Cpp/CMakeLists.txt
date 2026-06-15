cmake_minimum_required(VERSION 3.26)
project(FlipFilter_Cpp)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -funroll-loops -ffast-math -DNDEBUG")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")

add_executable(FlipFilter_Cpp main.cpp
        MurmurHash3.cpp
        header/bitmap.h
        header/Couper.h
        Couper.cpp
        CouponFilter.cpp
        FlipFilter.cpp
        LogLogFilter_Spread.cpp
        KjSkt.cpp
        rSkt.cpp
        SuperKjSkt.cpp
        vHLL.cpp
        FreeRS.cpp
        header/KjSkt.h)
