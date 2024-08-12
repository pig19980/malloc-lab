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

void *find_match_bp(size_t);
void set_block_size(void *, size_t, BlockType);
void add_new_free_block(void *, size_t);
void erase_node(void *);
void mem_check();

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

#define CLASS_N 10
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
	heap_start += (2 * WSIZE);
	heap_end = heap_start + (2 * WSIZE);

	// printf("\ninit end\n");

	return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
	size_t new_size, left_size, rest_size;
	void *cur_bp;
	new_size = ALIGN(size + DSIZE);
	if (new_size < BSIZE) {
		new_size = BSIZE;
	}

	cur_bp = find_match_bp(new_size);

	if (cur_bp) {
		// printf("malloc found empty\n");
		rest_size = GET_SIZE(HDRP(cur_bp)) - new_size;
		if (rest_size < BSIZE) {
			new_size = GET_SIZE(HDRP(cur_bp));
			rest_size = 0;
		}
		// printf("%d %d\n", new_size, temp_size);
		erase_node(cur_bp);
		set_block_size(cur_bp, new_size, ALLOC);
		if (rest_size) {
			void *new_free_bp = cur_bp + new_size;
			set_block_size(new_free_bp, rest_size, FREE);
			add_new_free_block(new_free_bp, rest_size);
		}
	} else {
		cur_bp = heap_end;
		if (GET_ALLOC(cur_bp - DSIZE)) {
			if (mem_sbrk(new_size) == (void *)-1) {
				return NULL;
			}
			heap_end += new_size;
		} else {
			left_size = GET_SIZE(cur_bp - DSIZE);
			// assert(new_size > left_size);
			cur_bp -= left_size;
			erase_node(cur_bp);
			if (mem_sbrk(new_size - left_size) == (void *)-1) {
				return NULL;
			}
			heap_end += (new_size - left_size);
		}
		set_block_size(cur_bp, new_size, ALLOC);
	}
	// printf("got malloc %d\n", size);
	// mem_check();
	return cur_bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
	// assert(GET_SIZE(HDRP(ptr)) == GET_SIZE(FTPR(ptr)));
	// assert(GET_ALLOC(HDRP(ptr)) == ALLOC);
	// assert(GET_ALLOC(HDRP(ptr)) == GET_ALLOC(FTPR(ptr)));
	void *temp_bp, *free_bp;
	size_t free_size = GET_SIZE(HDRP(ptr));
	free_bp = ptr;
	if (ptr - DSIZE > heap_start && !GET_ALLOC(ptr - DSIZE)) {
		temp_bp = ptr - GET_SIZE(ptr - DSIZE);
		erase_node(temp_bp);
		free_bp = temp_bp;
		free_size += GET_SIZE(HDRP(temp_bp));
	}
	if (ptr + GET_SIZE(HDRP(ptr)) < heap_end &&
		!GET_ALLOC(HDRP(ptr + GET_SIZE(HDRP(ptr))))) {
		temp_bp = ptr + GET_SIZE(HDRP(ptr));
		erase_node(temp_bp);
		free_size += GET_SIZE(HDRP(temp_bp));
	}
	set_block_size(free_bp, free_size, FREE);
	add_new_free_block(free_bp, free_size);

	// printf("got free\n");
	// mem_check();
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *old_bp, size_t size) {
	void *new_bp, *rest_bp, *temp_bp;
	size_t copy_size, old_size, rest_size, temp_size;
	if (old_bp == NULL) {
		return mm_malloc(size);
	}
	if (size == 0) {
		mm_free(old_bp);
		return NULL;
	}

	old_size = GET_SIZE(HDRP(old_bp));
	copy_size = ALIGN(size + DSIZE);
	if (old_size == copy_size) {
		return old_bp;
	} else if (old_size > copy_size) {
		rest_size = old_size - copy_size;
		if (rest_size < BSIZE) {
			return old_bp;
		}
		rest_bp = old_bp + copy_size;
		set_block_size(old_bp, copy_size, ALLOC);
		// for the case when right block of new_bp if free block
		set_block_size(rest_bp, rest_size, ALLOC);
		mm_free(rest_bp);
		return old_bp;
	}

	temp_bp = NEXT_BLKP(old_bp);
	if (temp_bp == heap_end) {
		if (mem_sbrk(copy_size - old_size) == (void *)-1) {
			return NULL;
		}
		heap_end += (copy_size - old_size);
		set_block_size(old_bp, copy_size, ALLOC);
		return old_bp;

	} else if (!GET_ALLOC(HDRP(temp_bp))) {
		temp_size = GET_SIZE(HDRP(temp_bp));
		if (old_size + temp_size >= copy_size ||
			NEXT_BLKP(temp_bp) == heap_end) {
			erase_node(temp_bp);
			set_block_size(old_bp, old_size + temp_size, ALLOC);

			return mm_realloc(old_bp, size);
		}
	}
	if (old_bp - DSIZE > heap_start && !GET_ALLOC(old_bp - DSIZE)) {
		temp_bp = old_bp - GET_SIZE(old_bp - DSIZE);
		temp_size = old_size + GET_SIZE(old_bp - DSIZE);
		if (old_bp + GET_SIZE(HDRP(old_bp)) < heap_end &&
			!GET_ALLOC(HDRP(old_bp) + old_size)) {
			temp_size += GET_SIZE(HDRP(old_bp) + old_size);
		}
		if (temp_size >= copy_size) {
			erase_node(temp_bp);
			if (old_bp + GET_SIZE(HDRP(old_bp)) < heap_end &&
				!GET_ALLOC(HDRP(old_bp) + old_size)) {
				erase_node(old_bp + GET_SIZE(HDRP(old_bp)));
			}
			memcpy(temp_bp, old_bp, old_size - DSIZE);
			set_block_size(temp_bp, temp_size, ALLOC);

			return mm_realloc(temp_bp, size);
		}
	}

	new_bp = mm_malloc(size);
	memcpy(new_bp, old_bp, old_size - DSIZE);
	mm_free(old_bp);

	return new_bp;
}

void *find_match_bp(size_t size) {
	node *cur_bp;
	for (int i = 0; i < CLASS_N; ++i) {
		if (i != CLASS_N - 1 && size >= CLASS_SIZE(i + 1)) {
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

void erase_node(void *bp) {
	PREV(bp)->next = NEXT(bp);
	NEXT(bp)->prev = PREV(bp);
}

void set_block_size(void *bp, size_t size, BlockType type) {
	PUT(HDRP(bp), PACK(size, type));
	PUT(FTPR(bp), PACK(size, type));
}

void add_new_free_block(void *bp, size_t size) {
	node *cur_bp = (node *)bp;
	int idx = 0;
	for (; idx < CLASS_N - 1; ++idx) {
		if (size < CLASS_SIZE(idx + 1)) {
			break;
		}
	}
	// printf("add new free size %d class %d\n", size, idx);
	node *head_node = class_start + idx;
	node *next_node = head_node->next;
	head_node->next = cur_bp;
	next_node->prev = cur_bp;
	cur_bp->prev = head_node;
	cur_bp->next = next_node;
}

void mem_check() {
	void *cur_bp = heap_start;
	node *cur_node;

	// printf("block check\n");
	while (cur_bp != heap_end) {
		assert(GET(HDRP(cur_bp)) == GET(FTPR(cur_bp)));
		// printf("%d %d %d\n", cur_bp - heap_start, GET_SIZE(HDRP(cur_bp)),
		// 	   GET_ALLOC(HDRP(cur_bp)));
		cur_bp += GET_SIZE(HDRP(cur_bp));
		assert(cur_bp <= heap_end);
	}
	// printf("free check\n");
	for (int i = 0; i < CLASS_N; ++i) {
		// printf("class %d min_size %d\n", i, CLASS_SIZE(i));
		cur_node = (class_start + i)->next;
		while (cur_node != NIL) {
			assert(GET_ALLOC(HDRP(cur_node)) == FREE);
			assert(GET(HDRP(cur_node)) == GET(FTPR(cur_node)));
			// printf("%d %d %d\n", (void *)cur_node - heap_start,
			// 	   GET_SIZE(HDRP(cur_node)), GET_ALLOC(HDRP(cur_node)));
			cur_node = cur_node->next;
		}
	}
}