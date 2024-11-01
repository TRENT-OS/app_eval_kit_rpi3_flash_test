#
# Test Filesystem as lib
#
# Copyright (C) 2020-2024, HENSOLDT Cyber GmbH
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# For commercial licensing, contact: info.cyber@hensoldt.net
#

cmake_minimum_required(VERSION 3.7.2)

os_set_config_file(system_config.h)

project(system_config C)
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR})

#-------------------------------------------------------------------------------
project(test_filesystem C)

set(LibUtilsDefaultZfLogLevel 5 CACHE STRING "" FORCE)

DeclareCAmkESComponent(
    test_OS_Storage
    SOURCES
        components/Tests/src/test_OS_Storage.c
    C_FLAGS
        -Wall
        -Werror
    LIBS
        system_config
        os_core_api
        lib_debug
        os_filesystem
)

TimeServer_DeclareCAmkESComponent(
    TimeServer
)

RPi_SPI_Flash_DeclareCAmkESComponent(
    RPi_SPI_Flash
    LIBS
        TimeServer_client
)

StorageServer_DeclareCAmkESComponent(
    StorageServer
)

DeclareAndCreateCamkESSystem(main.camkes)
GenerateSimulateScript()
