cmake_minimum_required(VERSION 3.26)
project(FlipFilter)

set(CMAKE_CXX_STANDARD 17)

add_executable(FlipFilter main.cpp
        MurmurHash3.cpp
        header/bitmap.h
        header/Couper.h
        Couper.cpp
        CouponFilter.cpp
        LogLogFilter_Spread.cpp
        KjSkt.cpp
        rSkt.cpp
        SuperKjSkt.cpp
        vHLL.cpp
        FreeRS.cpp
        header/HLLSampler.h
        HLLSampler.cpp
        header/KjSkt.h
        FlipFilter.cpp)
