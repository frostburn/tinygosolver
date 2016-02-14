#include <limits.h>
#include "bit_matrix.c"

#define MAX_CHAINS (15)
#define PATTERN3_SIZE (19683)

#define WEIGHT_MIN INT_MIN

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

int block_size(stones_t stones, stones_t *middle) {
    int count = popcount(stones);
    if (count == 0) {
        return 0;
    }
    else if (count == 1) {
        return 1;
    }
    else if (count == 2) {
        if ((stones & west(stones)) || (stones & north(stones))) {
            return 2;
        }
    }
    else if (count == 3) {
        stones_t temp = stones & north(stones);
        *middle = temp & south(stones);
        if (*middle) {
            return 3;  // vertical straight three
        }
        *middle = temp & west(stones);
        if (*middle) {
            return 3;  // Gamma
        }
        *middle = temp & east(stones);
        if (*middle) {
            return 3;  // 7
        }
        temp = west(stones) & stones;
        *middle = temp & east(stones);
        if (*middle) {
            return 3;  // horizontal straight three
        }
        *middle = temp & south(stones);
        if (*middle) {
            return 3;  // L
        }
        *middle = east(stones) & stones & south(stones);
        if (*middle) {
            return 3;  // J
        }
    }
    // TODO: four-blocks
    return -1;
}

stones_t unconditional_territory(stones_t player, stones_t opponent, stones_t playing_area) {
    stones_t player_unconditional = benson(player, opponent, playing_area);
    if (!player_unconditional) {
        return 0;
    }
    stones_t region_space = playing_area & ~player_unconditional;
    stones_t pu_cross = cross(player_unconditional);
    for (int i = 0; i < MAX_CHAINS; i++) {
        stones_t region = flood(3UL << (2 * i), region_space);
        if (region) {
            stones_t eyespace = region & ~pu_cross;
            stones_t middle;
            int bs = block_size(eyespace, &middle);
            if (bs < 3) {
                player_unconditional |= region;
                continue;
            }
            else if (bs == 3) {
                if (player & middle) {
                    player_unconditional |= region;
                    continue;
                }
            }
            else if (bs == 4) {
                if ((player & region) == middle) {
                    if (popcount(liberties(player & eyespace, playing_area & ~opponent)) < 4) {
                        player_unconditional |= region;
                        continue;
                    }
                    // TODO: 4 liberties.
                }
            }
            region_space ^= region;
        }
    }
    return player_unconditional;
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
    si.player_unconditional = unconditional_territory(s->player, s->opponent, s->playing_area);
    si.opponent_unconditional = unconditional_territory(s->opponent, s->player, s->playing_area);

    // stones_t pu = benson_alt(s->player, s->opponent, s->playing_area);
    // stones_t ou = benson_alt(s->opponent, s->player, s->playing_area);
    // print_state(s);
    // assert(si.player_unconditional == pu);
    // assert(si.opponent_unconditional == ou);

    return si;
}

int get_move_infos(state *s, move_info *move_infos) {
    int num_moves = 1;
    move_infos[0] = (move_info) {0, -1, pass, 0, 0};
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
            move_infos[num_moves++] = (move_info) {move, i, type, 0, 0};
        }
    }
    return num_moves;
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
#ifdef BLOCK_PASSES
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
#endif
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

void get_score_bounds(state *s, state_info si, int *lower_bound, int *upper_bound) {
    *lower_bound = popcount(si.player_unconditional) - popcount(s->playing_area & ~si.player_unconditional);
    *upper_bound = popcount(s->playing_area & ~si.opponent_unconditional) - popcount(si.opponent_unconditional);
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
            mis[i].weight = WEIGHT_MIN;
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

int euler(stones_t stones) {
    stones_t temp = stones | west(stones);
    int characteristic = -popcount(stones & WEST_WALL);  // western vertical edges
    characteristic -= popcount(temp); // rest of the vertical edges
    characteristic += stones & 1UL;  // northwestern vertex
    characteristic += popcount(temp & NORTH_WALL);  // northern vertices
    characteristic += popcount(temp | north(temp));  // rest of the vertices
    temp = stones | north(stones);
    characteristic += popcount(temp & WEST_WALL);  // western vertices
    characteristic -= popcount(stones & NORTH_WALL);  // northern horizontal edges
    characteristic -= popcount(temp);  // rest of the horizontal edges
    characteristic += popcount(stones);  // pixels

    return characteristic;
}

int _heuristic_score(stones_t player, stones_t opponent, stones_t playing_area) {
    int score = popcount(cross(player) & playing_area & ~opponent);
    score -= euler(player);
    score *= 2;
    score -= popcount(player & cross(playing_area));
    return score;
}

int heuristic_score(state *s) {
    return _heuristic_score(s->player, s->opponent, s->playing_area) - _heuristic_score(s->opponent, s->player, s->playing_area);
}

int state_plus_test() {
    stones_t a = 376873877;
    stones_t b = (1UL << (2 * V_SHIFT)) | (1UL << (4 * H_SHIFT + V_SHIFT));
    print_stones(a);
    print_stones(b);
    stones_t u = benson(a, b, rectangle(6, 5));
    print_stones(u);

    for (int i = 0; i < STATE_SIZE; i++) {
        assert(euler(1UL << i) == 1);
        if (i % WIDTH < WIDTH - 1) {
            assert(euler(3UL << i) == 1);
        }
    }
    assert(euler(rectangle(3, 3) ^ 1UL << (H_SHIFT + V_SHIFT)) == 0);

    return 0;
}
