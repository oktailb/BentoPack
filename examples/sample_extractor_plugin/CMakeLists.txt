cmake_minimum_required(VERSION 3.20)
project(sample_custom_extractor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(BentoPack REQUIRED)

add_library(sample_custom_extractor MODULE
    sample_extractor.cpp
    sample_extractor.h
)

target_link_libraries(sample_custom_extractor PRIVATE
    BentoPack::BentoPackCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
