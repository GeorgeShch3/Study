cmake_minimum_required(VERSION 3.16)
project(lidar_sim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenMP REQUIRED)

add_compile_options(-O2 -Wall -Wextra)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

function(add_lidar_app target src)
    add_executable(${target} ${src})
    target_link_libraries(${target} PRIVATE OpenMP::OpenMP_CXX)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

add_lidar_app(corner_scan corner_scan.cpp)

add_lidar_app(adaptive_scan adaptive_scan.cpp)

set(TINYOBJ_H "${CMAKE_CURRENT_SOURCE_DIR}/tiny_obj_loader.h")
if(NOT EXISTS "${TINYOBJ_H}")
    message(STATUS "Скачиваем tiny_obj_loader.h ...")
    file(DOWNLOAD
        "https://github.com/tinyobjloader/tinyobjloader/releases/latest/download/tiny_obj_loader.h"
        "${TINYOBJ_H}"
        STATUS dl_status)
    list(GET dl_status 0 dl_code)
    if(NOT dl_code EQUAL 0)
        message(WARNING "Не удалось скачать tiny_obj_loader.h — obj_scan пропущен")
        set(TINYOBJ_H "")
    endif()
endif()

if(EXISTS "${TINYOBJ_H}")
    add_lidar_app(obj_scan obj_scan.cpp)
    target_compile_definitions(obj_scan PRIVATE TINYOBJLOADER_IMPLEMENTATION)
else()
    message(STATUS "obj_scan: положи tiny_obj_loader.h рядом и пересобери")
endif()

find_package(Eigen3 QUIET)
set(SMALL_GICP_DIR "" CACHE PATH "Путь к small_gicp")

if(Eigen3_FOUND AND EXISTS "${SMALL_GICP_DIR}/include/small_gicp/registration/registration_helper.hpp")
    add_lidar_app(robot_scan robot_scan.cpp)
    target_include_directories(robot_scan PRIVATE
        ${EIGEN3_INCLUDE_DIR}
        ${SMALL_GICP_DIR}/include)
    target_sources(robot_scan PRIVATE
        ${SMALL_GICP_DIR}/src/small_gicp/registration/registration_helper.cpp
        ${SMALL_GICP_DIR}/src/small_gicp/registration/registration.cpp)
    message(STATUS "robot_scan: сборка с small_gicp + Eigen3")
else()
    message(STATUS "robot_scan: пропуск (укажи -DSMALL_GICP_DIR=...)")
endif()