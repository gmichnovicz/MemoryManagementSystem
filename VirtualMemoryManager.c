/* A VIRTUALIZED HEAP ALLOCATION SCHEME.
 Grace Michnovicz, Skye McKay
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*Variables developed from TF test code in order to evaluate our heap */
#define KBLU  "\x1B[34m"
#define KRED  "\x1B[31m"
#define KRESET "\x1B[0m"

#define ERROR_OUT_OF_MEM    0x1
#define ERROR_DATA_INCON    0x2
#define ERROR_ALIGMENT      0x4
#define ERROR_NOT_FF        0x8
#define LOCATION_OF(addr)     ((size_t)(*addr))
#define DATA_OF(addr)         (*(addr))

#define rdtsc(x)      __asm__ __volatile__("rdtsc \n\t" : "=A" (*(x)))

#define DEFAULT_MEM_SIZE 1<<20

#define ALIGNMENT 8
#define ALIGNED(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define DEFAULT_MEM_SIZE 1<<20
#define R 1<<20

/* Types used throughout code */
typedef char* addrs_t;
typedef void* any_t;

/* prototypes for included functions are below */
void VInit(size_t);
addrs_t* VMalloc (size_t size);
void VFree (addrs_t* addr);
addrs_t* VPut (any_t data, size_t size);
void VGet (any_t return_data, addrs_t* addr, size_t size);
void heapChecker(void);
void PrintAddrs(void);
int test_stability(int, unsigned long*, unsigned long*);
int test_ff(void);
int test_maxNumOfAlloc(void);
int test_maxSizeOfAlloc(int);
void print_testResult(int);


static addrs_t RT[R]; //redirection table static array. made up of pointers to the memory heap.
static addrs_t basePointer; //pointer to base address of the heap.
static addrs_t curPointer; //pointer to the end of the allocated memory in the virtual memory heap.
static size_t memSize; //memory size of heap
static addrs_t* tableEndPointer; //pointer to the current "end" in the redirection table, updated during each allocation.


/*static variables needed for heapChecker */
static long int mallocCount = 0; //variable to count the number of malloc requests
static long int freeCount = 0; //variable to count the number of free requests
static long int reqfailCount= 0; //variable to count the failed requests
static long int rawTotalAllocated= 0; //variable to count the raw total memory requested
static long int paddedTotalAllocated = 0; //variable to count the padded total allocated
static long int allocatedBlocks = 0;
static long int freeBlocks = 1;
static long int rawTotalFree; //variable to count the raw total memory free
static long int paddedTotalFree;


int main(int argc, char **argv){
    
    /* a set of tests below that sufficiently test our memory allocating system */
    
    unsigned mem_size = (1<<20);
    
    // Parse the arguments
    if (argc > 2){
        fprintf(stderr, "Usage: %s [buffer size in bytes]\n",argv[0]);
        exit(1);
    }else if (argc == 2){
        mem_size = atoi(argv[1]);
    }
    
    printf("Evaluating a VirtualHeap of %d KBs...\n",mem_size/1024);
    
    unsigned long tot_alloc_time, tot_free_time;
    int numIterations = 1000000;
    
    /* Initialize the heap */
    VInit(mem_size);
    
    /* TEST 1: STABILITY */
    printf("\nTest 1 - Stability and consistency:\n");
    print_testResult(test_stability(numIterations,&tot_alloc_time,&tot_free_time));
    printf("Average clock cycles for a Malloc request: %lu\n",tot_alloc_time/numIterations);
    printf("Average clock cycles for a Free request: %lu\n",tot_free_time/numIterations);
    printf("Total clock cycles for %d Malloc/Free requests: %lu\n",numIterations,tot_alloc_time+tot_free_time);
    /* printf("\n\nheapChecker Results:\n");
     heapChecker();
     */
    
    /* TEST 2: FIRST - FIT */
    printf("\nTest 2 - First-fit policy:\n");
    print_testResult(test_ff());
    /*printf("\n\nheapChecker Results:\n");
     heapChecker();
     */
    
    /* TEST 3: MAX # OF ALLOCATIONS */
    printf("\nTest 3 - Max # of 1 byte allocations:\n");
    printf("[%s%i%s]\n",KBLU,test_maxNumOfAlloc(), KRESET);
    /*printf("\n\nheapChecker Results:\n");
     heapChecker();
     */
    
    /*TEST 4: MAX ALLOCATION SIZE */
    printf("\nTest 4 - Max allocation size:\n");
    printf("[%s%i KB%s]\n\n", KBLU, test_maxSizeOfAlloc(4*1024*1024)>>10, KRESET);
    /*
     printf("\n\nheapChecker Results:\n\n");
     heapChecker();
     */
    
    
    
}

void VInit(size_t size){
    /*
     use the system malloc() routine (new in C++) only to allocate size bytes for the initial
     memory area, M2. baseptr is the starting address of M2.
     */
    
    /* add other initializations as needed */
    basePointer = (addrs_t) malloc (size); //set the static basePointer variable to track the virtual address to the start of the heap
    memSize = size;     // set the static memsize variable to track when the heap is full.
    curPointer = basePointer + 4; //set the pointer for the end of the allocated virtual memory to be the start of the v-heap.
    tableEndPointer = &RT; //initialize the table pointer to be the start of the redirection table.
    rawTotalFree = size - 4; //Set both to size of the data into the global variables that calculate the free space
    paddedTotalFree = size; // subtract 4 for the base header also.
}


addrs_t* VMalloc(size_t size){
    /*Virtualized Malloc implementation */
    
    unsigned int alignedSize = ALIGNED(size);
    
    //Checks to see if size requested can fit into the Heap
    if (alignedSize > memSize){
        reqfailCount++;
        return NULL;
    }
    
    addrs_t* tableIndex = &RT; //set a search table pointer to find the end of the allocated
    addrs_t RTentry; //value to be added to the redirection table.
    
    if ((curPointer+size) >= (basePointer+memSize)){ //only one contiguous block of memory, so therefore only free space is at the end of the heap.
        reqfailCount++;
        return NULL;
    }
    
    /*Sets the header and footer of the block being allocated*/
    *(unsigned int*)curPointer = alignedSize;
    *(unsigned int*)(curPointer + alignedSize + 4)= alignedSize;
    RTentry = curPointer + 4;
    curPointer = curPointer + 8 + alignedSize; // increment current pointer to address the end of the allocated block
    
    
    /*Looks through table for an empty space if there is none just extends the table
     assumes tableEndPointer < actual memory address at the table end of the allocated space for the table */
    while(tableIndex!=tableEndPointer && *tableIndex!=NULL){
        tableIndex = (addrs_t*)((char*) tableIndex + sizeof(addrs_t));
    }
    
    /*assigns table pointer to heap pointer */
    *(tableIndex) = RTentry; //fill redirection table with the address to the result of mallocing - type addrs_t.
    
    /*updates the pointer to the end of the redirection table if needed*/
    if (tableIndex == tableEndPointer){
        tableEndPointer = (addrs_t*) ((char*)tableEndPointer + sizeof(addrs_t*));//set the pointer to the end of the redirection table to be waiting for the next entry
    }
    
    /*Increments global variables for HeapChecker */
    rawTotalAllocated += alignedSize;
    paddedTotalAllocated += alignedSize + 8;
    rawTotalFree -= alignedSize;
    paddedTotalFree -= (alignedSize + 8);
    allocatedBlocks++;
    mallocCount++;
    
    /* returns address to the redirection table */
    return tableIndex;
}


addrs_t* VPut(any_t data, size_t size){
    /* function to allocate data onto the heap */
    
    addrs_t* RTpointer;
    RTpointer = VMalloc(size); //pointer to the redirection table
    //*RTpointer is equivalent to the address on the redirection table where the address to the memory must be copied to, not the memory itself
    if (RTpointer == NULL){
        return NULL;
    }
    
    /*Places data at location */
    memcpy((void*)((*RTpointer)),data,size);
    
    return RTpointer;
}



void VFree(addrs_t* addr){
    //Checks for failures
    if (addr >= tableEndPointer || *addr == NULL){
        reqfailCount++;
        return;
    }
    
    /*Find the size of what you're taking out and set up the values need for a while loop to check for coalescing to the heap to maintain one contiguous block */
    unsigned int temp;
    addrs_t Heap = *addr;
    size_t size = (*(unsigned int *)((char *)(Heap) - 4))& ~0x7;
    size_t addon = 8;
    addrs_t* TableIndex = &RT;
    curPointer -=  (size + 8); //update curPointer accordingly
    addrs_t index;
    
    
    /*Goes through the table to find if what any address the table pointers point to are the next one after what's removed, for proper coalescing */
    while ( TableIndex < tableEndPointer){
        index = *TableIndex;
        if (index == size + addon + Heap){ //if we found a block after the freed block to be moved.
            temp = (*(unsigned  int *)index);
            size_t cursize = (*(unsigned int *)((char *)(index) - 4))& ~0x7;
            addon += cursize; //+ 8;
            index -= (size + 8);
            *(index - 4)  = cursize;
            *(index + cursize)= cursize;
            *index = (unsigned int*) temp;
            *TableIndex = index;
            
            TableIndex = &RT;
        }
        else { //if we did not find a block after the free block yet.
            TableIndex = (addrs_t*) (((char*)TableIndex) + sizeof(addrs_t*));
        }
    }
    
    /*update static heapchecker variables*/
    allocatedBlocks--;
    rawTotalAllocated -= size;
    paddedTotalAllocated -= (size + 8);
    rawTotalFree += size;
    paddedTotalFree += size + 8;
    freeCount++;
    
    *addr = NULL; //free the internal entry in the redirection table.
}


void VGet(any_t return_data, addrs_t* addr, size_t size){
    /*Sets return_data to the data at *(*(addr)) then frees addr */
    
    addrs_t Heap = *addr;
    unsigned int temp = (*(unsigned int *)Heap);
    size_t cursize = (*(unsigned int *)((char *)(Heap) - 4))& ~0x7;
    addrs_t TableIndex = &RT;
    addrs_t index;
    VFree(addr);
    while( size > cursize && TableIndex < tableEndPointer){
        index = *TableIndex;
        if (Heap == index){
            cursize += (*(unsigned int *)((char *)(index) - 4))& ~0x7;
            temp += (*(unsigned int *)index);
            VFree(TableIndex);
            TableIndex = &RT;
        }
        else{
            TableIndex += sizeof(addrs_t);
        }
    }
    
    *((unsigned int * )return_data) = (unsigned int*)temp; //copies the data to the return_data.
}


/*The heapChecker to be implemented anywhere you want throughout the code to check
 the status of the global vairables.*/
void heapChecker(void){
    printf("Number of allocated blocks: %ld\n",allocatedBlocks); //
    
    printf("Number of free blocks: %ld\n",freeBlocks); //FREE BLOCKS discounting padding bytes
    
    printf("Raw total number of bytes allocated: %ld\n",rawTotalAllocated); //RAW TOTAL ALLOCATED which is the actual total bytes requested
    
    printf("Padded total number of bytes allocated: %ld\n",paddedTotalAllocated); //PADDED TOTAL ALLOCATED which is the total bytes requested plus internally fragmented blocks wasted due to padding/alignment
    
    printf("Raw total number of bytes free: %ld\n",rawTotalFree); //RAW TOTAL FREE
    
    printf("Aligned total number of bytes free: %ld\n",paddedTotalFree); //ALIGNED TOTAL FREE which is sizeof(M1) minus the padded total number of bytes allocated.
    
    printf("Total number of Malloc requests: %ld\n",mallocCount); //TOTAL MALLOC requests
    
    printf("Total number of Free requests: %ld\n",freeCount); //TOTAL FREE REQUESTS
    
    printf("Total number of request failures: %ld\n",reqfailCount); //TOTAL which were unable to satisfy the allocation or de-allocation requests
}


/* function we implemented in order to check the current status of our addresses in our different memory areas. */
void PrintAddrs(void){
    printf("\n");
    printf("Base = %p\n", basePointer);
    printf("Cur = %p\n",curPointer);
    printf("RT is %p\n",RT);
    printf("tableend Pointer is %p\n",tableEndPointer);
    
}

/* FUNCTIONS BELOW WERE DEVELOPED FROM TF TEST SUITES IN ORDER TO PROPERLY ASSESS OUR HEAP */

void print_testResult(int code){
    if (code){
        printf("[%sFailed%s] due to: ",KRED,KRESET);
        if (code & ERROR_OUT_OF_MEM)
            printf("<OUT_OF_MEM>");
        if (code & ERROR_DATA_INCON)
            printf("<DATA_INCONSISTENCY>");
        if (code & ERROR_ALIGMENT)
            printf("<ALIGMENT>");
        printf("\n");
    }
    else{
        printf("[%sPassed%s]\n",KBLU, KRESET);
    }
}

int test_stability(int numIterations, unsigned long* tot_alloc_time, unsigned long* tot_free_time){
    int i, n, res = 0;
    char s[80];
    addrs_t* addr1;
    addrs_t* addr2;
    char data[80];
    char data2[80];
    
    unsigned long start, finish;
    *tot_alloc_time = 0;
    *tot_free_time = 0;
    //numIterations = 1;
    for (i = 0; i < numIterations; i++) {//numIterations
        n = sprintf (s, "String 1, the current count is %d\n", i);
        rdtsc(&start);
        addr1 = VPut(s, n+1);
        rdtsc(&finish);
        *tot_alloc_time += finish - start;
        addr2 = VPut(s, n+1);
        // Check for heap overflow
        if (!addr1 || !addr2){
            res |= ERROR_OUT_OF_MEM;
            break;
        }
        // Check aligment
        if (((uint64_t)addr1 & (ALIGNMENT-1)) || ((uint64_t)addr2 & (ALIGNMENT-1)))
            res |= ERROR_ALIGMENT;
        // Check for data consistency
        rdtsc(&start);
        VGet((any_t)data2, addr2, n+1);
        rdtsc(&finish);
        *tot_free_time += finish - start;
        if (!strcmp(data,data2))
            res |= ERROR_DATA_INCON;
        VGet((any_t)data2, addr1, n+1);
        if (!strcmp(data,data2))
            res |= ERROR_DATA_INCON;
    }
    return res;
}

int test_ff(void){
    // Round 1 - 2 consequtive allocations should be allocated after one another
    addrs_t* v1,v2,v3,v4;
    
    v1 = VMalloc(8);
    v2 = VMalloc(4);
    
    // Round 2 - New allocation should be placed in a free block at top if fits
    VFree(v1);
    v3 = VMalloc(64);
    v4 = VMalloc(5);
    
    // Round 3 - Correct merge
    VFree(v4);
    VFree(v2);
    v4 = VMalloc(10);
    
    // Round 4 - Correct Merge 2
    VFree(v4);
    VFree(v3);
    v4 = VMalloc(256);
    // Clean-up
    VFree(v4);
    return 0;
}

int test_maxNumOfAlloc(void){
    int count = 0;
    char *d = "x";
    const int testCap = 1000000;
    addrs_t* allocs[testCap];
    
    while ((allocs[count]=VPut(d,1)) && count < testCap){
        addrs_t data = *allocs[count];
        if (DATA_OF(data)!='x') break;
        count++;
    }
    // Clean-up
    int i;
    for (i = 0 ; i < count ; i++)
        VFree(allocs[i]);
    return count;
}

int test_maxSizeOfAlloc(int size){
    char* d = "x";
    if (!size) return 0;
    addrs_t* v1 = VMalloc(size);
    if (v1){
        return size + test_maxSizeOfAlloc(size>>1);
    }else{
        return test_maxSizeOfAlloc(size>>1);
    }
}

