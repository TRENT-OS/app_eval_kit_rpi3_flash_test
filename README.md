# Production tests of Eval Kit by Alpha in Ulm (2020)
## Purpose
This test application provides a "user-friendly" way to test the 8MiB W25Q64 
NOR flashs that are shipped together with the Evaluation Kit. The general 
problem with these flashs is that without testing we can not guarantee that 
they funciton properly. In fact, there have been occasions where some of these 
flashs were holding a smaller amount of memory than was claimed by the 
manufacturer. The flashs were tested by Alpha in Ulm (2020). The application 
was specifically created for this purpose and is working with TRENTOS-M v1.2.   
   
Disclaimer: Even with these tests in place, the user should not expect to have
a working flash at all times. Tests will only reveal the presence of bugs but
not guarantee that a program/hardware is working as expected. All tests can do
is trying to test as many test cases as possible to reduce the probability that
the software/hardware is not working as expected.
   
## Description
With these flashs, there were two different scenarios for why a flash was not
working as expected:   
1) The claimed memory size is larger than the actual memory size of the flash.  
2) There are single memory blocks that can not be read or written.  
   
Both test cases are covered by the test application. 

### Detecting memory size
The first test case is probing the actual size of the flash memory. The idea 
behind memory size detection is to write to an address that is one block larger 
than the actual memory size of the flash chip. This would lead to a write 
operation on the very first block of the flash chip as flash memory wraps around 
at the end of the available storage. An overflow is detected if the first block 
of the flash is different than before the write operation. The idea would be to 
write a certain pattern to the first memory block of the flash. Afterwards, 
increasing memory sizes are written with yet another bit pattern. After every 
write operation, the bit pattern of the first block is tested again. In case an 
overflow occurred, the original bit pattern of the first block is overwritten. 
Thus, the actual size of the flash chip is detected.    
    
As flash chips usually come in sizes to the power of 2, we can double the 
suspected memory size on every memory test operation up to 8 MiB of memory 
starting from a block size of 4096 Bytes. This reduces the number of actual 
tests to detect the available size to log2(FLASH_SZ/BLOCK_SZ).   
    
### Testing all memory blocks
This sub-test validates every single block of memory. First a bit pattern is 
validated for a particular block, then the memory block is erased.

### Performance Tweaking
When using the default settings for the flash configuration, the test performs 
very poorly. For a 8MiB flash, the test runs for about 24 minutes. As the 
settings in the default flash configuration, set the write and erase times to 
the maximal values of the datasheet (write: 15ms, erase: 300ms), these values 
can be tweaked. Especially the erase time can be greatly reduced. When set to 
the minimal times (write: 10ms, erase: 45ms), the test duration decreases to 
only 6 minutes. Especially when testing multiple of these flashs, these time 
savings greatly reduce overall testing time. The new timing values are 
implemented on branch SEOS-1774 in "seos_sandbox/components/RPI_SPI_FLASH".   
    
It should be mentioned that the maximal values were set due to the fact that the 
maximal values are guaranteed times. When setting to minimal values, it depends 
on how fast the internal queue of the flash is filled and might lead to issues. 
To guarantee these minimal values busy flag polling should be implemented in the 
flash driver.    
    
More on the flash test can be read at: 
https://wiki.hensoldt-cyber.systems/display/HEN/TRENTOS-M+Flash+test.

## Build description
```
seos_tests/seos_sandbox/scripts/open_trentos_build_env.sh seos_tests/seos_sandbox/build-system.sh seos_tests/src/tests/test_storage rpi3 build-rpi3-Debug-test_flash -DCMAKE_BUILD_TYPE=Debug
```
