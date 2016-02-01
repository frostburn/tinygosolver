#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// No headers? Are you serious?!
#define MAIN
#include "state.c"
#include "dict.c"
#include "node.c"

// Where's the makefile? Oh, you gotta be kidding me.
// gcc -std=gnu99 -Wall -O3 solver.c -o solver; solver 4 4

#define PRISONER_VALUE (2)

void iterate(
        dict *d, lin_dict *ko_ld,
        node_value *base_nodes, node_value *pass_nodes, node_value *ko_nodes, value_t *leaf_nodes,
        state *s, size_t key_min, int japanese_rules
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
                        int prisoners = (popcount(s->opponent) - popcount(child->player)) * PRISONER_VALUE;
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
                    key = next_key(d, key);
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


void endstate(
        dict *d, lin_dict *ko_ld,
        node_value *base_nodes, node_value *pass_nodes, node_value *ko_nodes,
        state *s, node_value parent_v, int turn, int low_player
    ) {
    if (s->passes == 2) {
        if (turn) {
            stones_t temp = s->player;
            s->player = s->opponent;
            s->opponent = temp;
        }
        return;
    }
    state child_;
    state *child = &child_;
    for (int j = -1; j < STATE_SIZE; j++) {
        *child = *s;
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
            size_t child_key = to_key(child);
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
            int is_best_child = (-child_v.high == parent_v.low && child_v.high_distance + 1 == parent_v.low_distance);
            is_best_child = low_player ? is_best_child : (-child_v.low == parent_v.high && child_v.low_distance + 1 == parent_v.high_distance);
            if (is_best_child) {
                *s = *child;
                endstate(
                    d, ko_ld,
                    base_nodes, pass_nodes, ko_nodes,
                    s, child_v, !turn, !low_player
                );
            }
        }
    }
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

    state s_ = (state) {rectangle(width, height), 0, 0, 0, 0};
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

    lin_dict ko_ld_ = (lin_dict) {0, 0, 0, NULL};
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
        key = next_key(d, key);
    }

    finalize_lin_dict(ko_ld);

    node_value *ko_nodes = (node_value*) malloc(ko_ld->num_keys * sizeof(node_value));
    printf("Unique positions with ko %zu\n", ko_ld->num_keys);

    for (size_t i = 0; i < ko_ld->num_keys; i++) {
        ko_nodes[i] = (node_value) {VALUE_MIN, VALUE_MAX, DISTANCE_MAX, DISTANCE_MAX};
    }

    #ifndef PRELOAD
    printf("Negamax with Chinese rules.\n");
    iterate(
        d, ko_ld,
        base_nodes, pass_nodes, ko_nodes, leaf_nodes,
        s, key_min, 0
    );
    #endif

    char dir_name[16];
    sprintf(dir_name, "%dx%d", width, height);
    struct stat sb;
    if (stat(dir_name, &sb) == -1) {
        mkdir(dir_name, 0700);
    }
    assert(chdir(dir_name) == 0);
    FILE *f;

    #ifndef PRELOAD
    f = fopen("d_slots.dat", "wb");
    fwrite((void*) d->slots, sizeof(slot_t), d->num_slots, f);
    fclose(f);
    f = fopen("d_checkpoints.dat", "wb");
    fwrite((void*) d->checkpoints, sizeof(size_t), (d->num_slots >> 4) + 1, f);
    fclose(f);
    f = fopen("ko_ld_keys.dat", "wb");
    fwrite((void*) ko_ld->keys, sizeof(size_t), ko_ld->num_keys, f);
    fclose(f);

    f = fopen("base_nodes.dat", "wb");
    fwrite((void*) base_nodes, sizeof(node_value), num_states, f);
    fclose(f);
    f = fopen("pass_nodes.dat", "wb");
    fwrite((void*) pass_nodes, sizeof(node_value), num_states, f);
    fclose(f);
    f = fopen("leaf_nodes.dat", "wb");
    fwrite((void*) leaf_nodes, sizeof(value_t), num_states, f);
    fclose(f);
    f = fopen("ko_nodes.dat", "wb");
    fwrite((void*) ko_nodes, sizeof(node_value), ko_ld->num_keys, f);
    fclose(f);
    #endif

    #ifdef PRELOAD
    f = fopen("base_nodes.dat", "rb");
    fread((void*) base_nodes, sizeof(node_value), num_states, f);
    fclose(f);
    f = fopen("pass_nodes.dat", "rb");
    fread((void*) pass_nodes, sizeof(node_value), num_states, f);
    fclose(f);
    f = fopen("leaf_nodes.dat", "rb");
    fread((void*) leaf_nodes, sizeof(value_t), num_states, f);
    fclose(f);
    f = fopen("ko_nodes.dat", "rb");
    fread((void*) ko_nodes, sizeof(node_value), ko_ld->num_keys, f);
    fclose(f);
    #endif

    // Japanese leaf state calculation.
    state new_s_;
    state *new_s = &new_s_;
    key = key_min;
    for (size_t i = 0; i < num_states; i++) {
        assert(from_key(s, key));
        stones_t empty = s->playing_area & ~(s->player | s->opponent);

        *new_s = *s;
        endstate(
            d, ko_ld,
            base_nodes, pass_nodes, ko_nodes,
            new_s, base_nodes[i], 0, 1
        );

        stones_t player_controlled = new_s->player | liberties(new_s->player, new_s->playing_area & ~new_s->opponent);
        stones_t opponent_controlled = new_s->opponent | liberties(new_s->opponent, new_s->playing_area & ~new_s->player);
        value_t score = popcount(player_controlled & empty) - popcount(opponent_controlled & empty);
        score += 2 * (popcount(player_controlled & s->opponent) - popcount(opponent_controlled & s->player));
        leaf_nodes[i] = score;

        *new_s = *s;
        endstate(
            d, ko_ld,
            base_nodes, pass_nodes, ko_nodes,
            new_s, base_nodes[i], 0, 0
        );

        player_controlled = new_s->player | liberties(new_s->player, new_s->playing_area & ~new_s->opponent);
        opponent_controlled = new_s->opponent | liberties(new_s->opponent, new_s->playing_area & ~new_s->player);
        score = popcount(player_controlled & empty) - popcount(opponent_controlled & empty);
        score += 2 * (popcount(player_controlled & s->opponent) - popcount(opponent_controlled & s->player));
        leaf_nodes[i] += score;

        key = next_key(d, key);
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
        s, key_min, 1
    );

    f = fopen("base_nodes_j.dat", "wb");
    fwrite((void*) base_nodes, sizeof(node_value), num_states, f);
    fclose(f);
    f = fopen("pass_nodes_j.dat", "wb");
    fwrite((void*) pass_nodes, sizeof(node_value), num_states, f);
    fclose(f);
    f = fopen("leaf_nodes_j.dat", "wb");
    fwrite((void*) leaf_nodes, sizeof(value_t), num_states, f);
    fclose(f);
    f = fopen("ko_nodes_j.dat", "wb");
    fwrite((void*) ko_nodes, sizeof(node_value), ko_ld->num_keys, f);
    fclose(f);

    return 0;
}
