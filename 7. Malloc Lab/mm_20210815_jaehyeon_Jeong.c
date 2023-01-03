/* 
 * mm_20210815_jaehyeon_Jeong
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* Macro */
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

//Define max and min 
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) > (y) ? (y) : (x))

//PACK, GET, PUT, SIZE, ALLOC
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (val))
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

//Header, Footer
#define HDRP(ptr) ((char *)(ptr) - WSIZE)
#define FTRP(ptr) ((char *)(ptr) + GET_SIZE(HDRP(ptr)) - DSIZE)

//Given block pointer, compute address of next and previous blocks
#define NEXT_BLKP(ptr) ((void *)(ptr) + GET_SIZE(HDRP(ptr)))
#define PREV_BLKP(ptr) ((void *)(ptr) - GET_SIZE((void *)(ptr) - DSIZE))

//Useful pointer reference
#define GET_PREV(ptr) (*(char **)(ptr))
#define SET_PREV(prev_ptr, next_ptr) (GET_PREV(prev_ptr) = next_ptr)
#define GET_NEXT(ptr) (*(char **)(ptr + WSIZE))
#define SET_NEXT(prev_ptr, next_ptr) (GET_NEXT(prev_ptr) = next_ptr)

//Extra Function
static void *extend_heap(size_t size); //extends heap size
static void *coalesce(void *ptr); //coalesces free blocks
static void *place(void *ptr, size_t asize); //place block
static void *find_fit(size_t asize); //find position 
static void insert_list(void *ptr, size_t size); //insert free block from list
static void delete_list(void *ptr); //delete free block form list

//Global variable
static char *heap_list = 0; //heap_list
static char *seg_list[32]; //segregated_free_list

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    int i;
    //make the initial heap
    for (i = 0; i < 32; i+=1) {
        seg_list[i] = NULL; // makes every element of lists to NULL
    }
    
    //return -1 when requires extra than maximum val  
    if ((heap_list = mem_sbrk(4*WSIZE)) == (void *)-1) 
        return -1;
    PUT(heap_list + 0*WSIZE, 0); // allignment padding
    //set prologue header and footer, epilogue with WSIZE
    PUT(heap_list + (1*WSIZE), PACK(DSIZE, 1)); //header
    PUT(heap_list + (2*WSIZE), PACK(DSIZE, 1)); //footer
    PUT(heap_list + (3*WSIZE), PACK(0, 1)); //epilogue header
    heap_list += 2*WSIZE;

    if (extend_heap(CHUNKSIZE) == NULL) { //alloc heap
        return -1;
    }
    return 0;//return
}

/*
 * mm_malloc - allocate a block using the brk pointer.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      //asize is adjusted block size
    void *ptr = NULL;  //current block pointer
    
    //if given size is 0, no have to alloc 
    if (size == 0) return NULL;
    if (size > DSIZE) asize = ALIGN(size+DSIZE);
    else if (size <= DSIZE) asize = 2 * DSIZE;
    //find area to place the block
    ptr = find_fit(asize); 

    if ((ptr == NULL) && ((ptr = extend_heap(MAX(asize, CHUNKSIZE))) == NULL)) return NULL;
    //place block to ptr
    ptr = place(ptr, asize); 
    return ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0)); //setting header
    PUT(FTRP(ptr), PACK(size, 0)); //setting footer

    insert_list(ptr, size); //insert the block to free list
    coalesce(ptr);
}

/*
 * extend_heap - extend heap using mem_sbrk
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t asize;
    asize = ALIGN(words); //create asize

    if ((bp = mem_sbrk(asize)) == (void *)-1) return NULL;

    //add header, footer
    PUT(HDRP(bp), PACK(asize, 0)); //header
    PUT(FTRP(bp), PACK(asize, 0)); //footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); //epilouge header

    insert_list(bp, asize); //insert block to seg_list
    bp=coalesce(bp);
    return bp; //coalesce blocks
}

/*
 * coalesce - when free blocks appear, then colaesce blocks
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp))); // alloc info of prev block
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // alloc info of next block
    size_t size = GET_SIZE(HDRP(bp)); //block pointer size
    
    //previous and next blocks are allocated
    if (prev_alloc && next_alloc) {
        return bp; // no blocks to coalesce
    }

    //after upper part, at least one of previous or next block is free
    //coalesce the cur block to free block and call delete_list fn current block
    
    //allocated previous block but freed next block
    if (prev_alloc && !next_alloc) {
        delete_list(bp);
        delete_list(NEXT_BLKP(bp)); 
        size += GET_SIZE(HDRP((NEXT_BLKP(bp))));
        PUT(HDRP(bp), PACK(size, 0)); //update header
        PUT(FTRP(bp), PACK(size, 0)); //update footer
    } 
    //freed previous block but allocated next block
    else if (!prev_alloc && next_alloc) {
        delete_list(bp);
        delete_list(PREV_BLKP(bp)); 
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); 
        PUT(FTRP(bp), PACK(size, 0)); // update footer
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // update header
        bp = PREV_BLKP(bp); //prev block pointer
    } 
    //previous and next blocks are freed
    else {
        delete_list(bp);
        delete_list(PREV_BLKP(bp)); 
        delete_list(NEXT_BLKP(bp)); 
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); //update header
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); //update footer
        bp = PREV_BLKP(bp); //prev block pointer
    }
    insert_list(bp, size); //insert block to list
    return bp; // return
}

/*
 * places the block of given size to the given pointer
 */
static void *place(void *ptr, size_t asize)
{
    size_t bsize = GET_SIZE(HDRP(ptr));
    delete_list(ptr); //delete the block

    //2*DSIZE >= min size
    if (2*DSIZE >= bsize - asize) {  
        PUT(HDRP(ptr), PACK(bsize, 1)); // update header
        PUT(FTRP(ptr), PACK(bsize, 1)); // update footer
        return ptr; // return
    }
    //asize<100
     else if (asize < 100) {
        PUT(HDRP(ptr), PACK(asize, 1)); //header
        PUT(FTRP(ptr), PACK(asize, 1)); //footer
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(bsize - asize, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(bsize - asize, 0));
        insert_list(NEXT_BLKP(ptr), bsize - asize); //insert
        return ptr; // return
    }
    //otherwise
    else {
        PUT(HDRP(ptr), PACK(bsize - asize, 0)); //header
        PUT(FTRP(ptr), PACK(bsize - asize, 0)); //footer
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(asize, 1));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(asize, 1));
        insert_list(ptr, bsize - asize); //insert
        return NEXT_BLKP(ptr); // return
    }
}

/*
 * find_fit - find block in seg_list by best_fit
 */
static void *find_fit(size_t asize)
{
    size_t size = asize;
    void *ptr;
    int i;
    //search free block in seg_list
    for (i=0; i<32; i+=1) {
        if ((seg_list[i] != NULL) && (size <= 1)) {
            ptr = seg_list[i];
            //pass blocks so small or marked realloc bit
            while ((ptr!=NULL) && (asize>GET_SIZE(HDRP(ptr)) || (GET(HDRP(ptr)) & 0x2) != 0))
                ptr = GET_PREV(ptr);
            if (ptr != NULL)
                break;
        }
        size>>=1;
    }
    return ptr;
}

/*
 * insert_list - insert block to seg_list
 */
static void insert_list(void *ptr, size_t size) 
{
    int i;
    void *old_ptr = NULL;
    void *new_ptr = NULL;

    for (i=0; (i < 31 ) && (size > 1); i+=1) 
        size >>= 1;

    old_ptr = seg_list[i];
    //find ptr to insert the free block
    while ((old_ptr != NULL) && (size > GET_SIZE(HDRP(old_ptr)))) {
        new_ptr = old_ptr;
        old_ptr = GET_PREV(old_ptr);
    }
    if (old_ptr == NULL) { //old_ptr is NULL
        SET_PREV(ptr, NULL);
        if (new_ptr == NULL) {
            SET_NEXT(ptr, NULL);
            seg_list[i] = ptr;
        }
        else if (new_ptr != NULL) {
            SET_NEXT(ptr, old_ptr);
            SET_PREV(old_ptr, ptr);
        } 
    }
    else if (old_ptr != NULL) { //old_ptr is not NULL
        SET_PREV(ptr, old_ptr);
        SET_NEXT(old_ptr, ptr);
        if (new_ptr == NULL) {
            SET_NEXT(ptr, NULL);
            seg_list[i] = ptr;
        }
        else if (new_ptr != NULL) {
            SET_NEXT(ptr, old_ptr);
            SET_PREV(old_ptr, ptr);
        }
    } 
}

/*
 * delete_list - delete block from seg_list
 */
static void delete_list(void *ptr) 
{
    int i;
    size_t size = GET_SIZE(HDRP(ptr));

    for (i=0; (i < 31) && (size > 1); i+=1) {
        size >>= 1;
    }
 
    if (GET_PREV(ptr) == NULL) {    //GET_PREV(ptr) is NULL
        if (GET_NEXT(ptr) == NULL) {
            seg_list[i] = NULL;
        }
        else if (GET_NEXT(ptr) != NULL) {
            SET_PREV(GET_NEXT(ptr), NULL);
        }
    }
    else if (GET_PREV(ptr) != NULL) {   //GET_PREV(ptr) is not NULL
        if (GET_NEXT(ptr) == NULL) {
            SET_NEXT(GET_PREV(ptr), NULL);
            seg_list[i] = GET_PREV(ptr);
        }
        else if (GET_NEXT(ptr) != NULL) {
            SET_NEXT(GET_PREV(ptr), GET_NEXT(ptr));
            SET_PREV(GET_NEXT(ptr), GET_PREV(ptr));
        }
    } 
}

/*
 * mm_realloc - consist by mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *new_ptr = ptr; //pointer returned
    size_t new_size = ALIGN(size+DSIZE)+32; //new block's size
    int remain; // remain size
    int check_buffer = GET_SIZE(HDRP(ptr)) - new_size;
    
    //pass that size is 0
    if (size == 0) {
	    mm_free(ptr);
	    return NULL;
    }
    else if(ptr==NULL) { //when ptr is NULL call malloc
        return mm_malloc(size);
    }
    else {
        //alloc extra space
        if ((check_buffer) < 0) {
            if ((GET_SIZE(HDRP(NEXT_BLKP(ptr)))) && (GET_ALLOC(HDRP(NEXT_BLKP(ptr))))) {
                new_ptr = mm_malloc(new_size - DSIZE);
                memcpy(new_ptr, ptr, MIN(size, new_size));
                mm_free(ptr);
                return new_ptr; //return ptr
            }
            // Check whether next block freed or epilogued
            else if (!(GET_SIZE(HDRP(NEXT_BLKP(ptr)))) || !(GET_ALLOC(HDRP(NEXT_BLKP(ptr))))) {
                remain = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - new_size;
                if (remain < 0) {
                    if (extend_heap(MAX(-remain, CHUNKSIZE)) == NULL)
                        return NULL;
                    remain += MAX(-remain, CHUNKSIZE);
                }
                delete_list(NEXT_BLKP(ptr));
                PUT(HDRP(ptr), PACK(new_size + remain, 1)); //header
                PUT(FTRP(ptr), PACK(new_size + remain, 1)); //footer
                return new_ptr; //return ptr
            }
            else {
                new_ptr = mm_malloc(size);//malloc with size
                if(new_ptr == NULL)
                    return NULL; //return 
                memcpy(new_ptr,ptr,GET_SIZE(HDRP(ptr))-DSIZE); //call memcpy 
                mm_free(ptr);   //free ptr
                return new_ptr;
            }
        }
    }
    return new_ptr; //return ptr
}