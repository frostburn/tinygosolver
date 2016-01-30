#include <stdio.h>

// No headers? Are you serious?!
#define MAIN
#include "state.c"
#include "dict.c"

// Where's the makefile? Oh, you gotta be kidding me.
// gcc -std=gnu99 -O3 solver.c; ./a.out

#define VALUE_MIN (-127)  // Not -128 because that breaks things.
#define VALUE_MAX (127)
#define DISTANCE_MAX (254)

typedef signed char value_t;
typedef unsigned char distance_t;

typedef struct node_value
{
    value_t low;
    value_t high;
    distance_t low_distance;
    distance_t high_distance;
} node_value;

void print_node(node_value nv) {
    printf("node_value(%d, %d, %d, %d)\n", nv.low, nv.high, nv.low_distance, nv.high_distance);
}

node_value negamax(node_value parent, node_value child) {
    if (-child.high > parent.low) {
        parent.low = -child.high;
        parent.low_distance = child.high_distance + 1;
    }
    else if (-child.high == parent.low && child.high_distance + 1 < parent.low_distance) {
        parent.low_distance = child.high_distance + 1;
    }
    if (-child.low > parent.high) {
        parent.high = -child.low;
        parent.high_distance = child.low_distance + 1;
    }
    else if (-child.low == parent.high && child.low_distance + 1 > parent.high_distance) {
        parent.high_distance = child.low_distance + 1;
    }
    if (parent.low_distance > DISTANCE_MAX) {
        parent.low_distance = DISTANCE_MAX;
    }
    if (parent.high_distance > DISTANCE_MAX) {
        parent.high_distance = DISTANCE_MAX;
    }
    return parent;
}

int equal(node_value a, node_value b) {
    if (a.low != b.low) {
        return 0;
    }
    if (a.high != b.high) {
        return 0;
    }
    if (a.low_distance != b.low_distance) {
        return 0;
    }
    return a.high_distance == b.high_distance;
}

int main() {
    state s_ = (state) {rectangle(4, 3), 0, 0, 0, 0};
    state *s = &s_;

    dict d_;
    dict *d = &d_;

    size_t max_k = max_key(s);
    init_dict(d, max_k);

    size_t key_min = ~0;
    size_t key_max = 0;

    size_t total_legal = 0;
    for (size_t k = 0; k < max_k; k++) {
        if (!from_key(s, k)){
            continue;
        }
        total_legal++;
        canonize(s);
        size_t key = to_key(s);
        add_key(d, key);
        if (key < key_min) {
            key_min = key;
        }
        if (key > key_max) {
            key_max = key;
        }
    }
    resize_dict(d, key_max);
    finalize_dict(d);
    size_t num_states = num_keys(d);

    printf("Total legal positions %zu\n", total_legal);
    printf("Total unique positions %zu\n", num_states);

    node_value *base_nodes = (node_value*) malloc(num_states * sizeof(node_value));
    node_value *pass_nodes = (node_value*) malloc(num_states * sizeof(node_value));
    value_t *leaf_nodes = (value_t*) malloc(num_states * sizeof(value_t));

    lin_dict ko_ld_ = (lin_dict) {0, 0, NULL};
    lin_dict *ko_ld = &ko_ld_;

    state child_;
    state *child = &child_;
    size_t child_key;
    size_t key = key_min;
    for (size_t i = 0; i < num_states; i++) {
        assert(from_key(s, key));
        value_t score = liberty_score(s);
        leaf_nodes[i] = score;
        pass_nodes[i] = (node_value) {-score, VALUE_MAX, 1, DISTANCE_MAX};  // We already almost know the result of the first iteration.
        base_nodes[i] = (node_value) {VALUE_MIN, VALUE_MAX, DISTANCE_MAX, DISTANCE_MAX};
        for (int j = 0; j < STATE_SIZE; j++) {
            *child = *s;
            if (make_move(child, 1UL << j)) {
                if (child->ko) {
                    canonize(child);
                    child_key = to_key(child) * STATE_SIZE + bitscan(child->ko);
                    add_lin_key(ko_ld, child_key);
                }
            }
        }
        if (key != key_max){
            key = next_key(d, key);
        }
    }

    node_value *ko_nodes = (node_value*) malloc(ko_ld->num_keys * sizeof(node_value));
    printf("Unique positions with ko %zu\n", ko_ld->num_keys);

    for (size_t i = 0; i < ko_ld->num_keys; i++) {
        ko_nodes[i] = (node_value) {VALUE_MIN, VALUE_MAX, DISTANCE_MAX, DISTANCE_MAX};
    }

    node_value new_v;
    node_value child_v;
    int changed = 1;
    while (changed) {
        changed = 0;
        key = key_min;
        for (size_t i = 0; i < 2 * num_states + ko_ld->num_keys; i++) {
            if (i < 2 * num_states) {
                assert(from_key(s, key));
                if (i % 2 == 1) {
                    s->passes = 1;
                }
            }
            else {
                key = ko_ld->keys[i - 2 * num_states];
                size_t ko_pos = key % STATE_SIZE;
                key /= STATE_SIZE;
                assert(from_key(s, key));
                s->ko = 1UL << ko_pos;
            }
            new_v = (node_value) {VALUE_MIN, VALUE_MIN, DISTANCE_MAX, 0};
            for (int j = -1; j < STATE_SIZE; j++) {
                *child = *s;
                stones_t move;
                if (j == -1){
                    move = 0;
                }
                else {
                    move = 1UL << j;
                }
                if (make_move(child, move)) {
                    canonize(child);
                    child_key = to_key(child);
                    if (child->passes == 2){
                        value_t score = leaf_nodes[key_index(d, child_key)];
                        child_v = (node_value) {score, score, 0, 0};
                    }
                    else if (child->passes == 1) {
                        child_v = pass_nodes[key_index(d, child_key)];
                    }
                    else if (child->ko) {
                        child_key = child_key * STATE_SIZE + bitscan(child->ko);
                        child_v = ko_nodes[lin_key_index(ko_ld, child_key)];
                    }
                    else {
                        child_v = base_nodes[key_index(d, child_key)];
                    }
                    new_v = negamax(new_v, child_v);
                }
            }
            if (i < 2 * num_states) {
                if (i % 2 == 0){
                    changed = changed || !equal(base_nodes[i / 2], new_v);
                    base_nodes[i / 2] = new_v;
                }
                else {
                    changed = changed || !equal(pass_nodes[i / 2], new_v);
                    pass_nodes[i / 2] = new_v;
                    if (key != key_max){
                        key = next_key(d, key);
                    }
                }
            }
            else {
                changed = changed || !equal(ko_nodes[i - 2 * num_states], new_v);
                ko_nodes[i - 2 * num_states] = new_v;
            }
        }
        print_node(base_nodes[0]);
    }
    print_node(base_nodes[0]);

    /*
    key = key_min;
    for (int i=0;i<num_states;i++){
        from_key(s, key);
        print_state(s);
        print_node(base_nodes[i]);
        print_node(pass_nodes[i]);
        key = next_key(d, key);
    }
    */

    return 0;
}
