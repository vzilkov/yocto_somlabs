cmake_minimum_required(VERSION 3.20)
project(button-led-proj)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_library(GPIOD_LIB NAMES gpiod)
find_library(GPIODCXX_LIB NAMES gpiodcxx)

if(NOT GPIOD_LIB OR NOT GPIODCXX_LIB)
    message(FATAL_ERROR "GPIO libraries not found")
endif()

# Исходные файлы
set(SOURCES
    button-led.cpp
)

# Исполняемый файл
add_executable(button-led ${SOURCES})

target_link_libraries(button-led PRIVATE ${GPIOD_LIB} ${GPIODCXX_LIB})

# Установка
install(TARGETS button-led
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
