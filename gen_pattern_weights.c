#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAIN
#include "state.c"
#include "dict.c"
#include "node.c"

size_t load_dat(char *filename, void **target, size_t item_size) {
    struct stat sb;
    stat(filename, &sb);

    *target = malloc(sb.st_size);

    FILE *f = fopen(filename, "rb");
    assert(fread(*target, item_size, sb.st_size / item_size, f));
    fclose(f);

    return sb.st_size / item_size;
}

int main() {
    printf("hello\n");

    assert(chdir("4x4") == 0);

    state s_ = (state) {rectangle(4, 4), 0, 0, 0, 0};
    state *s = &s_;
    state cs_;
    state *cs = &cs_;
    state child_;
    state *child = &child_;

    dict d_;
    dict *d = &d_;
    d->num_slots = load_dat("d_slots.dat", (void**) &(d->slots), sizeof(slot_t));
    load_dat("d_checkpoints.dat", (void**) &(d->checkpoints), sizeof(size_t));

    node_value *base_nodes;
    load_dat("base_nodes_j.dat", (void**) &base_nodes, sizeof(node_value));

    int *pattern3_weights = (int*) calloc(PATTERN3_SIZE, sizeof(int));
    int *pattern3_corner_weights = (int*) calloc(PATTERN3_SIZE, sizeof(int));
    int *pattern3_edge_weights = (int*) calloc(PATTERN3_SIZE, sizeof(int));

    size_t max_k = max_key(s);

    for (size_t k = 0; k < max_k; k++) {
        if (!from_key(s, k)){
            continue;
        }
        *cs = *s;
        canonize(cs);
        size_t key = to_key(cs);
        node_value v = base_nodes[key_index(d, key)];

        for (int j = 0; j < STATE_SIZE; j++) {
            *child = *s;
            stones_t move = 1UL << j;
            if (make_move(child, move)) {
                if (child->ko) {
                    continue;
                }
                canonize(child);
                size_t child_key = to_key(child);
                node_value child_v = base_nodes[key_index(d, child_key)];
                if (-child_v.high == v.low) {
                    if (j == 0) {
                        size_t p3 = pattern3(s->player, s->opponent);
                        pattern3_corner_weights[p3]++;
                    }
                    else if (j == 3) {
                        size_t p3 = pattern3(s_mirror_h4(s->player), s_mirror_h4(s->opponent));
                        pattern3_corner_weights[p3]++;
                    }
                    else if (j == 3 * V_SHIFT) {
                        size_t p3 = pattern3(s_mirror_v4(s->player), s_mirror_v4(s->opponent));
                        pattern3_corner_weights[p3]++;
                    }
                    else if (j == 3 + 3 * V_SHIFT) {
                        size_t p3 = pattern3(s_mirror_v4(s_mirror_h4(s->player)), s_mirror_v4(s_mirror_h4(s->opponent)));
                        pattern3_corner_weights[p3]++;
                    }
                    else if (j == 1 || j == 2) {
                        size_t p3 = pattern3(s->player >> (j - 1), s->opponent >> (j - 1));
                        pattern3_edge_weights[p3]++;
                    }
                    else if (j == V_SHIFT || j == 2 * V_SHIFT) {
                        size_t p3 = pattern3(s_mirror_d(s->player) >> ((j / V_SHIFT) - 1), s_mirror_d(s->opponent) >> ((j / V_SHIFT) - 1));
                        pattern3_edge_weights[p3]++;
                    }
                    else if (j == 3 + V_SHIFT || j == 3 + 2 * V_SHIFT) {
                        int shift = ((j - 3) / V_SHIFT) - 1;
                        size_t p3 = pattern3(s_mirror_d(s_mirror_h4(s->player)) >> shift, s_mirror_d(s_mirror_h4(s->opponent)) >> shift);
                        pattern3_edge_weights[p3]++;
                    }
                    else if (j == 1 + 3 * V_SHIFT || j == 2 + 3 * V_SHIFT){
                        size_t p3 = pattern3(s_mirror_v4(s->player) >> (j - 3 * V_SHIFT - 1), s_mirror_v4(s->opponent) >> (j - 3 * V_SHIFT - 1));
                        pattern3_edge_weights[p3]++;
                    }
                    else {
                        stones_t b = blob(move);
                        size_t p3 = pattern3(rshift(s->player & b, j - H_SHIFT - V_SHIFT), rshift(s->opponent & b, j - H_SHIFT - V_SHIFT));
                        pattern3_weights[p3]++;
                    }
                    //state preview = (state) {0, s->player & b, s->opponent & b, move, 0};
                    //print_state(&preview);
                }
            }
        }
    }

    assert(chdir("..") == 0);

    FILE *f = fopen("pattern3_weights.dat", "wb");
    fwrite((void*) pattern3_weights, sizeof(int), PATTERN3_SIZE, f);
    fclose(f);
    f = fopen("pattern3_corner_weights.dat", "wb");
    fwrite((void*) pattern3_corner_weights, sizeof(int), PATTERN3_SIZE, f);
    fclose(f);
    f = fopen("pattern3_edge_weights.dat", "wb");
    fwrite((void*) pattern3_edge_weights, sizeof(int), PATTERN3_SIZE, f);
    fclose(f);
}
