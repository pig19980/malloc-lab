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
#include "rbtree.h"

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

typedef struct empty_node {
	struct empty_node *prev;
	struct empty_node *next;
} empty_node;

typedef struct rb_node {
	struct rb_node *parent;
	struct rb_node *left;
	struct rb_node *right;
	int color;
} rb_node;

int get_class_idx(size_t);
void *find_match_bp(size_t);

rb_node *new_rb_node();
rbtree *new_rbtree();

void *new_rbblock();
void *erase_rbblock();

void add_free_block(void *, size_t);
void erase_free_block(void *, size_t);

/*
 * Maximum heap size in bytes
 */
#define MAX_HEAP (20 * (1 << 20)) /* 20 MB */

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

#define CLASS_N 20
#define CLASS_SIZE(idx) (1 << (idx + 4))

// about block
#define HDRP(bp) ((void *)(bp)-DSIZE)
#define OFFSET(bp) (HDRP(bp) - heap_start)
#define IS_RIGHT(bp) ((OFFSET(bp)) & (CLASS_SIZE(GET_CIDX(HDRP(bp)))))

#define LEFT_BP(bp)                                                            \
	((void *)((unsigned)heap_start + DSIZE +                                   \
			  ((unsigned)(OFFSET(bp)) & ~(CLASS_SIZE(GET_CIDX(HDRP(bp)))))))
#define RIGHT_BP(bp)                                                           \
	((void *)((unsigned)heap_start + DSIZE +                                   \
			  ((unsigned)(OFFSET(bp)) | (CLASS_SIZE(GET_CIDX(HDRP(bp)))))))

static node class_start[CLASS_N];
static node _NIL = {NULL, NULL};
static node *NIL = &_NIL;
static void *heap_start;

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
	for (int i = 0; i < CLASS_N; ++i) {
		class_start[i].next = NIL;
	}
	if ((heap_start = mem_sbrk(0)) == (void *)-1) {
		return -1;
	}
	// printf("\n%p init_done\n\n", heap_start);
	return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
	int cidx, pcidx;
	void *empty_bp, *heap_end;
	cidx = get_class_idx(size + 8);

	// printf("malloc %d %d\n", size, cidx);

	empty_bp = find_match_bp(cidx);
	if (!empty_bp) {
		heap_end = mem_sbrk(0);
		for (int ccidx = 0; ccidx < cidx; ccidx++) {
			if ((heap_end - heap_start) & CLASS_SIZE(ccidx)) {
				if ((heap_end = mem_sbrk(CLASS_SIZE(ccidx))) == (void *)-1) {
					return NULL;
				}
				PUT(heap_end, PACK(ccidx, FREE));
				add_node(heap_end + DSIZE, ccidx);
				heap_end = mem_sbrk(0);
			}
		}
		if ((heap_end = mem_sbrk(CLASS_SIZE(cidx))) == (void *)-1) {
			return NULL;
		}
		PUT(heap_end, PACK(cidx, FREE));
		add_node(heap_end + DSIZE, cidx);
		empty_bp = heap_end + DSIZE;
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
	// mem_check();
	return empty_bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) { return; }

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *old_bp, size_t size) { return old_bp; }

int get_class_idx(size_t size) {
	int class_idx = 0;
	while (1 << (class_idx + 4) < size) {
		class_idx++;
	}
	return class_idx;
}

rb_node *new_rb_node() {
	rb_node *n = malloc(sizeof(rb_node));

	n->parent = NULL;
	n->left = NULL;
	n->right = NULL;
	n->color = 1;

	return n;
}

rbtree *new_rbtree() {
	rbtree *t = malloc(sizeof(rbtree));
	rb_node *nil_rb_node = malloc(sizeof(rb_node));

	nil_rb_node->parent = NULL;
	nil_rb_node->left = NULL;
	nil_rb_node->right = NULL;
	nil_rb_node->data = 0;
	nil_rb_node->color = 0;
	t->NIL = nil_rb_node;
	t->root = t->NIL;

	return t;
}