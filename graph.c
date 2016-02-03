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
#define TAG (2)

typedef struct graph_value
{
    value_t low;
    value_t high;
    char low_final;
    char high_final;
} graph_value;

typedef struct graph
{
    int num_moves;
    stones_t moves[STATE_SIZE + 1];
    state *root_state;
    value_t max_score;
    int depth;
    vertex *root;
    size_t num_keys;
    size_t key_set[STATE_SIZE + 1];
} graph;

void print_graph_node(graph_value v) {
    printf("graph_node(%d, %d, %d, %d)\n", v.low, v.high, v.low_final, v.high_final);
}

graph_value negamax_value(graph_value parent, graph_value child) {;
    if (-child.high > parent.low) {
        parent.low = -child.high;
    }
    if (-child.low > parent.low) {
        parent.low_final = parent.low_final && child.high_final;
    }
    if (-child.low > parent.high) {
        parent.high = -child.low;
        parent.high_final = child.low_final;
    }
    else if (-child.low == parent.high){
        parent.high_final = parent.high_final || child.low_final;
    }
    if (parent.high == VALUE_MAX) {
        assert(!parent.high_final);
    }
    if (parent.low == VALUE_MIN) {
        assert(!parent.low_final);
    }
    if (parent.low == parent.high) {
        parent.low_final = 1;
        parent.high_final = 1;
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
    if (a.low_final != b.low_final) {
        return 2;
    }
    if (a.high_final != b.high_final) {
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
    g->num_moves = 1;
    g->moves[0] = 0;
    for (int i = 0; i < STATE_SIZE; i++) {
        stones_t move = 1ULL << i;
        if (move & s->playing_area) {
            g->moves[g->num_moves++] = move;
        }
    }
    size_t key_max = (max_key(s) + 1) * KEY_MUL;
    g->depth = 0;
    while (key_max) {
        key_max /= 2;
        g->depth++;
    }
    g->root = (vertex*) calloc(1, sizeof(vertex));
    graph_set(g, to_key(s) * KEY_MUL, (graph_value) {-g->max_score, g->max_score, 0, 0}, 0);
}

void from_key_g(state *s, size_t key) {
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

int negamax_node(graph *g, size_t key, graph_value old_v, int mode) {
    state s_ = (state) {g->root_state->playing_area, 0, 0, 0, 0};
    state *s = &s_;
    state child_;
    state *child = &child_;
    from_key_g(s, key);
    if (s->passes >= 2) {
        return 0;
        // value_t score = liberty_score(s);
        // return graph_set(g, key, (graph_value) {score, score, 1, 1}, 0);
    }
    graph_value v = (graph_value) {VALUE_MIN, VALUE_MIN, 1, 0};
    state_info si = get_state_info(s);
    for (int i = 0; i < g->num_moves; i++) {
        *child = *s;
        graph_value child_v;
        if (make_move_plus(child, si, g->moves[i])) {
            canonize_plus(child);
            size_t child_key = to_key_g(child);
            if (child->passes >= 2) {
                value_t score = liberty_score_plus(child);
                child_v = (graph_value) {score, score, 1, 1};
            }
            else {
                child_v = graph_get(g, child_key, (graph_value) {-g->max_score, g->max_score, 0, 0});
            }
            v = negamax_value(v, child_v);
        }
    }
    // Some finality oscillations were seen during testing. Maybe they're gone now.
    // if (mode == 1) {
    //     v.low_final = v.low_final && old_v.low_final;
    //     v.high_final = v.high_final && old_v.high_final;
    // }
    int changed = graph_set(g, key, v, 0);
    return changed;
}

int negamax_graph(graph *g, int mode) {
    int changed = 0;
    void visit(size_t key, void *value) {
        int change = negamax_node(g, key, *((graph_value*) value), mode);
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

void use_heuristic_move(graph *g) {
    size_t n = g->num_moves;
    for (size_t i = 0; i < n - 1; i++) {
      size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
      stones_t t = g->moves[j];
      g->moves[j] = g->moves[i];
      g->moves[i] = t;
    }
}

void expand_node(graph *g, size_t key) {
    state s_ = (state) {g->root_state->playing_area, 0, 0, 0, 0};
    state *s = &s_;
    state child_;
    state *child = &child_;
    from_key_g(s, key);
    if (s->passes >= 2) {
        return;
    }
    state_info si = get_state_info(s);
    for (int i = 0; i < g->num_moves; i++) {
        *child = *s;
        if (make_move_plus(child, si, g->moves[i])) {
            canonize_plus(child);
            size_t child_key = to_key_g(child);
            graph_value v = (graph_value) {-g->max_score, g->max_score, 0, 0};
            if (child->passes == 2){
                value_t score = liberty_score_plus(child);
                v = (graph_value) {score, score, 1, 1};
            }
            graph_set(g, child_key, v, 1);
        }
    }
}

int add_tag(graph *g, size_t key) {
    graph_value *value;
    value = (graph_value*) btree_get(g->root, g->depth, key);
    int tagged = value->low_final & TAG;
    value->low_final |= TAG;
    return tagged;
}

int expand_state(graph *g, state *s, size_t key, graph_value v, int mode) {
    //printf("%zu\n", key);
    //print_graph_node(v);
    assert(s->passes < 2);
    if (mode == 1 || mode == 3) {
        if (add_tag(g, key)) {
            return 1;
        }
    }
    if (mode == 0) {
        //assert(v.low == VALUE_MIN);
    }
    else if (mode == 1) {
        // TODO: Find out why this fails.
        //assert(v.high == VALUE_MAX);
    }
    else if (mode == 2) {
        //assert(!v.high_final);
    }
    else if (mode == 3) {
        //assert(!v.low_final);
    }
    state child_;
    state *child = &child_;
    if (mode == 0 || mode == 2) {
        use_heuristic_move(g);
    }
    g->num_keys = 0;
    state_info si = get_state_info(s);
    for (int i = 0; i < g->num_moves; i++) {
        *child = *s;
        if (make_move_plus(child, si, g->moves[i])) {
            canonize_plus(child);
            size_t child_key = to_key_g(child);
            if (key_in_set(g, child_key)) {
                continue;
            }
            if (child->passes >= 2) {
                //value_t score = liberty_score(child);
                //graph_set(g, child_key, (graph_value) {score, score, 1, 1}, 0);
                continue;
            }
            graph_value child_v = graph_get(g, child_key, (graph_value) {-g->max_score, g->max_score, 0, 0});
            if (mode == 1) {
                if (child_v.low > VALUE_MIN) {
                    continue;
                }
            }
            else if (mode == 2) {
                if (-child_v.low < v.high) {
                    continue;
                }
            }
            else if (mode == 3) {
                if (child_v.high_final || -child_v.low < v.low) {
                    // TODO: Find out why this fails.
                    //continue;
                }
            }
            if (!graph_set(g, child_key, child_v, 1)) {
                int child_mode = -1;
                if (mode == 0) {
                    child_mode = 1;
                }
                else if (mode == 1) {
                    child_mode = 0;
                }
                else if (mode == 2) {
                    child_mode = 3;
                }
                else if (mode == 3) {
                    child_mode = 2;
                }
                int loops = expand_state(g, child, child_key, child_v, child_mode);
                if ((mode == 0 || mode == 2) && !loops) {
                    break;
                }
            }
        }
    }
    return 0;
}

int expand(graph *g) {
    size_t key = to_key_g(g->root_state);
    graph_value v = graph_get(g, key, (graph_value) {-g->max_score, g->max_score, 0, 0});
    if (v.low == VALUE_MIN) {
        expand_state(g, g->root_state, key, v, 0);
    }
    else if (v.high == VALUE_MAX) {
        expand_state(g, g->root_state, key, v, 1);
    }
    else if (!v.high_final) {
        expand_state(g, g->root_state, key, v, 2);
    }
    else if (!v.low_final) {
        expand_state(g, g->root_state, key, v, 3);
    }
    else {
        return 0;
    }
    void visit(size_t key, void *value) {
        ((graph_value *) value)->low_final &= ~TAG;
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
            v.low_final = 1;
        }
        if (v.high < VALUE_MAX) {
            v.high_final = 1;
        }
        *((graph_value*) value) = v;
    }
    btree_traverse(g->root, g->depth, 0, visit);
}

void print_graph(graph *g) {
    void visit(size_t key, void *value) {
        graph_value v = *((graph_value*) value);
        state s_ = (state) {g->root_state->playing_area, 0, 0, 0, 0};
        state *s = &s_;
        from_key_g(s, key);
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

    state s_ = (state) {rectangle(width, height), 0, 0, 0, 0};
    state *s = &s_;
    state child_;
    state *child = &child_;

    state_info si = get_state_info(s);

    graph g_;
    graph *g = &g_;

    init_graph(g, s);

    graph_value dummy = (graph_value) {VALUE_MIN, VALUE_MAX, 0, 0};
    while (expand(g)) {
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
            if (make_move_plus(child, si, g->moves[i])) {
                canonize_plus(child);
                size_t child_key = to_key_g(child);
                if (key_in_set(g, child_key)) {
                    continue;
                }
                child_v = graph_get(g, child_key, (graph_value) {VALUE_MIN, VALUE_MAX, 0, 0});
                printf("%zu\n", child_key);
                print_graph_node(child_v);
            }
        }
    }
    print_state(g->root_state);
    print_graph_node(graph_get(g, 0, dummy));

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
