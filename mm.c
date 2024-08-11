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
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

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

typedef enum _BlockType { FREE, ALLOC } BlockType;

typedef struct _node {
	struct _node *prev;
	struct _node *next;
} node;

void *_find_match_bp(size_t);
void set_block_size(void *, size_t, BlockType);
void add_new_free_block(void *, size_t);

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define BSIZE 16

#define MAX(x, y) ((x) > (y) ? (x) : (y))

// about size and type of block
#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (unsigned int)(val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// about block
#define HDRP(bp) ((void *)(bp)-WSIZE)
#define FTPR(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define PREV_BLKP(bp) ((void *)(bp)-GET_SIZE(((void *)(bp)-DSIZE)))
#define NEXT_BLKP(bp) ((void *)(bp) + GET_SIZE(((void *)(bp)-WSIZE)))

#define CLASS_N 8
#define CLASS_SIZE(idx) (1 << (idx + 4))

// about linked list node
#define PREV(bp) (((node *)(bp))->prev)
#define NEXT(bp) (((node *)(bp))->next)

static node class_start[CLASS_N];
static node _NIL = {NULL, NULL};
static node *NIL = &_NIL;
static void *heap_start, *heap_end;

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
	for (int i = 0; i < CLASS_N; ++i) {
		class_start[i].next = NIL;
	}
	if ((heap_start = mem_sbrk(4 * WSIZE)) == (void *)-1) {
		return -1;
	}
	PUT(heap_start, 0);
	PUT(heap_start + (1 * WSIZE), PACK(DSIZE, ALLOC));
	PUT(heap_start + (2 * WSIZE), PACK(DSIZE, ALLOC));
	PUT(heap_start + (3 * WSIZE), PACK(0, FREE));
	heap_end = heap_start + (4 * WSIZE);

	return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
	size_t newsize = ALIGN(size + DSIZE), temp_size;
	void *cur_bp;
	node *prev_node, *next_node;
	if (newsize < BSIZE) {
		newsize = BSIZE;
	}

	cur_bp = _find_match_bp(newsize);

	if (cur_bp) {
		temp_size = GET_SIZE(HDRP(cur_bp)) - newsize;
		if (temp_size < BSIZE) {
			newsize = GET_SIZE(HDRP(cur_bp));
			temp_size = 0;
		}
		prev_node = PREV(cur_bp);
		next_node = NEXT(cur_bp);
		prev_node->next = next_node;
		next_node->prev = prev_node;
		set_block_size(cur_bp, newsize, ALLOC);
		if (temp_size) {
			void *new_free_bp = cur_bp + newsize;
			set_block_size(new_free_bp, temp_size, FREE);
			add_new_free_block(new_free_bp, temp_size);
		}
	} else {
		cur_bp = heap_end;
		if (GET_ALLOC(cur_bp - DSIZE)) {
			if (mem_sbrk(newsize) == (void *)-1) {
				return NULL;
			}
			heap_end += newsize;
		} else {
			temp_size = GET_SIZE(cur_bp - DSIZE);
			assert(newsize > temp_size);
			cur_bp -= temp_size;
			prev_node = PREV(cur_bp);
			next_node = NEXT(cur_bp);
			prev_node->next = next_node;
			next_node->prev = prev_node;
			if (mem_sbrk(newsize - temp_size) == (void *)-1) {
				return NULL;
			}
			heap_end += (newsize - temp_size);
		}
		set_block_size(cur_bp, newsize, ALLOC);
	}
	return cur_bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
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

void *_find_match_bp(size_t size) {
	node *cur_bp;
	for (int i = 0; i < CLASS_N; ++i) {
		if (i != CLASS_N - 1 && size > CLASS_SIZE(i)) {
			continue;
		}
		cur_bp = (class_start + i)->next;
		while (cur_bp != NIL) {
			if (GET_SIZE(HDRP(cur_bp)) >= size) {
				return cur_bp;
			}
			cur_bp = NEXT(cur_bp);
		}
	}

	return NULL;
}

void set_block_size(void *bp, size_t size, BlockType type) {
	PUT(HDRP(bp), PACK(size, type));
	PUT(FTPR(bp), PACK(size, type));
}

void add_new_free_block(void *bp, size_t size) {
	node *cur_bp = (node *)bp;
	int idx = 0;
	for (; idx < CLASS_N; ++idx) {
		if (size <= CLASS_SIZE(idx)) {
			break;
		}
	}
	node *head_node = class_start + idx;
	node *next_node = head_node->next;
	head_node->next = cur_bp;
	next_node->prev = cur_bp;
	cur_bp->prev = head_node;
	cur_bp->next = next_node;
}