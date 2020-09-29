/*
 * Copyright (C) 2020, Hensoldt Cyber GmbH
 */

#include <camkes.h>
#include "lib_debug/Debug.h"
#include "OS_Dataport.h"
#include <math.h>
#include <string.h>
#include <stdbool.h>

static OS_Dataport_t port_storage  = OS_DATAPORT_ASSIGN(storage_port);

//------------------------------------------------------------------------------
/**
 * This function checks if the expected value (buf_ref) is available in the
 * specified memory block (addr). Thus, this helper function checks if each
 * write/read/erase access is working as expected.
 */
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
    const int serr = memcmp(data, buf_ref, sz);
    if (0 != serr)
    {
        return OS_ERROR_ABORTED;
    }

    return OS_SUCCESS;
}


//------------------------------------------------------------------------------
/**
 * This function first erases the specified memory block (addr) and verifies
 * that the erase operation succeeded, by comparing the resulting content in
 * the memory block with buf_ref_empty.
 * When erasing succeeded, the next step is to actually write buf_ref_pattern
 * to the memory block and check if the write access succeeded.
 */
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

    ret = storage_rpc_erase(addr, FLASH_BLOCK_SZ, &bytes_erased);

    if ((ret != OS_SUCCESS) || (bytes_erased != FLASH_BLOCK_SZ))
    {
        Debug_LOG_ERROR("erase failed len_ret=%jx, code %d", bytes_erased, ret);
        return OS_ERROR_ABORTED;
    }

    ret = read_validate(addr, FLASH_BLOCK_SZ, buf, buf_ref_empty);
    if (OS_SUCCESS != ret)
    {
        Debug_LOG_ERROR("erase 0xFF validation, code %d", ret);
        return OS_ERROR_ABORTED;
    }

    for (unsigned int i = 0; i<(FLASH_BLOCK_SZ/FLASH_PAGE_SZ); i++)
    {
        memcpy(OS_Dataport_getBuf(port_storage), buf_ref_pattern, FLASH_PAGE_SZ);
        off_t write_addr = addr + i*FLASH_PAGE_SZ;
        size_t bytes_written = 0;
        ret = storage_rpc_write(write_addr, FLASH_PAGE_SZ, &bytes_written);
        if ((OS_SUCCESS != ret) || (bytes_written != FLASH_PAGE_SZ))
        {
            Debug_LOG_ERROR(
                "storage_rpc_write failed, addr=0x%jx, sz=0x%x, read=0x%x, code %d",
                write_addr, FLASH_PAGE_SZ, bytes_written, ret);
        }
    }

    ret = read_validate(addr, FLASH_BLOCK_SZ, buf, buf_ref_pattern);
    if (OS_SUCCESS != ret)
    {
        Debug_LOG_ERROR("pattern validation failed, code %d", ret);
        return OS_ERROR_ABORTED;
    }

    return OS_SUCCESS;
}

//------------------------------------------------------------------------------
/**
 * Memory size detection
 *
 * Assumption 1: Flash memory always comes in powers of 2.
 * Assumption 2: In our case the flash memories have 8 MiB of storage.
 * Assumption 3: In case we write to an address that is bigger than the
 *               actual memory size, we start from the first memory block
 *               again and overwrite it (wrap-around).
 *
 * Our memory size detection starts from 4kiB and is doubled every iteration
 * (left-shift by i). Every loop iteration we write our expected memory
 * pattern (buf_ref_pattern) to the first block after the memory size we
 * test for in the current loop iteration. In case we write to an address that
 * is bigger than the actual memory size, we overwrite the first memory block
 * with our pattern (buf_ref_pattern). As we have set the first memory block of
 * the flash to a different pattern (buf_ref_pattern_block_0), we can detect
 * that the memory actually wrapped around and thus know that the memory size we
 * were testing for is the actual memory size. In case this memory size is
 * different from the memory size we expect (8MiB), we abort the flash test.
 */
static OS_Error_t
__attribute__((unused))
test_memory_size(void)
{
    Debug_LOG_INFO("Detecting available memory size...");

    //--------------------------------------------------------------------------
    // Test setup
    static uint8_t buf[FLASH_BLOCK_SZ];

    // pattern expected after erasing a memory block
    static uint8_t buf_ref_empty[FLASH_BLOCK_SZ];
    memset(buf_ref_empty, 0xff, sizeof(buf_ref_empty));

    // pattern expected in first memory block of flash
    static uint8_t buf_ref_pattern_block_0[FLASH_BLOCK_SZ];
    memset(buf_ref_pattern_block_0, 0x5a, sizeof(buf_ref_pattern_block_0));

    // reference pattern
    static uint8_t buf_ref_pattern[FLASH_BLOCK_SZ];
    memset(buf_ref_pattern, 0xa5, sizeof(buf_ref_pattern));
    //--------------------------------------------------------------------------

    OS_Error_t ret;
    off_t addr = 0;
    // Make sure that the first memory block actually holds the memory pattern
    // we expect for the first memory block, thus buf_ref_pattern_block_0.
    test_flash_block(0, buf, buf_ref_empty, buf_ref_pattern_block_0);

    for (unsigned int i = 0; i <= log2(FLASH_SZ/FLASH_BLOCK_SZ); i++)
    {
        addr = FLASH_BLOCK_SZ << i;

        Debug_LOG_INFO(
            "Testing memory size: %lld Bytes (%lld MiB).",
            addr,addr/1024/1024);

        // write expected memory pattern to first block after the memory size
        // we test for
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

        // check if first block still holds the memory pattern
        // buf_ref_pattern_block_0
        ret = read_validate(0, FLASH_BLOCK_SZ, buf, buf_ref_pattern_block_0);
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

    // if the address we last wrote to (addr) differs from the expected flash
    // memory size (FLASH_SZ), the flash test is aborted
    if(addr != FLASH_SZ)
    {
        return OS_ERROR_ABORTED;
    }
    return OS_SUCCESS;
}

//------------------------------------------------------------------------------
/**
 * Test every single memory block if it is readable, writeable and erasable.
 * This test checks if every memory cell is working as expected.
 */
static OS_Error_t
__attribute__((unused))
test_memory_blocks(void)
{
    Debug_LOG_INFO("Testing every memory block...");

    //--------------------------------------------------------------------------
    // Test setup
    static uint8_t buf[FLASH_BLOCK_SZ];

    // pattern expected after erasing a memory block
    static uint8_t buf_ref_empty[FLASH_BLOCK_SZ];
    memset(buf_ref_empty, 0xff, sizeof(buf_ref_empty));

    // pattern expected in first memory block of flash
    static uint8_t buf_ref_pattern_block_0[FLASH_BLOCK_SZ];
    memset(buf_ref_pattern_block_0, 0x5a, sizeof(buf_ref_pattern_block_0));

    // reference pattern
    static uint8_t buf_ref_pattern[FLASH_BLOCK_SZ];
    memset(buf_ref_pattern, 0xa5, sizeof(buf_ref_pattern));
    //--------------------------------------------------------------------------

    OS_Error_t ret;
    off_t addr = 0;
    const size_t start_addr = 0x0;
    const size_t end_addr = FLASH_SZ;
    const size_t print_delta = 50;
    for(addr = start_addr; addr < end_addr; addr += FLASH_BLOCK_SZ)
    {
        //pattern test
        if ((addr/FLASH_BLOCK_SZ) % print_delta == 0)
        {
            Debug_LOG_INFO(
                "Testing block %lld of %d",
                addr/FLASH_BLOCK_SZ, FLASH_SZ/FLASH_BLOCK_SZ);
        }

        ret = test_flash_block(addr, buf, buf_ref_empty, buf_ref_pattern);
        if (OS_SUCCESS != ret)
        {
            Debug_LOG_ERROR(
                "test_flash_block %lld failed at addr 0x%jx, code %d",
                addr/FLASH_BLOCK_SZ,addr, ret);
            break;
        }

        //block erase
        off_t bytes_erased = 0;
        ret = storage_rpc_erase(addr, FLASH_BLOCK_SZ, &bytes_erased);
        if ((ret != OS_SUCCESS) || (bytes_erased != FLASH_BLOCK_SZ))
        {
            Debug_LOG_ERROR(
                "erase failed for block %lld (0x0%jx), code %d",
                addr/FLASH_BLOCK_SZ,addr,ret);
            break;
        }
    }
    Debug_LOG_INFO(
        "Functioning flash up to block %lld (0x0%jx) => %s\n",
        addr/FLASH_BLOCK_SZ,addr,addr==FLASH_SZ ? "All blocks working" : "Defect blocks");

    if(addr != FLASH_SZ)
    {
        return OS_ERROR_ABORTED;
    }
    return OS_SUCCESS;
}

//------------------------------------------------------------------------------
/**
 * This function executes two types of tests:
 *      Test 1: Detect memory size
 *      Test 2: Test if each memory block is read-/writeable
 */
static OS_Error_t
__attribute__((unused))
test_OS_BlockAccess(void)
{
    OS_Error_t ret;
    off_t sz;
    if ((ret = storage_rpc_getSize(&sz)) != OS_SUCCESS)
    {
        Debug_LOG_ERROR("storage_rpc_getSize() failed, code %d", ret);
        return OS_ERROR_ABORTED;
    }

    // Test 1: Memory size detection
    ret = test_memory_size();
    if(OS_SUCCESS != ret)
    {
        return ret;
    }

    // Test 2: Test every memory block (only if test 1 succeeds)
    ret = test_memory_blocks();

    return ret;
}

// Public Functions ------------------------------------------------------------

int run()
{
    Debug_LOG_INFO("Starting NOR Flash test.");
    Debug_LOG_INFO("Expected flash size: %d Bytes (%d MiB)",FLASH_SZ,FLASH_SZ/1024/1024);
    const OS_Error_t ret = test_OS_BlockAccess();
    Debug_LOG_INFO("%s\n",(OS_SUCCESS == ret) ? "FLASH OK!" : "FLASH DEFECT!");
    Debug_LOG_INFO("All tests done!");
    return ret;
}
