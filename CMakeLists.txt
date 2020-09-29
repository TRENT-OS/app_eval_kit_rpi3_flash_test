#
# Test Filesystem as lib
#
# Copyright (C) 2020, Hensoldt Cyber GmbH
#

cmake_minimum_required(VERSION 3.7.2)

os_set_config_file(system_config.h)

project(system_config C)
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR})

#-------------------------------------------------------------------------------
project(test_filesystem C)

DeclareCAmkESComponent(
    test_OS_Storage
    SOURCES
        components/Tests/src/test_OS_Storage.c
    INCLUDES
        ${OS_LIBS_DIR}
    C_FLAGS
        -Wall
        -Werror
    LIBS
        system_config
        os_core_api
        os_libs
        os_filesystem
)

DeclareCAmkESComponent_TimeServer(
    TimeServer
)

DeclareCAmkESComponent_RPI_SPI_Flash(
    RPI_SPI_Flash
    LIBS
        TimeServer_client
)

DeclareCAmkESComponent_StorageServer(
    StorageServer
)

DeclareAndCreateCamkESSystem(main.camkes)
GenerateSimulateScript()
