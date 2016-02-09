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

#define LOW_FINAL (1)
#define HIGH_FINAL (2)
#define PROVES_LOW (4)
#define PROVES_HIGH (8)
#define PROVES_HIGH_FINAL (16)
#define PROVES_LOW_FINAL (32)
#define TAG (128)

static int* pattern3_weights;
static int* pattern3_corner_weights;
static int* pattern3_edge_weights;

typedef struct graph_value
{
    value_t low;
    value_t high;
    unsigned char flags;
} graph_value;

typedef struct graph
{
    int num_moves;
    move_info move_infos[STATE_SIZE + 1];
    state *root_state;
    value_t max_score;
    int depth;
    vertex *root;
    size_t num_keys;
    size_t key_set[STATE_SIZE + 1];
} graph;

typedef struct key_item
{
    size_t key;
    unsigned char mode;
    struct key_item *next;
} key_item;

typedef struct key_queue
{
    key_item *front;
    key_item *back;
} key_queue;

void queue_push_back(key_queue *q, size_t key, int mode) {
    key_item *item = (key_item*) malloc(sizeof(key_item));
    item->key = key;
    item->mode = mode;
    item->next = NULL;
    if (q->back == NULL) {
        q->front = item;
        q->back = item;
    }
    else {
        q->back->next = item;
        q->back = item;
    }
}

void queue_push_front(key_queue *q, size_t key, int mode) {
    key_item *item = (key_item*) malloc(sizeof(key_item));
    item->key = key;
    item->mode = mode;
    item->next = q->front;
    if (q->back == NULL) {
        q->back = item;
    }
    q->front = item;
}

void queue_pop_front(key_queue *q, size_t *key, int *mode) {
    assert(q->front != NULL);
    *key = q->front->key;
    *mode = q->front->mode;
    key_item *front = q->front;
    q->front = front->next;
    if (q->front == NULL) {
        q->back = NULL;
    }
    free(front);
}

int queue_has_items(key_queue *q) {
    return q->front != NULL;
}

void queue_clear(key_queue *q) {
    while (q->front) {
        key_item *front = q->front;
        q->front = front->next;
        free(front);
    }
    q->back = NULL;
}

void print_graph_node(graph_value v) {
    printf("low = %d", v.low);
    if (v.flags & LOW_FINAL) {
        printf("f");
    }
    printf(" high = %d", v.high);
    if (v.flags & HIGH_FINAL) {
        printf("f");
    }
    if (v.flags & TAG) {
        printf(" tagged");
    }
    if (v.flags & PROVES_LOW) {
        printf(" p_low");
    }
    if (v.flags & PROVES_HIGH) {
        printf(" p_high");
    }
    if (v.flags & PROVES_HIGH_FINAL) {
        printf(" p_high_f");
    }
    if (v.flags & PROVES_LOW_FINAL) {
        printf(" p_low_f");
    }
    printf("\n");
}

graph_value negamax_value(graph_value parent, graph_value child) {;
    if (-child.high > parent.low) {
        parent.low = -child.high;
    }
    if (-child.low > parent.low) {
        // parent.low_final = parent.low_final && child.high_final;
        if (parent.flags & LOW_FINAL) {
            if (!(child.flags & HIGH_FINAL)) {
                parent.flags ^= LOW_FINAL;
            }
        }
    }
    if (-child.low > parent.high) {
        parent.high = -child.low;
        // parent.high_final = child.low_final;
        parent.flags &= ~HIGH_FINAL;
        if (child.flags & LOW_FINAL) {
            parent.flags |= HIGH_FINAL;
        }
    }
    else if (-child.low == parent.high){
        // parent.high_final = parent.high_final || child.low_final;
        if (child.flags & LOW_FINAL) {
            parent.flags |= HIGH_FINAL;
        }
    }
    if (parent.high == VALUE_MAX) {
        // assert(!parent.high_final);
        assert(!(parent.flags & HIGH_FINAL));
    }
    if (parent.low == VALUE_MIN) {
        // assert(!parent.low_final);
        assert(!(parent.flags & LOW_FINAL));
    }
    if (parent.low == parent.high) {
        // parent.low_final = 1;
        // parent.high_final = 1;
        parent.flags |= LOW_FINAL | HIGH_FINAL;
    }
    return parent;
}

int inequal_g(graph_value a, graph_value b) {
    if (a.low != b.low) {
        return 1;
    }
    if (a.high != b.high) {
        return 1;
    }
    // if (a.low_final != b.low_final) {
    //     return 2;
    // }
    // if (a.high_final != b.high_final) {
    //     return 2;
    // }
    if ((a.flags ^ b.flags) & (LOW_FINAL | HIGH_FINAL)) {
        return 2;
    }
    return 0;
}

int key_in_set(graph *g, size_t key) {
    for (size_t i = 0; i < g->num_keys; i++) {
        if (g->key_set[i] == key) {
            return 1;
        }
    }
    g->key_set[g->num_keys++] = key;
    return 0;
}

int graph_set(graph *g, size_t key, graph_value v, int protected) {
    graph_value *value = (graph_value*) malloc(sizeof(graph_value));
    assert(value);
    *value = v;
    void *old_value = btree_set(g->root, g->depth, key, (void*) value, protected);
    if (protected) {
        if (old_value != NULL) {
            free((void*) value);
            return 0;
        }
        return 1;
    }
    else {
        if (old_value != NULL) {
            int changed = inequal_g(v, *((graph_value*) old_value));
            free(old_value);
            return changed;
        }
        return 1;
    }
}

graph_value graph_get(graph *g, size_t key, graph_value default_) {
    graph_value *value;
    value = (graph_value*) btree_get(g->root, g->depth, key);
    if (value == NULL) {
        return default_;
    }
    return *value;
}

void init_graph(graph *g, state *s) {
    g->root_state = s;
    g->max_score = popcount(s->playing_area);
    // g->max_score = VALUE_MAX;
    g->num_moves = 1;
    g->move_infos[0] = (move_info) {0, -1, pass, 0, 0};
    int width = 0;
    int height = 0;
    dimensions(s->playing_area, &width, &height);
    width -= 1;
    height -= 1;
    for (int i = 0; i < STATE_SIZE; i++) {
        stones_t move = 1ULL << i;
        move_type type = center;
        int x = i % WIDTH;
        int y = i / WIDTH;
        if (x == 0) {
            if (y == 0) {
                type = corner_nw;
            }
            else if (y == height) {
                type = corner_sw;
            }
            else {
                type = edge_w;
            }
        }
        else if (x == width) {
            if (y == 0) {
                type = corner_ne;
            }
            else if (y == height) {
                type = corner_se;
            }
            else {
                type = edge_e;
            }
        }
        else if (y == 0) {
            type = edge_n;
        }
        else if (y == height) {
            type = edge_s;
        }
        if (move & s->playing_area) {
            g->move_infos[g->num_moves++] = (move_info) {move, i, type, 0, 0};
        }
    }
    size_t key_max = (max_key(s) + 1) * KEY_MUL;
    g->depth = 0;
    while (key_max) {
        key_max /= 2;
        g->depth++;
    }
    g->root = (vertex*) calloc(1, sizeof(vertex));
    graph_set(g, to_key(s) * KEY_MUL, (graph_value) {-g->max_score, g->max_score, 0}, 0);
}

void from_key_g(graph *g, state *s, size_t key) {
    s->playing_area = g->root_state->playing_area;
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

size_t to_key_g(state *s) {
    assert(s->passes <= 2);
    size_t key = to_key(s) * KEY_MUL + s->passes;
    if (s->ko) {
        key += KO_SHIFT + bitscan(s->ko);
    }
    return key;
}

graph_value negamax_node(graph *g, size_t key, int depth, int *changed) {
    state s_;
    state *s = &s_;
    state child_;
    state *child = &child_;
    from_key_g(g, s, key);
    if (s->passes >= 2) {
        value_t score = liberty_score_plus(s);
        return (graph_value) {score, score, LOW_FINAL | HIGH_FINAL};
    }
    if (s->passes) {
        depth++;
    }
    if (depth == 0) {
        return graph_get(g, key, (graph_value) {-g->max_score, g->max_score, 0});
    }
    graph_value v = (graph_value) {VALUE_MIN, VALUE_MIN, LOW_FINAL};
    state_info si = get_state_info(s);
    for (int i = 0; i < g->num_moves; i++) {
        *child = *s;
        graph_value child_v;
        if (make_move_plus(child, si, g->move_infos[i].move)) {
            canonize_plus(child);
            size_t child_key = to_key_g(child);
            child_v = negamax_node(g, child_key, depth - 1, NULL);
            v = negamax_value(v, child_v);
        }
    }
    // Some finality oscillations were seen during testing. Maybe they're gone now.
    // if (mode == 1) {
    //     v.low_final = v.low_final && old_v.low_final;
    //     v.high_final = v.high_final && old_v.high_final;
    // }
    if (changed != NULL) {
        *changed = graph_set(g, key, v, 0);
    }
    return v;
}

int negamax_graph(graph *g, int mode) {
    int changed = 0;
    void visit(size_t key, void *value) {
        int change;
        negamax_node(g, key, 1, &change);
        if (change == 2 && changed == 0) {
            changed = 2;
        }
        if (change == 1) {
            changed = 1;
        }
    }
    btree_traverse(g->root, g->depth, 0, visit);
    return changed;
}

void use_heuristic_move(graph *g, state *s) {
#ifdef RANDOM_HEURISTIC
    size_t n = g->num_moves;
    for (size_t i = 0; i < n - 1; i++) {
      size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
      move_info t = g->move_infos[j];
      g->move_infos[j] = g->move_infos[i];
      g->move_infos[i] = t;
    }
#else
    sort_move_infos(s, g->move_infos, g->num_moves, pattern3_weights, pattern3_corner_weights, pattern3_edge_weights);
#endif
}

void expand_node(graph *g, size_t key) {
    state s_ = (state) {g->root_state->playing_area, 0, 0, 0, 0};
    state *s = &s_;
    state child_;
    state *child = &child_;
    from_key_g(g, s, key);
    if (s->passes >= 2) {
        return;
    }
    state_info si = get_state_info(s);
    for (int i = 0; i < g->num_moves; i++) {
        *child = *s;
        if (make_move_plus(child, si, g->move_infos[i].move)) {
            canonize_plus(child);
            size_t child_key = to_key_g(child);
            graph_value v = (graph_value) {-g->max_score, g->max_score, 0};
            if (child->passes == 2){
                value_t score = liberty_score_plus(child);
                v = (graph_value) {score, score, LOW_FINAL | HIGH_FINAL};
            }
            graph_set(g, child_key, v, 1);
        }
    }
}

int add_tag(graph *g, size_t key) {
    graph_value *value;
    value = (graph_value*) btree_get(g->root, g->depth, key);
    if (value == NULL) {
        return 0;
    }
    int tagged = value->flags & TAG;
    value->flags |= TAG;
    return tagged;
}

void do_expand(graph *g, int mode) {
    size_t key = to_key_g(g->root_state);
    key_queue q_ = (key_queue) {NULL, NULL};
    key_queue *q = &q_;
    queue_push_back(q, key, mode);

    key_queue alt_q_ = (key_queue) {NULL, NULL};
    key_queue *alt_q = &alt_q_;

    state s_;
    state *s = &s_;
    state child_;
    state *child = &child_;

    unsigned long long int expansions = 0;
    while (1) {
        while (queue_has_items(q)) {
            queue_pop_front(q, &key, &mode);
            from_key_g(g, s, key);
            assert(s->passes < 2);
            //printf("%zu, %d, %d\n", key, mode, s->passes);
            if (mode == 1 || mode == 3) {
                if (add_tag(g, key)) {
                    continue;
                }
            }
            if (mode == 0 || mode == 2) {
                use_heuristic_move(g, s);
            }
            g->num_keys = 0;
            state_info si = get_state_info(s);
            // graph_value nothing = (graph_value) {VALUE_MAX, VALUE_MIN, 0};
            // graph_value v = graph_get(g, key, nothing);
            // assert(inequal_g(v, nothing));
            graph_value v = negamax_node(g, key, 0, NULL);
            int found_one = 0;
            for (int i = 0; i < g->num_moves; i++) {
                *child = *s;
                if (make_move_plus(child, si, g->move_infos[i].move)) {
                    canonize_plus(child);
                    size_t child_key = to_key_g(child);
                    if (key_in_set(g, child_key)) {
                        continue;
                    }
                    if (child->passes >= 2) {
                        continue;
                    }
                    // graph_value child_v = graph_get(g, child_key, (graph_value) {-g->max_score, g->max_score, 0});
                    graph_value child_v = negamax_node(g, child_key, 0, NULL);
                    if (mode == 0 && (child_v.flags & TAG)) {
                        continue;
                    }
                    if (mode == 1) {
                        if (child_v.low > VALUE_MIN) {
                            continue;
                        }
                    }
                    if (mode == 2 && (-child_v.low < v.high || (child_v.flags & TAG))) {
                        continue;
                    }
                    if (mode == 3) {
                        // if ((child_v.flags & HIGH_FINAL) || -child_v.low < v.low) {
                        //     // TODO: Find out why this fails.
                        //     continue;
                        // }
                        if (child_v.flags & HIGH_FINAL) {
                            continue;
                        }
                    }
                    if (graph_set(g, child_key, child_v, 1)) {
                        expansions++;
                    }
                    else {
                        int child_mode = -1;
                        if (mode == 1) {
                            child_mode = 0;
                        }
                        else if (mode == 0) {
                            child_mode = 1;
                        }
                        else if (mode == 2) {
                            child_mode = 3;
                        }
                        else if (mode == 3) {
                            child_mode = 2;
                        }
                        if (found_one) {
                            queue_push_front(alt_q, child_key, child_mode);
                        }
                        else {
                            queue_push_back(q, child_key, child_mode);
                            if (mode == 0 || mode == 2) {
                                found_one = 1;
                            }
                        }
                    }
                }
            }
        }
        if (expansions == 0) {
            queue_pop_front(alt_q, &key, &mode);
            queue_push_back(q, key, mode);
        }
        else {
            queue_clear(alt_q);
            break;
        }
    }
}

int expand(graph *g) {
    size_t key = to_key_g(g->root_state);
    graph_value v = graph_get(g, key, (graph_value) {-g->max_score, g->max_score, 0});
    if (v.low == VALUE_MIN) {
        do_expand(g, 0);
    }
    else if (v.high == VALUE_MAX) {
        do_expand(g, 1);
    }
    else if (!(v.flags & HIGH_FINAL)) {
        do_expand(g, 2);
    }
    else if (!(v.flags & LOW_FINAL)) {
        do_expand(g, 3);
    }
    else {
        return 0;
    }
    void visit(size_t key, void *value) {
        ((graph_value *) value)->flags &= ~TAG;
    }
    btree_traverse(g->root, g->depth, 0, visit);
    return 1;
}

void sporadic_expand(graph *g) {
    void visit(size_t key, void *value) {
        expand_node(g, key);
    }
    btree_traverse(g->root, g->depth, 0, visit);
}

void assume_final(graph *g) {
    void visit(size_t key, void *value) {
        graph_value v = *((graph_value*) value);
        if (v.low > VALUE_MIN) {
            v.flags |= LOW_FINAL;
        }
        if (v.high < VALUE_MAX) {
            v.flags |= HIGH_FINAL;
        }
        *((graph_value*) value) = v;
    }
    btree_traverse(g->root, g->depth, 0, visit);
}

// TODO: Fix finality proofs for 4x3.
int prove(graph *g, state *s, size_t key, int mode) {
    assert(s->passes < 2);
    graph_value v = graph_get(g, key, (graph_value) {-g->max_score, g->max_score, 0});

    if (mode == 0 || mode == 2) {
        if (v.flags & TAG) {
           return 1;
       }
        v.flags |= TAG;
    }

    if (mode == 0) {
        v.flags |= PROVES_HIGH;
    }
    else if (mode == 1) {
        v.flags |= PROVES_LOW;
    }
    else if (mode == 2) {
        v.flags |= PROVES_LOW_FINAL;
    }
    else if (mode == 3) {
        v.flags |= PROVES_HIGH_FINAL;
    }
    graph_set(g, key, v, 0);

    // if (mode == 0 && v.low == VALUE_MIN) {
    //     return 0;
    // }
    // if (mode == 1 && v.high == VALUE_MAX) {
    //     return 0;
    // }
    state child_;
    state *child = &child_;
    //size_t best_key = key;
    g->num_keys = 0;
    state_info si = get_state_info(s);
    for (int i = 0; i < g->num_moves; i++) {
        *child = *s;
        if (make_move_plus(child, si, g->move_infos[i].move)) {
            canonize_plus(child);
            size_t child_key = to_key_g(child);
            if (key_in_set(g, child_key)) {
                continue;
            }
            if (child->passes >= 2) {
                continue;
            }
            graph_value nothing = (graph_value) {VALUE_MAX, VALUE_MIN, 0};
            graph_value child_v = graph_get(g, child_key, nothing);
            if (!inequal_g(child_v, nothing)) {
                continue;
            }
            if (mode == 0) {
                if (-child_v.high == v.low) {
                    if (!prove(g, child, child_key, 1)){
                        return 0;
                    }
                }
            }
            else if (mode == 1) {
                prove(g, child, child_key, 0);
            }
            else if (mode == 2) {
                if (-child_v.low == v.high && (child_v.flags & LOW_FINAL)) {
                    if (!prove(g, child, child_key, 3)) {
                        return 0;
                    }
                }
            }
            else if (mode == 3) {
                if (-child_v.low < v.low) {
                    prove(g, child, child_key, 0);
                }
                else {
                    prove(g, child, child_key, 2);
                }
            }
        }
    }
    // if (best_key != key) {
    //     size_t child_key = best_key;
    //     from_key_g(g, child, child_key);
    //     graph_value child_v = graph_get(g, child_key, (graph_value) {-g->max_score, g->max_score, 0});
    //     prove(g, child, child_key, child_v, 1);
    // }
    return 0;
}

void prune(graph *g) {
    void clear_tag(size_t key, void *value) {
        ((graph_value *) value)->flags &= ~TAG;
    }

    size_t key = to_key_g(g->root_state);
    prove(g, g->root_state, key, 0);
    btree_traverse(g->root, g->depth, 0, clear_tag);
    prove(g, g->root_state, key, 1);
    btree_traverse(g->root, g->depth, 0, clear_tag);
    prove(g, g->root_state, key, 2);
    btree_traverse(g->root, g->depth, 0, clear_tag);
    prove(g, g->root_state, key, 3);

    void visit(size_t key, void *value) {
        graph_value *v = (graph_value*) value;
        v->flags &= ~TAG;
        if (!(v->flags & (PROVES_LOW | PROVES_HIGH | PROVES_LOW_FINAL | PROVES_HIGH_FINAL))) {
            free(v);
            btree_set(g->root, g->depth, key, NULL, 0);
        }
    }
    btree_traverse(g->root, g->depth, 0, visit);
    // TODO: btree_prune
}

void print_graph(graph *g) {
    void visit(size_t key, void *value) {
        graph_value v = *((graph_value*) value);
        state s_ = (state) {g->root_state->playing_area, 0, 0, 0, 0};
        state *s = &s_;
        from_key_g(g, s, key);
        printf("Node at key %zu\n", key);
        print_state(s);
        print_graph_node(v);
        printf("\n");
    }
    btree_traverse(g->root, g->depth, 0, visit);
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
    state child_;
    state *child = &child_;

    state_info si = get_state_info(s);

    graph g_;
    graph *g = &g_;

    init_graph(g, s);

    graph_value dummy = (graph_value) {VALUE_MIN, VALUE_MAX, 0};
    while (expand(g)) {
        assert(g->root_state->playing_area == rectangle(width, height));
        while (negamax_graph(g, 0) == 1);
        dummy = graph_get(g, 0, dummy);
        if (dummy.low > VALUE_MIN && dummy.high < VALUE_MAX) {
            assume_final(g);
            printf("Oscillations be damned!\n");
            while (negamax_graph(g, 1));
        }
        printf("Keys after expansion %zu\n", btree_num_keys(g->root, g->depth));
        print_graph_node(graph_get(g, 0, dummy));
        g->num_keys = 0;
        for (int i = 0; i < g->num_moves; i++) {
            *child = *g->root_state;
            graph_value child_v;
            if (make_move_plus(child, si, g->move_infos[i].move)) {
                canonize_plus(child);
                size_t child_key = to_key_g(child);
                if (key_in_set(g, child_key)) {
                    continue;
                }
                child_v = graph_get(g, child_key, (graph_value) {VALUE_MIN, VALUE_MAX, 0});
                printf("%zu\n", child_key);
                print_graph_node(child_v);
            }
        }
    }
    print_state(g->root_state);
    print_graph_node(graph_get(g, 0, dummy));

    prune(g);
    print_graph_node(graph_get(g, 0, dummy));
    assume_final(g);
    while (negamax_graph(g, 1));
    printf("Keys after pruning %zu\n", btree_num_keys(g->root, g->depth));
    print_graph_node(graph_get(g, 0, dummy));
    //print_graph(g);

    /*
    printf("Initial keys %zu\n", btree_num_keys(g->root, g->depth));
    expand_node(g, 0);
    printf("Keys after single expansion %zu\n", btree_num_keys(g->root, g->depth));

    for (int i = 0; i < 10; i++) {
        sporadic_expand(g);
        printf("Keys after sporadic expansion %zu\n", btree_num_keys(g->root, g->depth));
    }
    //print_graph(g);
    printf("Root value\n");
    print_graph_node(graph_get(g, 0, dummy));
    //assume_final(g);
    for (int i = 0; i < 20; i++) {
        if (i % 10 == 0) {
            assume_final(g);
        }
        negamax_graph(g);
        print_graph_node(graph_get(g, 0, dummy));
    }
    //print_graph(g);
    */
    return 0;
}
