SUMMARY = "Button-LED control application for VisionCB-6ULL"
DESCRIPTION = "C++ application that controls LED with button using sysfs"
HOMEPAGE = "https://github.com/vzilkov/yocto_somlabs"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# Версия рецепта
PR = "r0"

# Исходные файлы
SRC_URI = " \
    file://button-led.hpp \
    file://button-led.cpp \
    file://ethernet.hpp \
    file://ethernet.cpp \
    file://CMakeLists.txt \
    file://button-led.service \
"

# Папка с исходниками в рабочем каталоге
S = "${WORKDIR}/files"
UNPACKDIR = "${S}"

# Зависимости
DEPENDS = "libgpiod pkgconfig-native"

# Наследование класса cmake
inherit cmake systemd

# Системный сервис
SYSTEMD_SERVICE:${PN} = "button-led.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

# Настройки CMake
EXTRA_OECMAKE = ""

# Установка файлов
do_install:append() {
    # Установка systemd юнита
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/button-led.service ${D}${systemd_system_unitdir}
}

# Включение systemd поддержки
PACKAGECONFIG ??= "${@bb.utils.filter('DISTRO_FEATURES', 'systemd', d)}"
PACKAGECONFIG[systemd] = "-DSYSTEMD_SUPPORT=ON,,,systemd"

FILES:${PN} += " \
    ${bindir}/button-led \
    ${systemd_system_unitdir}/button-led.service \
"

# Зависимости пакета
RDEPENDS:${PN} = "libgpiod"
RDEPENDS:${PN} += "${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'systemd', '', d)}"