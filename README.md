# MemoryManagementSystem

Grace Michnovicz and Skye McKay

Files included MemoryManager.c and VirtualMemoryManager.c

Part 1 - A Basic Memory Management System

In this part we chose to implement the basic memory management system using the implicit free list method. To do this, we used static pointers for the beginning of the heap, and to the end of the allocated area (disregarding internal segmentation). We initialized a base header of 4 bytes in order to maintain 8 byte alignment within our heap. For each allocated block, we included a 4 byte header and footer to hold the size of each allocated block, with the least significant bit being used to specify whether the block is free or allocated. This implies that the smallest memory allocation will result in a block of 16 bytes (header/footer/aligned byte size). We implement Free coalescing by checking whether the memory block after and before are also free, and move the end of the allocated area pointer accordingly. 

Part 2 - A Virtualized Heap Allocation Scheme

For the virtualized heap scheme, we included a large array (Redirection Table) that was made up of elements holding addresses on the heap. The heap was created with same design as part 1, including a 4 byte header. Each VMalloc call returned an address to the location in redirection table, which results in multiple dereferences in order to get to the data on the heap. Data on the heap is always allocated in one contiguous block, and addresses in the redirection table are not necessarily sequential, due to the implementation of VFree. Within VFree, data is freed from the heap and blocks following that block are moved back accordingly. Addresses to the heap are updated accordingly, but their location within the table does not change. Newly freed table is set to NULL, in order to be used for future VMalloc calls. We also maintain pointers for the heap and the redirection table, including a base pointer on the heap, a current pointer to the end of the allocated area, a base pointer to the start of the redirection table, and a pointer to the end of the used space in the redirection table. 


heapChecker()

We included an embedded function in order to print status updates on the current state of the heap at any point. This includes a consistent updating of static variables through program execution and calls. These variables are then displayed within the program itself. For the 4 static variables: rawTotalAllocated, paddedTotalAllocated,rawFreeBytes, alignedTotalFree. We chose for rawTotalAllocated to include the bytes used for the aligned total data, but excluding the bytes for the header and the footer. For paddedTotalAllocated we include the aligned total data as well as the bytes for the header and the footer. For rawFreeBytes, we decrement the total memory size by the aligned data size on every Malloc call. 

Testing

In order to test our program, we included an adaptation of the test suites given by the TFs that is implemented within our main function. This includes the functions: test_stability, test_ff, test_maxNumOfAlloc, test_maxSizeOfAlloc. To test, simply compile and execute each file (pa31.c and pa32.c) and tests will complete. test_ff was updated for the virtual scheme to not test for first fit policy, but to test for proper placement within redirection table and updated addressing on the heap. We included calls to our heapChecker() function below each test call, but left them commented for your discretion. Feel free to implement the heapChecker() anywhere within our program for testing purposes.

