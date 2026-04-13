/*
 * mm.c - The fastest, least memory-efficient malloc package.
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
    "Team 4",
    /* First member's full name */
    "Liam Youm",
    /* First member's email address */
    "waiting4corona@gmail.com",
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

/* Macros for explicit free list */
#define PTRSIZE         sizeof(char *)
#define MINBLOCKSIZE    (WSIZE + PTRSIZE + PTRSIZE + WSIZE)
/* 현재 free block 안의 이전 / 다음 free block 주소를 담고 있는 포인터 */
#define PRED_PTR(bp)    ((char *)(bp))
#define SUCC_PTR(bp)    ((char *)(bp) + PTRSIZE)
/* 현재 free block의 이전 / 다음 free block 포인터 값 */
#define PRED(bp)        (*(char **)(PRED_PTR(bp))) // PRED_PTR이 char ** 타입이고, 그 값(주소)을 *로 꺼내겠다는 뜻
#define SUCC(bp)        (*(char **)(SUCC_PTR(bp)))

/* Static */
static char *heap_listp;
static char *free_listp;
// static char *rover; // for next_fit strategy
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void insert_free_block(void *bp);
static void remove_free_block(void *bp);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   
    heap_listp = mem_sbrk(4 * WSIZE);
    free_listp = NULL;

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

    // rover = heap_listp; // for next_fit strategy
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
    size_t asize;           /* Adjusted block size */
    size_t extendsize;      /* Amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size == 0) {
        return NULL;
    }
    /* Adjust block size to include overhead and alignment requests */
    asize = DWSIZE * ((size + DWSIZE + (DWSIZE - 1)) / DWSIZE); // payload 크기에 overhead(header & footer)를 더한 뒤 8의 배수로 올림하는 공식, 할당할 블록 찾는거라서 PRED_PTR, SUCC_PTR 크기까지 고려 안해도 됨 여기서는
    if (asize < MINBLOCKSIZE) {
        asize = MINBLOCKSIZE;
        // explicit free list에서 free block은 pred/succ 포인터를 저장해야 하므로
        // block 크기는 최소 MINBLOCKSIZE(24 bytes) 이상이어야 한다.
    }

    /* Search the free list for a fit */
    bp = find_fit(asize);
    if (bp != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    bp = extend_heap(extendsize / WSIZE);
    if (bp == NULL) {
        return NULL;
    }
    place(bp, asize);
    return bp;
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
 * mm_realloc - 기존 데이터를 최대한 보존하면서 block 크기 변경
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *newptr;
    size_t copySize;

    /* 기존 block이 없다는 뜻이니, 새로 할당하면 됨 */
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    /* 새 크기가 0이면, 기존 block을 유지할 이유가 없으니 해제 */
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    
    /* 새 블록 크기 할당 받음 */
    newptr = mm_malloc(size);
    /* 새 할당이 실패하면 old block은 절대 건드리면 안됨 */
    if (newptr == NULL) {
        return NULL;
    }

    /* 복사할 크기 계산, old block의 payload 최대 크기(header와 footer 제외) 구하기 */
    copySize = GET_SIZE(HDRP(ptr)) - DWSIZE;

    /* copySize가 원래 old block의 payload인데, 재할당하려는 size가 그것보다 작으면, copySize에 새로운 size를 덮어써야함. */
    if (size < copySize) {
        copySize = size;
    }

    /* 기존 payload 내용을 새 payload로 옮기기, header footer 안 건드리고 순수 데이터만 복사 */
    /* 즉, 이럴 경우 old payload의 데이터 중에서 새로 할당 받을 크기만큼의 앞 데이터만 보존 */
    memcpy(newptr, ptr, copySize);
    /* old block free */
    mm_free(ptr);
    /* return new pointer */
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
        insert_free_block(bp);
    }
    /* Case 2: Prev is allocated and next is free */
    else if (prev_alloc_bit && !next_alloc_bit) {
        remove_free_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0)); // 이미 윗 줄에서 header가 갱신되었기 때문에, 이렇게 써도 NEXT_BLKP footer가 있던 주소에 가서 새로 쓸 수 있음.
        insert_free_block(bp);
    }
    /* Case 3: Prev is free and next is allocated */
    else if (!prev_alloc_bit && next_alloc_bit) {
        remove_free_block(PREV_BLKP(bp));
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size ,0));
        bp = PREV_BLKP(bp);
        insert_free_block(bp);
    }
    /* Case 4: Both are already free */
    else {
        remove_free_block(PREV_BLKP(bp));
        remove_free_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        insert_free_block(bp);
    }

    // /* coalesce 이후, next_fit에서 사용하는 rover가 payload의 내부 어딘가를 가리키고 있을 수 있어서, 조건 확인 후 병합된 블록의 payload 시작점으로 rover를 옮겨준다. */
    // if ((rover > (char *)bp) && (rover < NEXT_BLKP(bp))) {
    //     rover = bp;
    // }

    return bp;
}

static void *find_fit(size_t asize) {
    char *bp = free_listp;
    
    /* First Fit */
    while (bp != NULL) {
        if (GET_SIZE(HDRP(bp)) >= asize) {
            return bp;
        }
        else {
            bp = SUCC(bp);
        }
    }
    return NULL;

    // /* Next fit */
    // char *old_rover = rover;
    // /* 1차 탐색, rover부터 block 끝까지 */
    // while (GET_SIZE(HDRP(rover)) > 0) {
    //     if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover)))) {
    //         bp = rover;
    //         rover = NEXT_BLKP(rover);
    //         return bp;
    //     }
    //     rover = NEXT_BLKP(rover);
    // }

    // /* 2차 탐색, block 처음부터 rover 전까지*/
    // rover = heap_listp;
    // while (rover < old_rover) {
    //     if (!(GET_ALLOC(HDRP(rover))) && (asize <= GET_SIZE(HDRP(rover)))) {
    //         bp = rover;
    //         rover = NEXT_BLKP(rover);
    //         return bp;
    //     }
    //     rover = NEXT_BLKP(rover);
    // }

    // /* next_fit으로 못 찾았음 */
    // rover = old_rover;
    return NULL;
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    remove_free_block(bp); // free block list에서 해당 블록 제거해줌

    // 만약 요청 들어온 크기를 현재 블록에 할당했음에도 남은 크기가 최소 블록 크기인 24바이트 이상이면
    if ((csize - asize) >= (MINBLOCKSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp); // 요청 받은 asize 만큼 앞으로 이동하고
        PUT(HDRP(bp), PACK(csize - asize, 0)); // 그 자리에서 새로 header 써준다. 이전 블록 - 요청 받은 크기 (최소 16) 만큼으로.
        PUT(FTRP(bp), PACK(csize - asize, 0));

        insert_free_block(bp);
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1)); // 그게 아니면 걍 블록 전체 크기로 alloc bit만 바꿔준다.
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* 새 free block을 리스트 맨 앞(head)에 넣기 */
static void insert_free_block(void *bp) {
    char *current_head = free_listp;

    SUCC(bp) = current_head;
    PRED(bp) = NULL;

    if (current_head != NULL) {
        PRED(current_head) = bp;
    }

    free_listp = bp;
}

static void remove_free_block(void *bp) {
    char *prev = PRED(bp);
    char *next = SUCC(bp);

    if (prev != NULL) {
        SUCC(prev) = next;
    } else {
        free_listp = next; // prev가 NULL일 경우 맨 앞 블록을 제거하는 경우가 됨.
    }
    
    if (next != NULL) {
        PRED(next) = prev;
    }
}