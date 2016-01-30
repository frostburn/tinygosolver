#include <stdio.h>
#include <unistd.h>

// No headers? Are you serious?!
#define MAIN
#include "state.c"
#include "dict.c"

// Where's the makefile? Oh, you gotta be kidding me.
// gcc -std=gnu99 -O3 solver.c; ./a.out

#define VALUE_MIN (-127)
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
    /*
    assert(parent.low <= parent.high);
    assert(child.low <= child.high);
    assert(parent.low != VALUE_MAX);
    assert(parent.high != VALUE_MIN);
    assert(child.low != VALUE_MAX);
    assert(child.high != VALUE_MIN);
    print_node(parent);
    print_node(child);
    */
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
    /*
    if (parent.low < VALUE_MIN) {
        parent.low = VALUE_MIN;
    }
    if (parent.high > VALUE_MAX) {
        parent.high = VALUE_MAX;
    }
    */
    if (parent.low_distance > DISTANCE_MAX) {
        parent.low_distance = DISTANCE_MAX;
    }
    if (parent.high_distance > DISTANCE_MAX) {
        parent.high_distance = DISTANCE_MAX;
    }
    assert(parent.low <= parent.high);
    if (parent.low > VALUE_MIN){
        assert(parent.low_distance < DISTANCE_MAX);
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


void iterate(
        dict *d, lin_dict *ko_ld,
        node_value *base_nodes, node_value *pass_nodes, node_value *ko_nodes, value_t *leaf_nodes,
        state *s, size_t key_min, size_t key_max, int japanese_rules
    ) {
    state child_;
    state *child = &child_;

    size_t num_states = num_keys(d);

    int changed = 1;
    while (changed) {
        changed = 0;
        size_t key = key_min;
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
            node_value new_v = (node_value) {VALUE_MIN, VALUE_MIN, DISTANCE_MAX, 0};
            for (int j = -1; j < STATE_SIZE; j++) {
                size_t child_key;
                node_value child_v;
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
                    if (japanese_rules) {
                        int prisoners = popcount(s->opponent) - popcount(child->player);
                        if (child_v.low > VALUE_MIN) {
                            child_v.low = child_v.low - prisoners;
                        }
                        if (child_v.high < VALUE_MAX) {
                            child_v.high = child_v.high - prisoners;
                        }
                    }
                    new_v = negamax(new_v, child_v);
                }
            }
            assert(new_v.high_distance > 0);
            if (i < 2 * num_states) {
                if (i % 2 == 0) {
                    assert(new_v.low_distance > 1);
                    assert(new_v.high_distance > 1);
                    assert(new_v.low >= base_nodes[i / 2].low);
                    assert(new_v.high <= base_nodes[i / 2].high);
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
    for (size_t i = 0; i < ko_ld->num_keys; i++) {
        assert(ko_nodes[i].low_distance > 2);
        assert(ko_nodes[i].high_distance > 2);
    }

    for (size_t i = 0; i < num_states; i++) {
        assert(base_nodes[i].low_distance > 1);
        assert(base_nodes[i].high_distance > 1);
        assert(pass_nodes[i].low_distance > 0);
        assert(pass_nodes[i].high_distance > 0);
    }
}


int main() {
    state s_ = (state) {rectangle(5, 2), 0, 0, 0, 0};
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
        pass_nodes[i] = (node_value) {VALUE_MIN, VALUE_MAX, DISTANCE_MAX, DISTANCE_MAX};
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

    printf("Negamax with Chinese rules.\n");
    iterate(
        d, ko_ld,
        base_nodes, pass_nodes, ko_nodes, leaf_nodes,
        s, key_min, key_max, 0
    );

    // Japanese leaf state calculation.
    state new_s_;
    state *new_s = &new_s_;
    key = key_min;
    for (size_t i = 0; i < num_states; i++) {
        assert(from_key(s, key));
        *new_s = *s;
        node_value new_v = base_nodes[i];
        int turn = 0;
        while(new_s->passes < 2) {
            for (int j = -1; j < STATE_SIZE; j++) {
                *child = *new_s;
                stones_t move;
                if (j == -1){
                    move = 0;
                }
                else {
                    move = 1UL << j;
                }
                node_value child_v;
                if (make_move(child, move)) {
                    canonize(child);
                    child_key = to_key(child);
                    if (child->passes == 2){
                        value_t score = liberty_score(child);
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
                    int is_best_child = (-child_v.high == new_v.low && child_v.high_distance + 1 == new_v.low_distance);
                    is_best_child = !turn ? is_best_child : (-child_v.low == new_v.high && child_v.low_distance + 1 == new_v.high_distance);
                    if (is_best_child) {
                        *new_s = *child;
                        new_v = child_v;
                        break;
                    }
                }
            }
            turn = !turn;
        }
        if (turn) {
            stones_t temp = new_s->player;
            new_s->player = new_s->opponent;
            new_s->opponent = temp;
        }
        stones_t player_controlled = new_s->player | liberties(new_s->player, new_s->playing_area & ~new_s->opponent);
        stones_t opponent_controlled = new_s->opponent | liberties(new_s->opponent, new_s->playing_area & ~new_s->player);
        stones_t empty = s->playing_area & ~(s->player | s->opponent);
        value_t score = popcount(player_controlled & empty) - popcount(opponent_controlled & empty);
        score += 2 * (popcount(player_controlled & s->opponent) - popcount(opponent_controlled & s->player));
        leaf_nodes[i] = score;
        if (key != key_max){
            key = next_key(d, key);
        }
    }

    // Clear the rest of the tree.
    for (size_t i = 0; i < num_states; i++) {
        pass_nodes[i] = (node_value) {VALUE_MIN, VALUE_MAX, DISTANCE_MAX, DISTANCE_MAX};
        base_nodes[i] = (node_value) {VALUE_MIN, VALUE_MAX, DISTANCE_MAX, DISTANCE_MAX};
    }
    for (size_t i = 0; i < ko_ld->num_keys; i++) {
        ko_nodes[i] = (node_value) {VALUE_MIN, VALUE_MAX, DISTANCE_MAX, DISTANCE_MAX};
    }

    printf("Negamax with Japanese rules.\n");
    iterate(
        d, ko_ld,
        base_nodes, pass_nodes, ko_nodes, leaf_nodes,
        s, key_min, key_max, 1
    );

    return 0;
}
