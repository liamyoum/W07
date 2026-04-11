/*
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

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4 /* Word Size (Header/Footer Size) (bytes)*/
#define DWSIZE 8 /* Double Word Size (bytes) */
#define CHUNKSIZE (1<<12) /* 1을 왼쪽으로 12비트 밀라는 뜻, 4096바이트. Heap 확장 기본 단위로 4KB */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc)) // 비트 연산자 |(OR) 사용, 두 비트 비교해서 하나라도 1이면 1이기에, 이 둘을 한 숫자로 합치는 것. 1 word에 size랑 alloc bit 넣어주는 놈

/* Read and write a word at address p */
#define GET(p)          (*(unsigned int *)(p)) // p를 unsigned int * 타입 포인터로 간주하고, 그 포인터가 가리키는 메모리 위치의 값을 꺼낸다
#define PUT(p, val)     (*(unsigned int *)(p) = (val)) // 그 메모리 위치의 값을 val로 교체

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~0x7) // 비트 연산자 &(AND) 사용, 두 비트 비교해서 둘 다 1이어야만 1. ~0x7은 16진수 7을 비트 뒤집은거고, 이진수로는 111...1000 이라서, 마지막 3비트만 0이고 나머지는 다 1인 마스크. -> 마지막 3비트 없애고 size만 꺼내겠다.
#define GET_ALLOC(p)    (GET(p) & 0x1) // 헤더 값에서 마지막 1비트(0x1)만 남겨 alloc 상태 확인

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE) // header 주소로 가기
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DWSIZE) // footer 주소로 가기

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(HDRP(bp))) // 다음 블록 payload 주소로 가기
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DWSIZE))) // 이전 블록 payload 주소로 가기

/* Static */
static char *heap_listp;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   
    heap_listp = mem_sbrk(4 * WSIZE);

    // if initialization fails
    if (heap_listp == (void *)-1) {
        return -1;
    }
    
    /* Create the initial empty heap */

    PUT(heap_listp, 0);                             /* Alignment Padding */
    PUT(heap_listp + WSIZE, PACK(DWSIZE, 1));       /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DWSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));      /* Epilogue header */
    heap_listp += DWSIZE;   // 이제 payload 앞, 여기서는 Prologue header와 footer 사이에 있음.

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    /* 왜 CHUNKSIZE를 WSIZE로 나눠주는 걸까? -> extend_heap이 인자로 words를 받음. 그래서 4096을 WSIZE인 4로 나눠주면, 워드가 1024개. 
       왜 그렇게 하는데? -> CHUNKSIZE 그대로 받으면 Bytes를 그대로 받는건데, extend_heap 내부 함수 구현을 보면 8 기준 alignment 하기 위해 홀수 / 짝수 분기 나눠 처리해줌. 바이트 그대로 받으면 8의 배수 맞추기 위해서 더 많은 조건 분기가 필요하고, 귀찮아질 것 */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }
    
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
        return NULL;
    else
    {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
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
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    bp = mem_sbrk(size); /* 새로 확장된 free block의 payload 주소*/
    if (bp == (void *)-1) {
        return NULL;
    }

    /* Initialize free block header/footer and the new epilogue header */
    PUT(HDRP(bp), PACK(size, 0));               /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));               /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));       /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

static void *coalesce(void *bp) {
    size_t prev_alloc_bit = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc_bit = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* Case 1: Both are already allocated */
    if (prev_alloc_bit && next_alloc_bit) {
        return bp;
    }
    /* Case 2: Prev is allocated and next is free */
    else if (prev_alloc_bit && !next_alloc_bit) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0)); // 이미 윗 줄에서 header가 갱신되었기 때문에, 이렇게 써도 NEXT_BLKP footer가 있던 주소에 가서 새로 쓸 수 있음.
    }
    /* Case 3: Prev is free and next is allocated */
    else if (!prev_alloc_bit && next_alloc_bit) {
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size ,0));
        bp = PREV_BLKP(bp);
    }
    /* Case 4: Both are already free */
    else {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *find_fit(size_t asize) {
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
}