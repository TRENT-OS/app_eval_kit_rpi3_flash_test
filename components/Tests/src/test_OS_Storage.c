/*
 * Copyright (C) 2020, Hensoldt Cyber GmbH
 */

#include <camkes.h>
#include "LibDebug/Debug.h"
#include "OS_Dataport.h"
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include "TimeServer.h"

#define FLASH_SZ    8*1024*1024
#define BLOCK_SZ    4096
#define PAGE_SZ     256
#define BLOCKS_TO_TEST 128

static uint64_t read_timestamps[2 * BLOCKS_TO_TEST];
static uint64_t write_timestamps[2 * BLOCKS_TO_TEST];
static uint64_t erase_timestamps[2 * BLOCKS_TO_TEST];

static OS_Dataport_t port_storage  = OS_DATAPORT_ASSIGN(storage_dp);

static const if_OS_Timer_t timer =
    IF_OS_TIMER_ASSIGN(
        timeServer_rpc,
        timeServer_notify);

static uint64_t
get_time_nsec(
    void)
{
    OS_Error_t err;
    uint64_t nsec;

    if ((err = TimeServer_getTime(&timer, TimeServer_PRECISION_NSEC,
                                  &nsec)) != OS_SUCCESS)
    {
        Debug_LOG_ERROR("TimeServer_getTime() failed with %d", err);
        nsec = 0;
    }

    return nsec;
}

static OS_Error_t
__attribute__((unused))
read_performance_test(void){
    uint64_t timestamp, newTimestamp = 0;
    
    for (size_t i = 0; i < BLOCKS_TO_TEST; i++)
    {
        size_t bytes_read = 0;
        timestamp = get_time_nsec();
        OS_Error_t ret = storage_rpc_read(i * BLOCK_SZ, BLOCK_SZ, &bytes_read);
        if ((ret != OS_SUCCESS) || (bytes_read != BLOCK_SZ))
        {
            Debug_LOG_ERROR("storage_rpc_read() failed");
            return OS_ERROR_ABORTED;
        }
        newTimestamp = get_time_nsec();
        read_timestamps[2*i] = timestamp;
        read_timestamps[2*i+1] = newTimestamp;
    }
    return OS_SUCCESS;
}

static OS_Error_t
__attribute__((unused))
write_performance_test(void){
    uint64_t timestamp, newTimestamp = 0;
    OS_Error_t ret;
    
    static uint8_t buf_ref_pattern[BLOCK_SZ];
    memset(buf_ref_pattern, 0xa5, sizeof(buf_ref_pattern));

    for (size_t i = 0; i < BLOCKS_TO_TEST; i++)
    {
        timestamp = get_time_nsec();
        for (unsigned int j = 0; j<(BLOCK_SZ/PAGE_SZ); j++)
        {
            memcpy(OS_Dataport_getBuf(port_storage), buf_ref_pattern, PAGE_SZ);
            off_t write_addr = i * BLOCK_SZ + j*PAGE_SZ;
            size_t bytes_written = 0;
            ret = storage_rpc_write(write_addr, PAGE_SZ, &bytes_written);
            if ((OS_SUCCESS != ret) || (bytes_written != PAGE_SZ))
            {
                Debug_LOG_ERROR("storage_rpc_write failed");
                return OS_ERROR_ABORTED;
            }
        }
        newTimestamp = get_time_nsec();
        write_timestamps[2*i] = timestamp;
        write_timestamps[2*i+1] = newTimestamp;
    }
    return OS_SUCCESS;
}

static OS_Error_t
__attribute__((unused))
erase_performance_test(void){
    uint64_t timestamp, newTimestamp = 0;
    OS_Error_t ret;
    
    for (size_t i = 0; i < BLOCKS_TO_TEST; i++)
    {
        off_t bytes_erased = 0;
        timestamp = get_time_nsec();
        ret = storage_rpc_erase(i * BLOCK_SZ, BLOCK_SZ, &bytes_erased);
        if ((ret != OS_SUCCESS) || (bytes_erased != BLOCK_SZ))
        {
            Debug_LOG_ERROR("storage_rpc_erase() failed");
            return OS_ERROR_ABORTED;
        }
        newTimestamp = get_time_nsec();
        erase_timestamps[2*i] = timestamp;
        erase_timestamps[2*i+1] = newTimestamp;
    }
    return OS_SUCCESS;
}

static void dump_info(uint64_t * arr,int size){
    printf("#;call_timestamp;return_timestamp;delta\n");
    for (size_t i = 0; i < size; i++)
    {
        printf(
            "%zd;%" PRIu64 ";%" PRIu64 ";%" PRIu64 "\n",
            (i+1),arr[2 * i],arr[2 * i + 1],(arr[2 * i + 1] - arr[2 * i]));
    }
    printf("\n");
    
    uint64_t delta = 0;
    int counter = 0;
    for (counter = 0; counter < size; counter++) delta += (arr[2 * counter + 1] - arr[2 * counter]);
    delta /= counter;
    printf("Average delta for call: %lf sec\n",(double)delta / (double)NS_IN_S);
    printf("Throughput: %lf B/s \n",(double)BLOCK_SZ / ((double)delta / (double)NS_IN_S));
    
    for (delta = 0, counter = 1; counter < size - 1; counter++) delta += (arr[2 * (counter + 1)] - arr[2 * counter]);
    delta /= counter;
    printf("Average delta for two calls: %lf sec\n",(double)delta / (double)NS_IN_S);
    printf("\n");
}

int run()
{
    OS_Error_t ret = 0;
    printf("Performance tests.\n\n");

    printf("Read performance tests.\n");
    ret = read_performance_test();
    if (ret != OS_SUCCESS)
    {
        Debug_LOG_ERROR("Read performance test failed.");
    }
    dump_info(read_timestamps,BLOCKS_TO_TEST);
    
    printf("Write performance tests.\n");
    ret = write_performance_test();
    if (ret != OS_SUCCESS)
    {
        Debug_LOG_ERROR("Write performance test failed.");
    }
    dump_info(write_timestamps,BLOCKS_TO_TEST);
    
    printf("Erase performance tests.\n");
    ret = erase_performance_test();
    if (ret != OS_SUCCESS)
    {
        Debug_LOG_ERROR("Erase performance test failed.");
    }
    dump_info(erase_timestamps,BLOCKS_TO_TEST);

    printf("All tests done!\n");
    return 0;
}
