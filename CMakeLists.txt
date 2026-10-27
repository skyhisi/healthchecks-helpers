cmake_minimum_required(VERSION 3.16)
project(healthcheck-helpers VERSION 1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)
find_package(CURL REQUIRED)

add_executable(
    healthchecks-systemd-oneshot
    healthchecks-systemd-oneshot.c
)

target_compile_options(
    healthchecks-systemd-oneshot PRIVATE
    -W -Wall
)

set_target_properties(
    healthchecks-systemd-oneshot PROPERTIES
    C_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

target_compile_definitions(healthchecks-systemd-oneshot PRIVATE
    _GNU_SOURCE
)

target_link_libraries(healthchecks-systemd-oneshot PRIVATE
    CURL::libcurl
    PkgConfig::SYSTEMD
)

include(GNUInstallDirs)
install(TARGETS healthchecks-systemd-oneshot
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES docs/healthchecks-systemd-oneshot.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
)
