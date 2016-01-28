#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH (6)
#define HEIGTH (5)
#define STATE_SIZE (30)
#define H_SHIFT (1)
#define V_SHIFT (6)
#define EAST_WALL (0x20820820)
#define WEST_BLOCK (0x1f7df7df)

typedef unsigned int stones_t;

typedef struct  state
{
    stones_t playing_area;
    stones_t player;
    stones_t opponent;
    stones_t ko;
    int passes;
} state;


int is_legal(state *s);


void print_stones(stones_t stones) {
    printf(" ");
    for (int i = 0; i < WIDTH; i++) {
        printf(" %c", 'A' + i);
    }
    printf("\n");
    for (int i = 0; i < 32; i++) {
        if (i % V_SHIFT == 0) {
            printf("%d", i / V_SHIFT);
        }
        if ((1UL << i) & stones) {
            printf(" @");
        }
        else {
            printf("  ");
        }
        if (i % V_SHIFT == V_SHIFT - 1){
            printf("\n");
        }
    }
    printf("\n");
}


void print_state(state *s) {
    printf(" ");
    for (int i = 0; i < WIDTH; i++) {
        printf(" %c", 'A' + i);
    }
    printf("\n");
    for (stones_t i = 0; i < 32; i++) {
        if (i % V_SHIFT == 0) {
            printf("%d", i / V_SHIFT);
        }
        stones_t p = (1UL << i);
        if (p & s->player) {
            printf(" @");
        }
        else if (p & s->opponent) {
            printf(" O");
        }
        else if (p & s->ko) {
            printf(" *");
        }
        else if (p & s->playing_area) {
            printf(" .");
        }
        else {
            printf("  ");
        }
        if (i % V_SHIFT == V_SHIFT - 1){
            printf("\n");
        }
    }
    printf("\n");
}


int popcount(stones_t stones) {
    return __builtin_popcount(stones);
}

size_t bitscan(stones_t stones) {
    assert(stones);
    size_t index = 0;
    while (!((1UL << index) & stones)){
        index++;
    }
    return index;
}


stones_t rectangle(int width, int height) {
    stones_t r = 0;
    for (int i = 0; i < width; ++i)
    {
        for (int j = 0; j < height; ++j)
        {
            r |= 1UL << (i * H_SHIFT + j * V_SHIFT);
        }
    }
    return r;
}


stones_t flood(stones_t source, stones_t target) {
    source &= target;
    if (!source){
        return source;
    }
    stones_t temp;
    do {
        temp = source;
        source |= (
            ((source & WEST_BLOCK) << H_SHIFT) |
            ((source >> H_SHIFT) & WEST_BLOCK) |
            (source << V_SHIFT) |
            (source >> V_SHIFT)
        ) & target;
    } while (temp != source);
    return source;
}

stones_t north(stones_t stones) {
    return stones >> V_SHIFT;
}

stones_t south(stones_t stones) {
    return stones << V_SHIFT;
}

stones_t west(stones_t stones) {
    return (stones >> H_SHIFT) & WEST_BLOCK;
}

stones_t east(stones_t stones) {
    return (stones & WEST_BLOCK) << H_SHIFT;
}

stones_t liberties (stones_t stones, stones_t empty) {
    return (
        ((stones & WEST_BLOCK) << H_SHIFT) |
        ((stones >> H_SHIFT) & WEST_BLOCK) |
        (stones << V_SHIFT) |
        (stones >> V_SHIFT)
    ) & ~stones & empty;
}

int liberty_score(state *s) {
    stones_t player_controlled = s->player | liberties(s->player, s->playing_area & ~s->opponent);
    stones_t opponent_controlled = s->opponent | liberties(s->opponent, s->playing_area & ~s->player);
    return popcount(player_controlled) - popcount(opponent_controlled);
}

int make_move(state *s, stones_t move) {
    stones_t old_player = s->player;
    if (!move) {
        if (s->ko){
            s->ko = 0;
        }
        else {
            s->passes += 1;
        }
        s->player = s->opponent;
        s->opponent = old_player;
        assert(is_legal(s));
        return 1;
    }
    stones_t old_opponent = s->opponent;
    stones_t old_ko = s->ko;
    if (move & (s->player | s->opponent | s->ko | ~s->playing_area)) {
        return 0;
    }
    s->player |= move;
    stones_t kill = 0;
    stones_t empty = s->playing_area & ~s->player;
    stones_t chain = flood(north(move), s->opponent);
    if (!liberties(chain, empty)) {
        kill |= chain;
    }
    chain = flood(south(move), s->opponent);
    if (!liberties(chain, empty)) {
        kill |= chain;
    }
    chain = flood(west(move), s->opponent);
    if (!liberties(chain, empty)) {
        kill |= chain;
    }
    chain = flood(east(move), s->opponent);
    if (!liberties(chain, empty)) {
        kill |= chain;
    }

    s->opponent ^= kill;
    s->ko = 0;
    if (popcount(kill) == 1) {
        if (liberties(move, s->playing_area & ~s->opponent) == kill) {
            s->ko = kill;
        }
    }
    if (!liberties(flood(move, s->player), s->playing_area & ~s->opponent)) {
        s->player = old_player;
        s->opponent = old_opponent;
        s->ko = old_ko;
        return 0;
    }
    s->passes = 0;
    old_player = s->player;
    s->player = s->opponent;
    s->opponent = old_player;
    assert(is_legal(s));
    return 1;
}

int less_than(state *a, state *b) {
    if (a->player < b->player) {
        return 1;
    }
    else if (a->player == b->player){
        if (a->opponent < b->opponent) {
            return 1;
        }
        else if (a->opponent == b->opponent) {
            return a->ko < b->ko;
        }
    }
    return 0;
}

stones_t s_mirror_v(stones_t stones) {
    stones = (stones >> (3 * V_SHIFT)) | (stones & 0x3f000) | ((stones & 0xfff) << (3 * V_SHIFT));
    return ((stones >> V_SHIFT) & 0xfc003f) | (stones & 0x3f000) | ((stones & 0xfc003f) << V_SHIFT);
}

stones_t s_mirror_h(stones_t stones) {
    stones = ((stones >> (3 * H_SHIFT)) & 0x71c71c7) | ((stones & 0x71c71c7) << (3 * H_SHIFT));
    return ((stones >> (2 * H_SHIFT)) & 0x9249249) | (stones & 0x12492492) | ((stones & 0x9249249) << (2 * H_SHIFT));
}

void snap(state *s) {
    for (int i = 0; i < 30; i++) {
        if (s->playing_area & (1UL << i)){
            s->playing_area >>= i;
            s->player >>= i;
            s->opponent >>= i;
            s->ko >>= i;
            return;
        }
    }
}

void mirror_v(state *s) {
    s->playing_area = s_mirror_v(s->playing_area);
    s->player = s_mirror_v(s->player);
    s->opponent = s_mirror_v(s->opponent);
    s->ko = s_mirror_v(s->ko);
    snap(s);
}

void mirror_h(state *s) {
    s->playing_area = s_mirror_h(s->playing_area);
    s->player = s_mirror_h(s->player);
    s->opponent = s_mirror_h(s->opponent);
    s->ko = s_mirror_h(s->ko);
    snap(s);
}

void canonize(state *s) {
    state temp_ = *s;
    state *temp = &temp_;

    mirror_v(temp);
    if (less_than(temp, s)) {
        *s = *temp;
    }
    mirror_h(temp);
    if (less_than(temp, s)) {
        *s = *temp;
    }
    mirror_v(temp);
    if (less_than(temp, s)) {
        *s = *temp;
    }
}

int is_legal(state *s) {
    stones_t p;
    stones_t chain;
    for (int i = 0; i < 32; i += 2) {
        p = (1UL << i) | (1UL << (i + 1));
        chain = flood(p, s->player);
        if (chain && !liberties(chain, s->playing_area & ~s->opponent)) {
            return 0;
        }
        chain = flood(p, s->opponent);
        if (chain && !liberties(chain, s->playing_area & ~s->player)) {
            return 0;
        }
    }
    // Checking for ko legality omitted.
    return 1;
}

int from_key(state *s, size_t key) {
    s->player = 0;
    s->opponent = 0;
    s->ko = 0;
    s->passes = 0;
    stones_t p = 1UL;
    while (key) {
        if (p & s->playing_area){
            if (key % 3 == 1) {
                s->player |= p;
            }
            else if (key % 3 == 2) {
                s->opponent |= p;
            }
            key /= 3;
        }
        p <<= 1;
    }
    return is_legal(s);
}

size_t to_key(state *s) {
    size_t key = 0;
    stones_t p = 1UL << 31;
    while (p) {
        if (p & s->playing_area) {
            key *= 3;
            if (p & s->player) {
                key += 1;
            }
            else if (p & s->opponent) {
                key += 2;
            }
        }
        p >>= 1;
    }
    return key;
}

size_t max_key(state *s) {
    size_t key = 0;
    stones_t p = 1UL << 31;
    while (p) {
        if (p & s->playing_area){
            key *= 3;
            key += 2;
        }
        p >>= 1;
    }
    return key;
}

/*
int main() {
    stones_t a = 0x19bf3315;
    stones_t b = 1UL << (1 * H_SHIFT + 3 * V_SHIFT);
    stones_t c = flood(b, a);
    print_stones(a);
    printf("%d\n", popcount(a));
    print_stones(b);
    printf("%d\n", popcount(b));
    print_stones(c);

    state s_ = (state) {rectangle(5, 4), 0, 0, 0, 0};
    state *s = &s_;
    make_move(s, 1UL << (2 + 3 * V_SHIFT));
    make_move(s, 1UL << (4 + 1 * V_SHIFT));
    print_state(s);
    mirror_v(s);
    print_state(s);
    canonize(s);
    print_state(s);

    size_t key = to_key(s);
    printf("%zu\n", key);
    printf("%d\n", from_key(s, key));
    print_state(s);
    //srand(time(0));
    //for (int i = 0; i < 300; i++) {
    //    make_move(s, 1UL << (rand() % WIDTH + (rand() % HEIGTH) * V_SHIFT));
    //    print_state(s);
    //}

    return 0;
}
*/
