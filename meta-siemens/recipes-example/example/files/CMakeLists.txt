cmake_minimum_required(VERSION 3.20)
project(button-led)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск libgpiod
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod)

# Исходные файлы
set(SOURCES
    button-led.cpp
)

# Исполняемый файл
add_executable(button-led ${SOURCES})
target_link_libraries(button-led PkgConfig::GPIOD)

# Установка
install(TARGETS button-led
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Установка systemd сервиса (опционально)
# if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/button-led.service")
#     install(FILES button-led.service
#         DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/systemd/system
#     )
# endif()