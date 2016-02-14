#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Yadda yadda
#define MAIN
#include "state.c"
#include "dict.c"
#include "node.c"

// Blah blah.
// gcc -std=gnu99 -Wall -O3 graph.c -o graph; graph 4 4

#define PASS (1)
#define LEAF (2)
#define KO_SHIFT (3)
#define KEY_MUL (33)

static int* pattern3_weights;
static int* pattern3_corner_weights;
static int* pattern3_edge_weights;

typedef struct tree_node
{
    signed char move_index;
    value_t low;
    value_t high;
    unsigned char num_children;
    struct tree_node *children;
} tree_node;

typedef struct tree
{
    int num_moves;
    move_info move_infos[STATE_SIZE + 1];
    state *root_state;
    tree_node *root_node;
    int depth;
    vertex *root;
    size_t num_keys;
    size_t key_set[STATE_SIZE + 1];
} tree;

typedef struct history
{
    size_t key;
    struct history *past;
} history;

void print_tree_node(tree_node *n) {
    printf("%d: low = %d high = %d children = %d\n", n->move_index, n->low, n->high, n->num_children);
}

int negamax_tree_node(tree_node *n) {
    value_t low = VALUE_MIN;
    value_t high = VALUE_MIN;
    for (int i = 0; i < n->num_children; i++) {
        if (-n->children[i].high > low) {
            low = -n->children[i].high;
        }
        if (-n->children[i].low > high) {
            high = -n->children[i].low;
        }
    }
    int changed = low != n->low || high != n->high;
    n->low = low;
    n->high = high;
    return changed;
}

int key_in_history(history *h, size_t key) {
    if (h == NULL) {
        return 0;
    }
    // printf("%zu ?= %zu\n", key, h->key);
    if (key == h->key) {
        return 1;
    }
    return key_in_history(h->past, key);
}

history* key_to_history(history *h, size_t key) {
    history *present = malloc(sizeof(history));
    present->key = key;
    present->past = h;
    return present;
}

int key_in_set_t(tree *t, size_t key) {
    for (size_t i = 0; i < t->num_keys; i++) {
        if (t->key_set[i] == key) {
            return 1;
        }
    }
    t->key_set[t->num_keys++] = key;
    return 0;
}

/*
void from_key_t(graph *t, state *s, size_t key) {
    s->playing_area = t->root_state->playing_area;
    size_t ko_part = key % KEY_MUL;
    key /= KEY_MUL;
    assert(from_key(s, key));
    if (ko_part < KO_SHIFT) {
        s->passes = ko_part;
    }
    else if (ko_part) {
        s->ko = 1ULL << (ko_part - KO_SHIFT);
    }
    assert(!s->ko || s->ko & s->playing_area);
    assert(!s->ko || s->opponent);
}
*/

size_t to_key_t(state *s) {
    assert(s->passes <= 2);
    size_t key = to_key(s) * KEY_MUL + s->passes;
    if (s->ko) {
        key += KO_SHIFT + bitscan(s->ko);
    }
    return key;
}

void init_tree(tree *t, state *s) {
    t->root_state = s;
    t->num_moves = get_move_infos(s, t->move_infos);
    size_t key_max = (max_key(s) + 1) * KEY_MUL;
    t->depth = 0;
    while (key_max) {
        key_max /= 2;
        t->depth++;
    }
    t->root = (vertex*) calloc(1, sizeof(vertex));
    t->root_node = malloc(sizeof(tree_node));
    *(t->root_node) = (tree_node) {-2, VALUE_MIN, VALUE_MAX, 0, NULL};
}

void use_heuristic_move(tree *t, state *s) {
#ifdef RANDOM_HEURISTIC
    size_t n = t->num_moves;
    for (size_t i = 0; i < n - 1; i++) {
      size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
      move_info temp = t->move_infos[j];
      t->move_infos[j] = t->move_infos[i];
      t->move_infos[i] = temp;
    }
#else
    sort_move_infos(s, t->move_infos, t->num_moves, pattern3_weights, pattern3_corner_weights, pattern3_edge_weights);
#endif
}

void make_children(tree *t, state *s, tree_node *n) {
    state child_;
    state *child = &child_;
    n->children = (tree_node*) malloc(STATE_SIZE * sizeof(tree_node));
    state_info si = get_state_info(s);
    t->num_keys = 0;
    use_heuristic_move(t, s);
    for (int i = 0; i < t->num_moves; i++) {
        *child = *s;
        if (make_move_plus(child, si, t->move_infos[i].move)) {
            // printf("asdkjasdjk\n");
            // print_stones(t->move_infos[i].move);
             canonize_plus(child);
            size_t child_key = to_key_t(child);
            if (key_in_set_t(t, child_key)) {
                 continue;
            }
            n->children[n->num_children++] = (tree_node) {t->move_infos[i].index, VALUE_MIN, VALUE_MAX, 0, NULL};
        }
    }
    n->children = (tree_node*) realloc(n->children, n->num_children * sizeof(tree_node));
}

void do_the_thing(tree *t, state *s, tree_node *n, history *h, int depth, int mode) {
    // print_state(s);
    if (s->passes >= 2) {
        value_t score = liberty_score_plus(s);
        n->low = score;
        n->high = score;
        return;
    }
    if (depth == 0) {
        n->low = VALUE_MAX - 2;
        n->high = VALUE_MAX - 2;
        return;
    }
    state child_;
    state *child = &child_;
    *child = *s;
    canonize_plus(child);
    size_t key = to_key_t(child);
    if (key_in_history(h, key)) {
        n->low = VALUE_MAX - 1;
        n->high = VALUE_MAX - 1;
        return;
    }
    h = key_to_history(h, key);
    if (n->num_children == 0) {
        make_children(t, s, n);
    }
    if (mode == 0) {
        *child = *s;
        stones_t move = 0;
        if (n->children[0].move_index >= 0) {
            move = 1UL << n->children[0].move_index;
        }
        make_move(child, move);
        do_the_thing(t, child, n->children, h, depth - 1, 1);
    }
    else if (mode == 1) {
        for (int i = 0; i < n->num_children; i++) {
            *child = *s;
            stones_t move = 0;
            if (n->children[i].move_index >= 0) {
                move = 1UL << n->children[i].move_index;
            }
            assert(make_move(child, move));
            do_the_thing(t, child, n->children + i, h, depth - 1, 0);
        }
    }
    negamax_tree_node(n);
    free(h);
    // printf("%d -- ", mode);
    // print_tree_node(n);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s width height\n", argv[0]);
        return 0;
    }
    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    if (width <= 0) {
        printf("Width must be larger than 0.\n");
        return 0;
    }
    if (width >= 7) {
        printf("Width must be less than 7.\n");
        return 0;
    }
    if (height <= 0) {
        printf("Height must be larger than 0.\n");
        return 0;
    }
    if (height >= 6) {
        printf("Height must be less than 6.\n");
        return 0;
    }

    //srand(time(0));
    pattern3_weights = malloc(PATTERN3_SIZE * sizeof(int));
    pattern3_corner_weights = malloc(PATTERN3_SIZE * sizeof(int));
    pattern3_edge_weights = malloc(PATTERN3_SIZE * sizeof(int));

    FILE *f = fopen("pattern3_weights.dat", "rb");
    assert(fread(pattern3_weights, sizeof(int), PATTERN3_SIZE, f));
    fclose(f);
    f = fopen("pattern3_corner_weights.dat", "rb");
    assert(fread(pattern3_corner_weights, sizeof(int), PATTERN3_SIZE, f));
    fclose(f);
    f = fopen("pattern3_edge_weights.dat", "rb");
    assert(fread(pattern3_edge_weights, sizeof(int), PATTERN3_SIZE, f));
    fclose(f);

    state s_ = (state) {rectangle(width, height), 0, 0, 0, 0};
    state *s = &s_;

    tree t_;
    tree *t = &t_;

    init_tree(t, s);
    do_the_thing(t, s, t->root_node, NULL, 20, 0);
    do_the_thing(t, s, t->root_node, NULL, 20, 1);
    print_tree_node(t->root_node);
    for (int i = 0; i < t->root_node->num_children; i++) {
        printf(" "); print_tree_node(t->root_node->children + i);
    }
    return 0;
}
