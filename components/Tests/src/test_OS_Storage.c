/*
 * Copyright (C) 2020, Hensoldt Cyber GmbH
 */

#include <camkes.h>
#include "LibDebug/Debug.h"
#include "OS_Dataport.h"
#include <math.h>
#include <string.h>
#include <stdbool.h>

static OS_Dataport_t port_storage  = OS_DATAPORT_ASSIGN(storage_port);

//------------------------------------------------------------------------------
static OS_Error_t
__attribute__((unused))
read_validate(
    off_t addr,
    size_t sz,
    uint8_t *buf,
    const uint8_t *buf_ref)
{
    size_t bytes_read = 0;
    OS_Error_t ret = storage_rpc_read(addr, sz, &bytes_read);
    if ((OS_SUCCESS != ret) || (bytes_read != sz))
    {
        Debug_LOG_ERROR(
            "storage_rpc_read failed, addr=0x%jx, sz=0x%x, read=0x%x, code %d",
            addr, sz, bytes_read, ret);
        return OS_ERROR_GENERIC;
    }

    const void* data = OS_Dataport_getBuf(port_storage);
    int serr = memcmp(data, buf_ref, sz);
    if (0 != serr)
    {
        return OS_ERROR_ABORTED;
    }

    return OS_SUCCESS;
}


//------------------------------------------------------------------------------
static OS_Error_t
__attribute__((unused))
test_flash_block(
    const off_t addr,
    void* buf,
    const void* buf_ref_empty,
    const void* buf_ref_pattern)
{
    OS_Error_t ret;

    off_t bytes_erased = 0;
    ret = storage_rpc_erase(addr, BLOCK_SZ, &bytes_erased);

    if ((ret != OS_SUCCESS) || (bytes_erased != BLOCK_SZ))
    {
        Debug_LOG_ERROR("erase failed len_ret=%jx, code %d", bytes_erased, ret);
        return OS_ERROR_ABORTED;
    }

    ret = read_validate(addr, BLOCK_SZ, buf, buf_ref_empty);
    if (OS_SUCCESS != ret)
    {
        Debug_LOG_ERROR("erase 0xFF validation, code %d", ret);
        return OS_ERROR_ABORTED;
    }

    for (unsigned int i = 0; i<(BLOCK_SZ/PAGE_SZ); i++)
    {
        memcpy(OS_Dataport_getBuf(port_storage), buf_ref_pattern, PAGE_SZ);
        off_t write_addr = addr + i*PAGE_SZ;
        size_t bytes_written = 0;
        ret = storage_rpc_write(write_addr, PAGE_SZ, &bytes_written);
        if ((OS_SUCCESS != ret) || (bytes_written != PAGE_SZ))
        {
            Debug_LOG_ERROR(
                "storage_rpc_write failed, addr=0x%jx, sz=0x%x, read=0x%x, code %d",
                write_addr, PAGE_SZ, bytes_written, ret);
        }
    }

    ret = read_validate(addr, BLOCK_SZ, buf, buf_ref_pattern);
    if (OS_SUCCESS != ret)
    {
        Debug_LOG_ERROR("pattern validation failed, code %d", ret);
        return OS_ERROR_ABORTED;
    }

    return OS_SUCCESS;
}


//------------------------------------------------------------------------------
static bool
__attribute__((unused))
test_OS_BlockAccess(void)
{

    static uint8_t buf[BLOCK_SZ];

    static uint8_t buf_ref_empty[BLOCK_SZ];
    memset(buf_ref_empty, 0xff, sizeof(buf_ref_empty));

    static uint8_t buf_ref_pattern_block_0[BLOCK_SZ];
    memset(buf_ref_pattern_block_0, 0x5a, sizeof(buf_ref_pattern_block_0));

    static uint8_t buf_ref_pattern[BLOCK_SZ];
    memset(buf_ref_pattern, 0xa5, sizeof(buf_ref_pattern));

    OS_Error_t ret;
    off_t sz;
    if ((ret = storage_rpc_getSize(&sz)) != OS_SUCCESS)
    {
        Debug_LOG_ERROR("storage_rpc_getSize() failed, code %d", ret);
        return false;
    }

    //test/detect memory size
    Debug_LOG_INFO("Detecting available memory size...");
    test_flash_block(0, buf, buf_ref_empty, buf_ref_pattern_block_0);
    off_t addr = 0;

    //note: flash memory always comes in powers of 2
    //note: in our case the flash memories have 8 MiB of storage
    for (unsigned int i = 0; i <= log2(FLASH_SZ/BLOCK_SZ); i++)
    {
        addr = BLOCK_SZ << i;

        Debug_LOG_INFO(
            "Testing memory size: %lld Bytes (%lld MiB).", 
            addr,addr/1024/1024);

        ret = test_flash_block(
                addr,
                buf,
                buf_ref_empty,
                buf_ref_pattern);
        if (OS_SUCCESS != ret)
        {
            Debug_LOG_ERROR(
                "test_flash_block 0 failed at addr 0x%jx, code %d",
                addr, ret);
            break;
        }

        ret = read_validate(0, BLOCK_SZ, buf, buf_ref_pattern_block_0);
        if (OS_SUCCESS != ret)
        {
            Debug_LOG_ERROR(
                "read_validate block 0 pattern failed at addr 0x%jx, code %d",
                addr, ret);
            break;
        }

    }

    Debug_LOG_INFO(
        "Detected memory size: %lld Bytes (%lld MiB) => %s\n",
        addr,addr/1024/1024,addr==FLASH_SZ ? "FLASH SIZE OK!" : "FLASH SIZE WRONG!");

    if(addr != FLASH_SZ) return false; 

    //test every single memory cell
    Debug_LOG_INFO("Testing every memory block...");
    const size_t start_addr = 0x0;
    const size_t end_addr = FLASH_SZ;
    const size_t print_delta = 50;
    for(addr = start_addr; addr < end_addr; addr += BLOCK_SZ)
    {
        //pattern test
        if ((addr/BLOCK_SZ) % print_delta == 0)
        {
            Debug_LOG_INFO(
                "Testing block %lld of %d", 
                addr/BLOCK_SZ, FLASH_SZ/BLOCK_SZ);
        }

        ret = test_flash_block(addr, buf, buf_ref_empty, buf_ref_pattern);
        if (OS_SUCCESS != ret)
        {
            Debug_LOG_ERROR(
                "test_flash_block %lld failed at addr 0x%jx, code %d",
                addr/BLOCK_SZ,addr, ret);
            break;
        }
        
        //block erase 
        off_t bytes_erased = 0;
        ret = storage_rpc_erase(addr, BLOCK_SZ, &bytes_erased);
        if ((ret != OS_SUCCESS) || (bytes_erased != BLOCK_SZ))
        {
            Debug_LOG_ERROR(
                "erase failed for block %lld (0x0%jx), code %d",
                addr/BLOCK_SZ,addr,ret);
            return false;
        }
    }
    Debug_LOG_INFO(
        "Functioning flash up to block %lld (0x0%jx) => %s\n",
        addr/BLOCK_SZ,addr,addr==FLASH_SZ ? "All blocks working" : "Defect blocks");

    return addr == FLASH_SZ;
}

// Public Functions ------------------------------------------------------------

int run()
{
    Debug_LOG_INFO("Starting NOR Flash test.");
    Debug_LOG_INFO("Expected flash size: %d Bytes (%d MiB)",FLASH_SZ,FLASH_SZ/1024/1024);
    bool ret = test_OS_BlockAccess();
    Debug_LOG_INFO("%s\n",ret ? "FLASH OK!" : "FLASH DEFECT!");
    Debug_LOG_INFO("All tests done!");
    return 0;
}
