#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    "swjungle8-week5-7",
    "KIM JOOYOUNG",
    "jujulia3040@gmail.com",
    "",
    ""
};

/* single word (4) or double word (8) alignment */
// #define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
// #define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// #define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//가용 리스트 조작을 위한 기본 상수와 매크로

/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE   (1<<6) /* Extend heap (bytes) */

#define MAX(x, y) ((x) > (y)? (x):(y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p)          (*(unsigned int*)(p))
#define PUT(p, val)     (*(unsigned int*)(p) = (val))

#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

#define HDRP(bp)        ((char *)(bp) - WSIZE)                          /* block header */
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)     /* footer header */

#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


#define GET_PRED(bp)    (*(void **)(bp))
#define GET_SUCC(bp)    (*(void **)((char *)(bp) + WSIZE))

static char *heap_listp;  // 처음에 쓸 큰 가용블록 힙을 만들어줌.

//추가 코드
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void set_no_free_block(void *bp);
static void add_free_block(void *bp);


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(8*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0); /* Alignment padding */

    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */

    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */

    PUT(heap_listp + (3*WSIZE), PACK(4*WSIZE, 0)); //header

    PUT(heap_listp + (4*WSIZE), NULL);

    PUT(heap_listp + (5*WSIZE), NULL);

    PUT(heap_listp + (6*WSIZE), PACK(4*WSIZE, 0)); //footer
    
    PUT(heap_listp + (7*WSIZE), PACK(0, 1)); /* Epilogue header */

    heap_listp += (4*WSIZE); //첫 가용 블록의 pred 가리키기

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    // explicit-LIFO : headp init
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize; /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;
    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}



/* 
* extend_heap - heap 
*/
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
    PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
    
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    if (prev_alloc && next_alloc) { /* Case 1 */
        add_free_block(bp);
        return bp;
    }
    else if (prev_alloc && !next_alloc) { /* Case 2 */
        set_no_free_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        
    }
    else if (!prev_alloc && next_alloc) { /* Case 3 */
        set_no_free_block(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);

    }
    else { /* Case 4 */
        set_no_free_block(NEXT_BLKP(bp));
        set_no_free_block(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    // explicit-LIFO : 새로 들어간 bp 세팅
    add_free_block(bp);
    return bp;
}

static void *find_fit(size_t asize)
{
    void *bp;
    for (bp=heap_listp; bp!=NULL; bp=GET_SUCC(bp))
    {
        if (GET_SIZE(HDRP(bp)) >= asize)
            return bp;
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    
    if ((csize - asize) >= (2*DSIZE)) {
        set_no_free_block(bp); //가용 리스트에서 빼주기
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        
        bp = NEXT_BLKP(bp);
        add_free_block(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    else {
        set_no_free_block(bp);
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static void set_no_free_block(void *bp)
{
    //가용 리스트에서 빼주기
    if (bp == heap_listp) {
        heap_listp = GET_SUCC(bp);
        return;
    } else {
        GET_SUCC(GET_PRED(bp)) = GET_SUCC(bp);
    }
    if (GET_SUCC(bp) != NULL) GET_PRED(GET_SUCC(bp)) = GET_PRED(bp);
}

static void add_free_block(void *bp)
{
    GET_SUCC(bp) = heap_listp;
    //가용 리스트 요소가 한개일 경우
    if (heap_listp != NULL){
        GET_PRED(heap_listp) = bp;
    }
    heap_listp = bp;
}