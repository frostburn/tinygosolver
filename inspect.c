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
// gcc -std=gnu99 -Wall -O3 inspect.c; ./a.out 4 4

#define PRISONER_VALUE (2)

void inspect(
        dict *d, lin_dict *ko_ld,
        node_value *base_nodes, node_value *pass_nodes, node_value *ko_nodes, value_t *leaf_nodes,
        state *s, node_value parent_v, int low_player, int japanese_rules
    ) {
    print_state(s);
    print_node(parent_v);
    if (s->passes == 2){
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

            node_value child_tree_v = child_v;
            if (japanese_rules) {
                int prisoners = (popcount(s->opponent) - popcount(child->player)) * PRISONER_VALUE;
                if (child_v.low > VALUE_MIN) {
                    child_v.low = child_v.low - prisoners;
                }
                if (child_v.high < VALUE_MAX) {
                    child_v.high = child_v.high - prisoners;
                }
            }

            int is_best_child = (-child_v.high == parent_v.low && child_v.high_distance + 1 == parent_v.low_distance);
            is_best_child = low_player ? is_best_child : (-child_v.low == parent_v.high && child_v.low_distance + 1 == parent_v.high_distance);
            if (is_best_child) {
                *s = *child;
                inspect(
                    d, ko_ld,
                    base_nodes, pass_nodes, ko_nodes, leaf_nodes,
                    s, child_tree_v, !low_player, japanese_rules
                );
                return;
            }
        }
    }
    assert(0);
}

size_t load_dat(char *filename, void **target, size_t item_size) {
    struct stat sb;
    stat(filename, &sb);

    *target = malloc(sb.st_size);

    FILE *f = fopen(filename, "rb");
    assert(fread(*target, item_size, sb.st_size / item_size, f));
    fclose(f);

    return sb.st_size / item_size;
}

int main(int argc, char *argv[]) {
    if (argc != 3 && argc != 4) {
        printf("Usage: %s width height high_path\n", argv[0]);
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
    int player_low = 1;
    if (argc == 4) {
        player_low = 0;
    }

    state s_ = (state) {rectangle(width, height), 0, 0, 0, 0};
    state *s = &s_;

    char dir_name[16];
    sprintf(dir_name, "%dx%d", width, height);
    assert(chdir(dir_name) == 0);

    dict d_;
    dict *d = &d_;
    d->num_slots = load_dat("d_slots.dat", (void**) &(d->slots), sizeof(slot_t));
    load_dat("d_checkpoints.dat", (void**) &(d->checkpoints), sizeof(size_t));

    lin_dict ko_ld_;
    lin_dict *ko_ld = &ko_ld_;

    ko_ld->num_keys = load_dat("ko_ld_keys.dat", (void**) &(ko_ld->keys), sizeof(size_t));

    node_value *base_nodes;
    load_dat("base_nodes_j.dat", (void**) &base_nodes, sizeof(node_value));
    node_value *pass_nodes;
    load_dat("pass_nodes_j.dat", (void**) &pass_nodes, sizeof(node_value));
    node_value *ko_nodes;
    load_dat("ko_nodes_j.dat", (void**) &ko_nodes, sizeof(node_value));
    value_t *leaf_nodes;
    load_dat("leaf_nodes_j.dat", (void**) &leaf_nodes, sizeof(value_t));

    canonize(s);
    inspect(
        d, ko_ld,
        base_nodes, pass_nodes, ko_nodes, leaf_nodes,
        s, base_nodes[key_index(d, to_key(s))], player_low, 1
    );

    /*
    FILE *f;

    f = fopen("d_slots.dat", "wb");
    fwrite((void*) d->slots, sizeof(slot_t), d->num_slots, f);
    fclose(f);
    f = fopen("d_checkpoints.dat", "wb");
    fwrite((void*) d->checkpoints, sizeof(size_t), (d->num_slots >> 4) + 1, f);
    fclose(f);
    f = fopen("ko_ld_keys.dat", "wb");
    fwrite((void*) ko_ld->keys, sizeof(size_t), ko_ld->num_keys, f);
    fclose(f);
    */
}
