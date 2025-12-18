cmake_minimum_required(VERSION 3.20)
project(test-proj)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск libgpiod
# find_package(PkgConfig REQUIRED)
# pkg_check_modules(GPIOD REQUIRED libgpiod libgpiodcxx)

find_library(GPIOD_LIB NAMES gpiod)
find_library(GPIODCXX_LIB NAMES gpiodcxx)

if(NOT GPIOD_LIB OR NOT GPIODCXX_LIB)
    message(FATAL_ERROR "GPIO libraries not found")
endif()

add_library(eth_lib ethernet.cpp ethernet.hpp)

# Исходные файлы
set(SOURCES
    button-led.cpp
)
set(HEADERS
    button-led.hpp
)
# Исполняемый файл
add_executable(button-led ${HEADERS} ${SOURCES})

target_link_libraries(button-led PRIVATE pthread eth_lib)

# Установка
install(TARGETS button-led
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
