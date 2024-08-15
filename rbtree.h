typedef struct rb_node {
	struct rb_node *parent;
	struct rb_node *left;
	struct rb_node *right;
	int color;
} rb_node;

typedef struct rbtree {
	rb_node *root;
	rb_node *NIL;
} rbtree;