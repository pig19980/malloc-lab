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
int get_class_idx(size_t);
void add_node(void *, int);

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define BSIZE 16

#define MAX(x, y) ((x) > (y) ? (x) : (y))

// about cidx and type of block
#define PACK(cidx, alloc) ((cidx << 1) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (unsigned int)(val))

#define GET_CIDX(p) (GET(p) >> 1)
#define GET_ALLOC(p) (GET(p) & 0x1)

// about block
#define HDRP(bp) ((void *)(bp)-DSIZE)
#define OFFSET(bp) (HDRP(bp) - heap_start)
#define IS_RIGHT(bp) ((OFFSET(bp)) & (1 << (GET_CIDX(HDRP(bp)) + 4)))

#define LEFT_BP(bp)                                                            \
	((void *)((unsigned)heap_start +                                           \
			  ((unsigned)(OFFSET(bp)) & ~(1 << (GET_CIDX(HDRP(bp)) + 4))) +    \
			  DSIZE))
#define RIGHT_BP(bp)                                                           \
	((void *)((unsigned)heap_start +                                           \
			  ((unsigned)(OFFSET(bp)) | (1 << (GET_CIDX(HDRP(bp))) + 4)) +     \
			  DSIZE))

#define CLASS_N 20
#define CLASS_SIZE(idx) (1 << (idx + 4))

// about linked list node
#define PREV(bp) (((node *)(bp))->prev)
#define NEXT(bp) (((node *)(bp))->next)

static node class_start[CLASS_N];
static node _NIL = {NULL, NULL};
static node *NIL = &_NIL;
static void *heap_start;
static int max_cidx;

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
	for (int i = 0; i < CLASS_N; ++i) {
		class_start[i].next = NIL;
	}
	max_cidx = 8;
	if ((heap_start = mem_sbrk(CLASS_SIZE(max_cidx))) == (void *)-1) {
		return -1;
	}
	PUT(heap_start, PACK(max_cidx, FREE));
	add_node(heap_start + 8, max_cidx);

	return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
	int cidx, pcidx;
	void *empty_bp;
	cidx = get_class_idx(size + 8);
	empty_bp = find_match_bp(cidx);
	if (!empty_bp) {
		while (true) {
			if ((empty_bp = mem_sbrk(CLASS_SIZE(max_cidx))) == (void *)-1) {
				return NULL;
			}
			PUT(empty_bp, PACK(max_cidx, FREE));
			add_node(empty_bp + 8, max_cidx);
			empty_bp += 8;
			max_cidx++;
			if (max_cidx > cidx) {
				break;
			}
		}
	}
	pcidx = GET_CIDX(HDRP(empty_bp));
	erase_node(empty_bp);
	while (cidx < pcidx) {
		pcidx--;
		PUT(HDRP(empty_bp), PACK(pcidx, FREE));
		PUT(HDRP(RIGHT_BP(empty_bp)), PACK(pcidx, FREE));
		add_node(RIGHT_BP(empty_bp), pcidx);
	}
	PUT(HDRP(empty_bp), PACK(cidx, ALLOC));
	return empty_bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
	int cidx;
	void *left_bp, *right_bp;
	cidx = GET_CIDX(HDRP(ptr));
	PUT(HDRP(ptr), PACK(cidx, FREE));
	add_node(ptr, cidx);
	while (cidx < max_cidx) {
		left_bp = LEFT_BP(ptr);
		right_bp = RIGHT_BP(ptr);
		// at least, one of both is FREE
		if (GET(HDRP(left_bp)) != GET(HDRP(right_bp))) {
			break;
		}
		cidx++;
		ptr = left_bp;
		erase_node(left_bp);
		erase_node(right_bp);
		PUT(HDRP(left_bp), PACK(cidx, FREE));
		add_node(left_bp, cidx);
	}

	return;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *old_bp, size_t size) {
	void *new_bp = mm_malloc(size);
	int new_size = CLASS_SIZE(GET_CIDX(HDRP(new_bp)));
	int old_size = CLASS_SIZE(GET_CIDX(HDRP(old_bp)));
	memcpy(new_bp, old_bp, old_size - DSIZE);
	mm_free(old_bp);
	return new_bp;
}

void *find_match_bp(size_t cidx) {
	node *cur_bp;
	for (int i = cidx; i < CLASS_N; ++i) {
		cur_bp = (class_start + i)->next;
		if (cur_bp != NIL) {
			return cur_bp;
		}
	}

	return NULL;
}

void erase_node(void *bp) {
	PREV(bp)->next = NEXT(bp);
	NEXT(bp)->prev = PREV(bp);
}

void add_node(void *bp, int cidx) {
	node *cur_node = bp;
	node *next_node = (class_start + cidx)->next;
	(class_start + cidx)->next = cur_node;
	cur_node->prev = class_start + cidx;
	cur_node->next = next_node;
	next_node->prev = cur_node;
}

void mem_check() {
	void *cur_bp = heap_start + DSIZE;
	void *heap_end = heap_start + CLASS_SIZE(max_cidx);
	node *cur_node;
	printf("\nblock check\n");
	while (cur_bp != heap_end + DSIZE) {
		printf("%d %d %d %d\n", OFFSET(cur_bp), GET_CIDX(HDRP(cur_bp)),
			   CLASS_SIZE(GET_CIDX(HDRP(cur_bp))), GET_ALLOC(HDRP(cur_bp)));
		cur_bp += CLASS_SIZE(GET_CIDX(HDRP(cur_bp)));
		assert(cur_bp <= heap_end + DSIZE);
	}
	printf("free check\n");
	for (int i = 0; i < CLASS_N; ++i) {
		printf("class %d min_size %d\n", i, CLASS_SIZE(i));
		cur_node = (class_start + i)->next;
		while (cur_node != NIL) {
			assert(GET_ALLOC(HDRP(cur_node)) == FREE);
			printf("%d %d %d\n", OFFSET(cur_node), GET_CIDX(HDRP(cur_node)),
				   GET_ALLOC(HDRP(cur_node)));
			cur_node = cur_node->next;
		}
	}
}

int get_class_idx(size_t size) {
	int class_idx = 0;
	while (1 << (class_idx + 4) < size) {
		class_idx++;
	}
	return class_idx;
}