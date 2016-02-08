#include "bit_matrix.c"

#define MAX_CHAINS (15)
#define PATTERN3_SIZE (19683)

typedef enum move_type {
    center,
    corner_nw,
    corner_ne,
    corner_sw,
    corner_se,
    edge_n,
    edge_s,
    edge_w,
    edge_e,
    pass
} move_type;

typedef struct state_info {
    stones_t player_unconditional;
    stones_t opponent_unconditional;
} state_info;

typedef struct move_info {
    stones_t move;
    int index;
    move_type type;
    int weight;
    float likelyhood;
} move_info;


// Has bugs. :(
stones_t benson_alt(stones_t player, stones_t opponent, stones_t playing_area) {
    stones_t chains[MAX_CHAINS];
    stones_t regions[MAX_CHAINS];
    unsigned char neighbours[MAX_CHAINS][MAX_CHAINS];
    char vital_count[MAX_CHAINS];
    unsigned char has_potential[MAX_CHAINS];
    int num_chains = 0;
    int num_regions = 0;
    stones_t chain_space = player;
    stones_t region_space = playing_area & ~player;
    for (int i = 0; i < MAX_CHAINS; i++) {
        stones_t p = 3UL << (2 * i);
        stones_t chain = flood(p, chain_space);
        if (chain) {
            vital_count[num_chains] = 0;
            chains[num_chains++] = chain;
            chain_space ^= chain;
        }
        stones_t region = flood(p, region_space);
        if (region) {
            has_potential[num_regions] = 0;
            regions[num_regions++] = region;
            region_space ^= region;
        }
    }
    if (num_regions < 2) {
        return 0;
    }
    for (int i = 0; i < num_chains; i++) {
        stones_t chain_cross = cross(chains[i]);
        stones_t far_empty = ~opponent & ~chain_cross;
        for (int j = 0; j < num_regions; j++) {
            neighbours[i][j] = 0;
            if (regions[j] & chain_cross){
                neighbours[i][j] = 1;
                if (!(regions[j] & far_empty)) {
                    vital_count[i]++;
                    has_potential[j] = 1;
                }
            }
        }
    }
    // for (int i = 0; i < num_chains; i++) {
    //     for (int j = 0; j < num_regions; j++) {
    //         printf("%d ", neighbours[i][j]);
    //     }
    //     printf("\n");
    // }
    int changed = 1;
    while (changed) {
        changed = 0;
        // for (int i = 0; i < num_chains; i++) {
        //     printf("Vitality %d, %d\n", i, vital_count[i]);
        // }
        // for (int j = 0; j < num_regions; j++) {
        //     printf("Potential %d, %d\n", j, has_potential[j]);
        // }
        for (int i = 0; i < num_chains; i++) {
            if (vital_count[i] == -1) {
                continue;
            }
            if (vital_count[i] < 2) {
                // printf("Dies %d, %d\n", i, vital_count[i]);
                changed = 1;
                vital_count[i] = -1;
                for (int j = 0; j < num_regions; j++) {
                    if (has_potential[j] && neighbours[i][j]) {
                        // printf("Loses %d\n", j);
                        has_potential[j] = 0;
                        for (int k = 0; k < num_chains; k++) {
                            if ((vital_count[k] != -1) && neighbours[k][j]) {
                                vital_count[k]--;
                                // printf("Withers %d -> %d because of %d\n", k, vital_count[k], j);
                            }
                        }
                    }
                }
            }
        }
    }
    // for (int i = 0; i < num_chains; i++) {
    //     printf("Vitality %d, %d\n", i, vital_count[i]);
    // }

    stones_t unconditional = 0;
    for (int i = 0; i < num_chains; i++) {
        if (vital_count[i] >= 2) {
            unconditional |= chains[i];
        }
    }
    for (int j = 0; j < num_regions; j++) {
        if (has_potential[j]) {
            unconditional |= regions[j];
        }
    }
    return unconditional;
}

stones_t benson(stones_t player, stones_t opponent, stones_t playing_area) {
    stones_t chains[MAX_CHAINS];
    stones_t regions[MAX_CHAINS];
    unsigned char chain_is_dead[MAX_CHAINS];
    int num_chains = 0;
    int num_regions = 0;
    stones_t chain_space = player;
    stones_t region_space = playing_area & ~player;
    for (int i = 0; i < MAX_CHAINS; i++) {
        stones_t p = 3UL << (2 * i);
        stones_t chain = flood(p, chain_space);
        if (chain) {
            chain_is_dead[num_chains] = 0;
            chains[num_chains++] = chain;
            chain_space ^= chain;
        }
        stones_t region = flood(p, region_space);
        if (region) {
            regions[num_regions++] = region;
            region_space ^= region;
        }
    }
    if (num_regions < 2) {
        return 0;
    }

    // Matrix of neighbourhood: region i borders chain j
    // stacked on top of matrix of vitality: region i is vital to chain j + num_chains
    bit_matrix *props = bit_matrix_new(num_regions, 2 * num_chains);

    // TODO: i for horizontal axis.
    for (int i = 0; i < num_chains; i++) {
        stones_t chain_cross = cross(chains[i]);
        stones_t far_empty = ~opponent & ~chain_cross;
        for (int j = 0; j < num_regions; j++) {
            if (regions[j] & chain_cross){
                bit_matrix_set(props, j, i);
                if (!(regions[j] & far_empty)) {
                    bit_matrix_set(props, j, i + num_chains);
                }
            }
        }
    }

    // print_bit_matrix(props);

    int quiet_time = 0;
    while (quiet_time++ < num_chains) {
        for (int i = 0; i < num_chains; i++) {
            if (chain_is_dead[i]) {
                continue;
            }
            if (bit_matrix_row_popcount(props, i + num_chains) < 2) {
                chain_is_dead[i] = 1;
                quiet_time = 0;
                bit_matrix_clear_columns_by_row(props, i);
            }
        }
    }

    stones_t unconditional = 0;
    for (int i = 0; i < num_chains; i++) {
        bit_matrix_clear_row(props, i);
        if (bit_matrix_row_nonzero(props, i + num_chains)) {
            unconditional |= chains[i];
        }
    }
    for (int j = 0; j < num_regions; j++) {
        if (bit_matrix_column_nonzero(props, j)) {
            unconditional |= regions[j];
        }
    }
    bit_matrix_free(props);
    return unconditional;
}

stones_t rshift(stones_t stones, int shift) {
    if (shift < 0) {
        return stones << (-shift);
    }
    return stones >> shift;
}

#define THREE_SLICE_OPPONENT (0x38)

static size_t THREE_KEYS[64] = {0, 1, 3, 4, 9, 10, 12, 13, 2, 1, 5, 4, 11, 10, 14, 13, 6, 7, 3, 4, 15, 16, 12, 13, 8, 7, 5, 4, 17, 16, 14, 13, 18, 19, 21, 22, 9, 10, 12, 13, 20, 19, 23, 22, 11, 10, 14, 13, 24, 25, 21, 22, 15, 16, 12, 13, 26, 25, 23, 22, 17, 16, 14, 13};

size_t pattern3(stones_t player, stones_t opponent) {
    opponent <<= 3 * H_SHIFT;
    size_t key = THREE_KEYS[(player & THREE_SLICE) | (opponent & THREE_SLICE_OPPONENT)];
    player >>= V_SHIFT;
    opponent >>= V_SHIFT;
    key = 27 * key + THREE_KEYS[(player & THREE_SLICE) | (opponent & THREE_SLICE_OPPONENT)];
    player >>= V_SHIFT;
    opponent >>= V_SHIFT;
    return 27 * key + THREE_KEYS[(player & THREE_SLICE) | (opponent & THREE_SLICE_OPPONENT)];
}

state_info get_state_info(state *s) {
    state_info si;
    si.player_unconditional = benson(s->player, s->opponent, s->playing_area);
    si.opponent_unconditional = benson(s->opponent, s->player, s->playing_area);

    // stones_t pu = benson_alt(s->player, s->opponent, s->playing_area);
    // stones_t ou = benson_alt(s->opponent, s->player, s->playing_area);
    // print_state(s);
    // assert(si.player_unconditional == pu);
    // assert(si.opponent_unconditional == ou);

    return si;
}

void print_state_info(state_info si) {
    state s_ = {0, si.player_unconditional, si.opponent_unconditional, ~0, -1};
    print_state(&s_);
}

void print_move_info(move_info mi) {
    print_stones(mi.move);
    printf("%d, %d, %g\n", mi.index, mi.weight, mi.likelyhood);
}

void canonize_plus(state *s) {
    // Killing adjacent opponent stones inside unconditial regions doens't change the status.
    state_info si = get_state_info(s);
    s->opponent &= ~liberties(s->player, si.player_unconditional);
    s->player &= ~liberties(s->opponent, si.opponent_unconditional);
    canonize(s);
}

int make_move_plus(state *s, state_info si, stones_t move) {
    if (!move) {
        // Don't pass before filling at least third of the board.
        if (3 * popcount(s->player | s->opponent) < popcount(s->playing_area)) {
            return 0;
        }
        // Don't pass before filling safe dame.
        if (liberties(si.player_unconditional, s->playing_area & ~s->opponent)) {
            return 0;
        }
    }
    if (move & (si.player_unconditional | si.opponent_unconditional)) {
        return 0;
    }
    return make_move(s, move);
}

int liberty_score_plus(state *s) {
    state_info si = get_state_info(s);
    stones_t player = (s->player | si.player_unconditional) & ~si.opponent_unconditional;
    stones_t opponent = (s->opponent | si.opponent_unconditional) & ~si.player_unconditional;
    stones_t player_controlled = player | liberties(player, s->playing_area & ~opponent);
    stones_t opponent_controlled = opponent | liberties(opponent, s->playing_area & ~player);
    return popcount(player_controlled) - popcount(opponent_controlled);
}

int compare_weight(const void *a_, const void *b_) {
    move_info *a = (move_info*) a_;
    move_info *b = (move_info*) b_;
    return b->weight - a->weight;
}

void sort_move_infos(state *s, move_info *mis, int num_moves, int *pattern3_weights, int *pattern3_corner_weights, int *pattern3_edge_weights) {
    state temp_;
    state *temp = &temp_;
    *temp = *s;
    for (int i = 0; i < num_moves; i++) {
        if (mis[i].move & (s->player | s->opponent | s->ko)) {
            mis[i].weight = -1000;
            continue;
        }
        if (mis[i].type == center) {
            int shift = mis[i].index - H_SHIFT - V_SHIFT;
            size_t p3 = pattern3(rshift(s->player, shift), rshift(s->opponent, shift));
            mis[i].weight = pattern3_weights[p3];
        }
        else if (mis[i].type == edge_n) {
            int shift = mis[i].index - H_SHIFT;
            size_t p3 = pattern3(rshift(s->player, shift), rshift(s->opponent, shift));
            mis[i].weight = pattern3_edge_weights[p3];
        }
        else if (mis[i].type == edge_s) {
            mirror_v(temp);
            int shift = (mis[i].index % WIDTH) - H_SHIFT;
            size_t p3 = pattern3(rshift(temp->player, shift), rshift(temp->opponent, shift));
            mis[i].weight = pattern3_edge_weights[p3];
        }
        else if (mis[i].type == edge_w) {
            mirror_d(temp);
            int shift = (mis[i].index / WIDTH) - H_SHIFT;
            size_t p3 = pattern3(rshift(temp->player, shift), rshift(temp->opponent, shift));
            mis[i].weight = pattern3_edge_weights[p3];
        }
        else if (mis[i].type == edge_e) {
            mirror_h(temp);
            mirror_d(temp);
            int shift = (mis[i].index / WIDTH) - H_SHIFT;
            size_t p3 = pattern3(rshift(temp->player, shift), rshift(temp->opponent, shift));
            mis[i].weight = pattern3_edge_weights[p3];
        }
        else if (mis[i].type == corner_nw) {
            size_t p3 = pattern3(s->player, s->opponent);
            mis[i].weight = pattern3_corner_weights[p3];
        }
        else if (mis[i].type == corner_ne) {
            mirror_h(temp);
            size_t p3 = pattern3(temp->player, temp->opponent);
            mis[i].weight = pattern3_corner_weights[p3];
        }
        else if (mis[i].type == corner_sw) {
            mirror_v(temp);
            size_t p3 = pattern3(temp->player, temp->opponent);
            mis[i].weight = pattern3_corner_weights[p3];
        }
        else if (mis[i].type == corner_se) {
            mirror_h(temp);
            mirror_v(temp);
            size_t p3 = pattern3(temp->player, temp->opponent);
            mis[i].weight = pattern3_corner_weights[p3];
        }
        else {
            mis[i].weight = 0;
        }
    }
    qsort((void*) mis, num_moves, sizeof(move_info), compare_weight);
    // printf("Done!\n");
    // for (int i = 0; i < num_moves; i++) { printf("%d %d\n", mis[i].weight, mis[i].index);}
}

int state_plus_test() {
    stones_t a = 376873877;
    stones_t b = (1UL << (2 * V_SHIFT)) | (1UL << (4 * H_SHIFT + V_SHIFT));
    print_stones(a);
    print_stones(b);
    stones_t u = benson(a, b, rectangle(6, 5));
    print_stones(u);
    return 0;
}
