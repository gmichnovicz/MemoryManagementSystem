/* A BASIC MEMORY MANAGEMENT SYSTEM
   Grace Michnovicz, Skye Mckay
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*Variables developed from TF test code in order to evaluate our heap */
#define ALIGNMENT 8
#define ALIGNED(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define rdtsc(x)      __asm__ __volatile__("rdtsc \n\t" : "=A" (*(x)))
#define DEFAULT_MEM_SIZE 1<<20
#define ERROR_OUT_OF_MEM    0x1
#define ERROR_DATA_INCON    0x2
#define ERROR_ALIGMENT      0x4
#define ERROR_NOT_FF        0x8

#define LOCATION_OF(addr)     ((size_t)addr)
#define DATA_OF(addr)         (*(addr))

#define KBLU  "\x1B[34m"
#define KRED  "\x1B[31m"
#define KRESET "\x1B[0m"

/* Types used throughout code */
typedef char* addrs_t;
typedef void* any_t;

/* prototypes for included functions are below */
void Init(size_t);
addrs_t Malloc(size_t);
void Free(addrs_t);
addrs_t Put(any_t, size_t);
void Get(any_t, addrs_t, size_t);
void PrintAddrs(void);
void heapChecker(void);
int test_stability(int, unsigned long*, unsigned long*);
int test_ff(void);
int test_maxNumOfAlloc(void);
int test_maxSizeOfAlloc(int);
void print_testResult(int);



static addrs_t basePointer; //static starting address of our heap space
static addrs_t curPointer; //current end address of allotted memory
static size_t memSize; //static memory size of allocated heap

/*static variables needed for heapChecker */
static long int mallocCount = 0; //variable to count the number of malloc requests
static long int freeCount = 0; //variable to count the number of free requests
static long int reqfailCount; //variable to count the failed requests
static long int rawTotalAllocated; //variable to count the raw total memory requested
static long int paddedTotalAllocated; //variable to count the padded total allocated
static long int allocatedBlocks = 0; //variable to count the number of allocated blocks
static unsigned long tot_alloc_time;
static unsigned long tot_free_time;
static long int freeBlocks;
static long int rawFreeBytes;



int main(int argc, char **argv){
    
    /* a set of tests below that sufficiently test our memory allocating system */
    
    int i, n;
    char s[80];
    char data[80];
    int mem_size = DEFAULT_MEM_SIZE;
    int numIterations = 1000000;
    
    if (argc>2){
        fprintf(stderr,"Usage %s [memory area size in bytes]\n",argv[0]);
        exit(1);
    }
    
    else if(argc==2)
        mem_size = atoi(argv[1]);
    
    // Initialize the heap
    Init(mem_size);
    
    printf("Evaluating a %s of %d KBs...\n","Heap",mem_size/1024);
    
    /* TEST 1: STABILITY */
    printf("\nTest 1: Testing Stability...\n");
    print_testResult(test_stability(numIterations,&tot_alloc_time,&tot_free_time));
    printf("Average clock cycles for a Malloc request: %lu\n",tot_alloc_time/numIterations);
    printf("Average clock cycles for a Free request: %lu\n",tot_free_time/numIterations);
    printf("Total clock cycles for %d Malloc/Free requests: %lu\n",numIterations,tot_alloc_time+tot_free_time);
    
    /*
     printf("\n\nheapChecker Results:\n");
     heapChecker();
     */
    
    /* TEST 2: FIRST - FIT */
    printf("\nTest 2 - First-fit policy...\n");
    print_testResult(test_ff());
    
    /*
     printf("\n\nheapChecker Results:\n");
     heapChecker();
     */
    
    /* TEST 3: MAX # OF ALLOCATIONS */
    printf("\nTest 3: Testing Max # of 1 byte allocations...\n");
    printf("[%s%i%s]\t\t\n",KBLU,test_maxNumOfAlloc(), KRESET);
    
    /*
     printf("\n\nheapChecker Results:\n");
     heapChecker();
     */
    
    /*TEST 4: MAX ALLOCATION SIZE */
    printf("\nTest 4: - Max allocation size...\n");
    printf("[%s%i KB%s]\t\t\n", KBLU, test_maxSizeOfAlloc(4*1024*1024)>>10, KRESET);
    
    /*
     printf("\n\nheapChecker Results:\n\n");
     heapChecker();
     */
    
    return 0;
}



/* Function we wrote in order to check where the pointer to the base and current is within the heap */
void PrintAddrs(void){
    printf("BasePointer is %p\n",basePointer);
    printf("CurPointer is %p\n",curPointer);
}


void Init(size_t size){
    /*
     use the system malloc() routine (new in C++) only to allocate size bytes for the initial
     memory area, M1. baseptr is the starting address of M1.
     */
    /* add other initializations as needed */
    
    basePointer = (addrs_t) malloc (size);//baseptr; //set the static basePointer variable to track the virtual address to the start of the heap
    curPointer = basePointer + 4; // set the curPointer to be the start of the list.7
    *basePointer = (unsigned int) size; // set the initial header to be the size of the entire thing.
    memSize = size;     // set the static memsize variable to track when the heap is full.
    rawFreeBytes = memSize-4; //subtract 4 from memSize in order to account for the 4 bytes included in the header.
    freeBlocks = 1;
}

addrs_t Malloc (size_t size){
    /* implement a memory allocation routine aligned on 8 byte boundaries.
     */
    
    unsigned int alignedSize = ALIGNED(size); //align size by 8 - originally had size_t
    
    /* update static heapChecker variables */
    mallocCount++;
    rawTotalAllocated+=alignedSize;
    paddedTotalAllocated+=(alignedSize+8);
    rawFreeBytes-=alignedSize;
    
    if (alignedSize > memSize) //if the size request is greater than the size available, return NULL.
    {
        reqfailCount++;
        return NULL;
    }
    
    /* locate the first available block for allocation. may be segmented within or at the end of the allocated block. */
    addrs_t memBlock;
    addrs_t searchPtr = basePointer + 4;
    while ((searchPtr != curPointer) && ((*searchPtr & 1) || ( (*searchPtr & -2) < alignedSize))){
        searchPtr = searchPtr + (*(unsigned int *)searchPtr & -2) + 8;
    }
    
    
    /*if searchPtr ends up being the end of the heap, return null*/
    if (searchPtr >= (basePointer + memSize))
    {
        reqfailCount++;
        return NULL;
    }
    
    
    /* Found the allocation block not to an internal block. */
    if (searchPtr == curPointer){
        memBlock = curPointer;  //set the memBlock return address to be the address of the curPointer.
        *(unsigned int*)memBlock = (unsigned int) alignedSize | 1; //set the first 4 bytes of memBlock to be the size word. Add 1 to size to denote that it is an allocated block.
        *(unsigned int*)(memBlock + alignedSize + 4) = (unsigned int) alignedSize | 1; //set the footer of the block to also be the size, also adding 1 to denote allocation.
        curPointer = memBlock + (*(unsigned int *)memBlock & -2) + 8; // set the curPointer to be the byte following the allocated block (accounting for the 4 byte footer)
        allocatedBlocks++; //update heapChecker variable accordingly.
        return memBlock + 4; //return address to the start of the data within the newly allocated block.
    }
    
    //otherwise searchPointer is an internal block and needs to be potentially split
    unsigned int oldSize = *(unsigned int *) searchPtr & -2; //type cast to be a 4 byte word, and mask out allocation bit.
    *(unsigned int *)searchPtr = alignedSize | 1; // marks that it is now an allocated block.
    *(searchPtr + alignedSize + 4) = alignedSize | 1; //mark the footer.
    
    if (alignedSize == oldSize){ //if there is no internal segmentation.
        allocatedBlocks++;
        freeBlocks--;
    }
    
    if (alignedSize < oldSize){ //if there is internal segmentation, update the blocks accordingly.
        unsigned int sizeDif = oldSize - alignedSize; //find the difference in the size needed to separate the blocks.
        *(searchPtr + alignedSize + 8) = sizeDif; //set the rest of the un-allocated internal block to have the new size that it needs.
        *(searchPtr + alignedSize + 12 + sizeDif) = sizeDif; //set the footer of the split block to hold the size.
        allocatedBlocks++;
    }
    
    return searchPtr + 4; // return the address to the start of the data in the new block
}

void Free(addrs_t addr){
    
    
    /* find addresses of all the memory blocks*/
    addrs_t footer, header;
    header = (addr - 4);
    size_t size = (*(unsigned int *) header) & ~0x7;
    footer = (header + size +4);
    
    /* update static heap checker variables */
    freeCount++;
    rawTotalAllocated -= size;
    rawFreeBytes+=size;
    paddedTotalAllocated -= (size+8);
    freeBlocks++;
    
    /* mark the header and footer of the freed block to be free */
    (*(unsigned int *)header) = (size & -2);
    (*(unsigned int*)footer) = (size & -2);
    
    int colNext = 0;
    
    /* find addresses of the next block */
    addrs_t next, nxthdr, nxtftr;
    next = (header + size + 8);
    
    if (next < curPointer)
    {
        next += 4;
        nxthdr = (next - 4);
        size_t nextsize = (*(unsigned int *) next)& ~0x7;
        nxtftr = (next + nextsize);
        //Checks to see if next needs to be coalesced
        
        if( !(*(unsigned int *)nxthdr & 1))
        {
            size += nextsize+8;
            (*(unsigned int *)header) = (size & -2);
            (*(unsigned int *)nxtftr) = (size & -2);
            colNext = 1;
            freeBlocks--; //one less freeblock since two blocks will be coalesced
        }
    }
    
    else
    {
        curPointer = header; //if the next pointer is at the curPointer, move the curPointer back to account for free.
    }
    
    
    if (addr-4 != basePointer + 4){
        addrs_t prev, prvhdr, prvftr;
        prvftr = (header - 4);
        size_t prevsize = (*(unsigned int *) prvftr)& ~0x7;
        prev = (header - prevsize - 4);
        prvhdr = (prev - 4);
        
        //Checks to see if block before needs to be coalesced
        if (!(*(unsigned int *) prvhdr & 1)){
            
            if (curPointer == header){ //moves the curPointer accordingly.
                curPointer = prvhdr;
            }
            freeBlocks--; //one less freeblock since the prior block is coalesced as well.
            size+=prevsize+8; //update block size based on previous size.
            (*(unsigned int *)prvhdr)= (size & -2); //set the header of the previous block to the updated size.
            
            
            if (colNext) //update the correct footer based on size.
            {
                (*(unsigned int *)nxtftr) =   (size & -2);
            }
            else{
                (*(unsigned int *)footer)=   (size & -2);
            }
        }
    }
    
    if (curPointer == basePointer+4){ //if we have managed to move the curPointer back to the start of the memory heap, we know we have only one free block.
        freeBlocks = 1;
    }
    
    allocatedBlocks--; //decrement the number of allocatedBlock.
}


addrs_t Put(any_t data, size_t size){
    /*allocate size bytes from M1 using Malloc(). Copy size bytes of data into Malloc'd memory.
     You can assume data is a storage area outside M1. Return starting address of data in Malloc'd memory.
     */
    addrs_t baseAddress = Malloc(size); // returns starting address of the memory available within the block allocated.
    
    if (baseAddress == NULL){
        return NULL;
    }
    
    memcpy(( (void*) (baseAddress)),data,size); //Copies data to the address returned by the call to Malloc
    return baseAddress;
    
}

void Get(any_t return_data, addrs_t addr, size_t size){
    /* copy size bytes from addr in the memory area, M1, to data address.
     As with Put(), you can assume data is a storage area outside M1. De-allocate size
     bytes of memory starting from addr using Free().
     */
    
    *((unsigned int * )return_data) =  *((unsigned int *)addr);
    
    size_t cursize = (*(unsigned int *) (addr - 4)) & ~0x7;
    addrs_t next = (addr + cursize + 4);
    Free(addr);
    while (size > cursize && next < curPointer){
        size -= cursize;
        *((unsigned int * )return_data) +=  *((unsigned int *)next+ 4);
        cursize = (*(unsigned int *) (next)) & ~0x7;
        Free(next + 4);
        next = (next + cursize + 8);
    }
    
    
}


/* heapChecker() makes use of static variables that are altered within varied areas of program execution in order to assess programs efficiency*/

void heapChecker(){
    
    
    /* Prints static values that have been updated throughout the implementation of the heap */
    
    printf("Number of allocated blocks: %ld\n",allocatedBlocks);
    
    printf("Number of free blocks: %ld\n",freeBlocks); //FREE BLOCKS discounting padding bytes
    
    printf("Raw total number of bytes allocated: %ld\n",rawTotalAllocated); //RAW TOTAL ALLOCATED which is the actual total bytes requested
    
    printf("Padded total number of bytes allocated: %ld\n",paddedTotalAllocated); //PADDED TOTAL ALLOCATED which is the total bytes requested plus internally fragmented blocks wasted due to padding/alignment
    
    printf("Raw total number of bytes free: %ld\n",rawFreeBytes); //RAW TOTAL FREE
    
    printf("Aligned total number of bytes free: %ld\n",memSize-paddedTotalAllocated); //ALIGNED TOTAL FREE which is sizeof(M1) minus the padded total number of bytes allocated.
    
    printf("Total number of Malloc requests: %ld\n",mallocCount); //TOTAL MALLOC requests
    
    printf("Total number of Free requests: %ld\n",freeCount); //TOTAL FREE REQUESTS
    
    printf("Total number of request failures: %ld\n",reqfailCount); //TOTAL which were unable to satisfy the allocation or de-allocation requests
    
    printf("Average clock cycles for a Malloc request: %ld\n",tot_alloc_time); //tot_alloc_time and below is allocated based on different program calls.
    
    printf("Average clock cycles for a Free request: %ld\n",tot_free_time);
    
    printf("Total clock cycles for all requests: %ld\n",tot_free_time+tot_alloc_time);
    
}



/* BELOW ARE EMBEDDED TEST SUITES FROM OUR TFS IN ORDER TO IMPLEMENT TESTING OF OUR HEAP*/

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
    }else{
        printf("[%sPassed%s]\n",KBLU, KRESET);
    }
}

int test_ff(void){
    int err = 0;
    addrs_t v1;
    addrs_t v2;
    addrs_t v3;
    addrs_t v4;
    
    // Round 1 - 2 consequtive allocations should be allocated after one another
    v1 = Malloc(8);
    v2 = Malloc(4);
    
    if (LOCATION_OF(v1) >= LOCATION_OF(v2))
        err |= ERROR_NOT_FF;
    if ((LOCATION_OF(v1) & (ALIGNMENT-1)) || (LOCATION_OF(v2) & (ALIGNMENT-1)))
        err |= ERROR_ALIGMENT;
    
    //Round 2 - New allocation should be placed in a free block at top if fits
    Free(v1);
    v3 = Malloc(64);
    v4 = Malloc(5);
    if (LOCATION_OF(v4) != LOCATION_OF(v1) || LOCATION_OF(v3) < LOCATION_OF(v2))
        err |= ERROR_NOT_FF;
    if ((LOCATION_OF(v3) & (ALIGNMENT-1)) || (LOCATION_OF(v4) & (ALIGNMENT-1)))
        err |= ERROR_ALIGMENT;
    
    //Round 3 - Correct merge
    Free(v4);
    Free(v2);
    
    v4 = Malloc(10);
    if (LOCATION_OF(v4) != LOCATION_OF(v1))
        err |= ERROR_NOT_FF;
    
    //Round 4 - Correct Merge 2
    Free(v4);
    Free(v3);
    
    v4 = Malloc(256);
    if (LOCATION_OF(v4) != LOCATION_OF(v1))
        err |= ERROR_NOT_FF;
    Free(v4);
    return err;
}


int test_stability(int numIterations, unsigned long* tot_alloc_time, unsigned long* tot_free_time){
    int i, n, res = 0;
    char s[80];
    addrs_t addr1;
    addrs_t addr2;
    char data[80];
    char data2[80];
    
    unsigned long start, finish;
    *tot_alloc_time = 0;
    *tot_free_time = 0;
    
    for (i = 0; i < numIterations; i++) {
        n = sprintf (s, "String 1, the current count is %d\n", i);
        rdtsc(&start);
        addr1 = Put(s, n+1);
        rdtsc(&finish);
        *tot_alloc_time += finish - start;
        addr2 = Put(s, n+1);
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
        Get((any_t)data2, addr2, n+1);
        rdtsc(&finish);
        *tot_free_time += finish - start;
        if (!strcmp(data,data2))
            res |= ERROR_DATA_INCON;
        Get((any_t)data2, addr1, n+1);
        if (!strcmp(data,data2))
            res |= ERROR_DATA_INCON;
    }
    return res;
}


int test_maxNumOfAlloc(){
    int count = 0;
    char *d = "x";
    const int testCap =1000000;
    addrs_t allocs[testCap];
    
    while ((allocs[count]=Put(d,1)) && count < testCap){
        if (*(allocs[count])!='x') break;
        count++;
    }
    //Clean-up
    int i;
    for (i = 0 ; i < count ; i++)
        Free(allocs[i]);
    return count;
}


int test_maxSizeOfAlloc(int size){
    char* d = "x";
    if (!size) return 0;
    addrs_t v1 = Malloc(size);
    if (v1){
        return size + test_maxSizeOfAlloc(size>>1);
    }else{
        return test_maxSizeOfAlloc(size>>1);
    }
}



