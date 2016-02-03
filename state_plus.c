#define MAX_CHAINS (15)

typedef struct state_info {
    stones_t player_unconditional;
    stones_t opponent_unconditional;
} state_info;

stones_t benson(stones_t player, stones_t opponent, stones_t playing_area) {
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

state_info get_state_info(state *s) {
    state_info si;
    si.player_unconditional = benson(s->player, s->opponent, s->playing_area);
    si.opponent_unconditional = benson(s->opponent, s->player, s->playing_area);
    return si;
}

void print_state_info(state_info si) {
    state s_ = {0, si.player_unconditional, si.opponent_unconditional, ~0, -1};
    print_state(&s_);
}

void canonize_plus(state *s) {
    // Killing adjacent opponent stones inside unconditial regions doens't change the status.
    state_info si = get_state_info(s);
    s->opponent &= ~liberties(s->player, si.player_unconditional);
    s->player &= ~liberties(s->opponent, si.opponent_unconditional);
    canonize(s);
}

int make_move_plus(state *s, state_info si, stones_t move) {
    // Passing always allowed for now because of infinite loops.
    // if (!move) {
    //     // Don't pass before filling at least third of the board.
    //     if (3 * popcount(s->player | s->opponent) < popcount(s->playing_area)) {
    //         return 0;
    //     }
    //     // Don't pass before filling safe dame.
    //     if (liberties(si.player_unconditional, s->playing_area)) {
    //         // Breaks for some reason.
    //         return 0;
    //     }
    // }
    if (move & (si.player_unconditional | si.opponent_unconditional)) {
        //printf("%u, %u, %u\n", s->player, s->opponent, move);
        //print_state(s);
        //print_state_info(si);
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

int state_plus_test() {
    stones_t a = 376873877;
    stones_t b = (1UL << (2 * V_SHIFT)) | (1UL << (4 * H_SHIFT + V_SHIFT));
    print_stones(a);
    print_stones(b);
    stones_t u = benson(a, b, rectangle(6, 5));
    print_stones(u);
    return 0;
}
