#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH (6)
#define HEIGTH (5)
#define STATE_SIZE (30)
#define H_SHIFT (1)
#define V_SHIFT (6)
#define D_SHIFT (5)
#define NORTH_WALL (0x3f)
#define WEST_WALL (0x1041041)
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
    printf(" passes = %d\n", s->passes);
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
        // assert(is_legal(s));
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
    // assert(is_legal(s));
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

stones_t s_mirror_d(stones_t stones) {
    return (
        (stones & 0x10204081) |
        ((stones & 0x408102) << D_SHIFT) |
        ((stones >> D_SHIFT) & 0x408102) |
        ((stones & 0x10204) << (2 * D_SHIFT)) |
        ((stones >> (2 * D_SHIFT)) & 0x10204) |
        ((stones & 0x408) << (3 * D_SHIFT)) |
        ((stones >> (3 * D_SHIFT)) & 0x408) |
        ((stones & 0x10) << (4 * D_SHIFT)) |
        ((stones >> (4 * D_SHIFT)) & 0x10)
    );
    return 0;
}

void snap(state *s) {
    for (int i = 0; i < WIDTH; i++) {
        if (s->playing_area & (WEST_WALL << i)){
            s->playing_area >>= i;
            s->player >>= i;
            s->opponent >>= i;
            s->ko >>= i;
            break;
        }
    }
    for (int i = 0; i < HEIGTH * V_SHIFT; i += V_SHIFT) {
        if (s->playing_area & (NORTH_WALL << i)){
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

void mirror_d(state *s) {
    s->player = s_mirror_d(s->player);
    s->opponent = s_mirror_d(s->opponent);
    s->ko = s_mirror_d(s->ko);
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

    if (s->playing_area == s_mirror_d(s->playing_area) && 0) {
        mirror_d(temp);
        if (less_than(temp, s)) {
            *s = *temp;
        }
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

size_t to_key_alt(state *s) {
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

// Assumes rectangular board.
#define FOUR_SLICE (0xf)
#define FIVE_SLICE (0x1f)
static size_t FOUR_KEYS[256] = {0, 1, 3, 4, 9, 10, 12, 13, 27, 28, 30, 31, 36, 37, 39, 40, 2, 1, 5, 4, 11, 10, 14, 13, 29, 28, 32, 31, 38, 37, 41, 40, 6, 7, 3, 4, 15, 16, 12, 13, 33, 34, 30, 31, 42, 43, 39, 40, 8, 7, 5, 4, 17, 16, 14, 13, 35, 34, 32, 31, 44, 43, 41, 40, 18, 19, 21, 22, 9, 10, 12, 13, 45, 46, 48, 49, 36, 37, 39, 40, 20, 19, 23, 22, 11, 10, 14, 13, 47, 46, 50, 49, 38, 37, 41, 40, 24, 25, 21, 22, 15, 16, 12, 13, 51, 52, 48, 49, 42, 43, 39, 40, 26, 25, 23, 22, 17, 16, 14, 13, 53, 52, 50, 49, 44, 43, 41, 40, 54, 55, 57, 58, 63, 64, 66, 67, 27, 28, 30, 31, 36, 37, 39, 40, 56, 55, 59, 58, 65, 64, 68, 67, 29, 28, 32, 31, 38, 37, 41, 40, 60, 61, 57, 58, 69, 70, 66, 67, 33, 34, 30, 31, 42, 43, 39, 40, 62, 61, 59, 58, 71, 70, 68, 67, 35, 34, 32, 31, 44, 43, 41, 40, 72, 73, 75, 76, 63, 64, 66, 67, 45, 46, 48, 49, 36, 37, 39, 40, 74, 73, 77, 76, 65, 64, 68, 67, 47, 46, 50, 49, 38, 37, 41, 40, 78, 79, 75, 76, 69, 70, 66, 67, 51, 52, 48, 49, 42, 43, 39, 40, 80, 79, 77, 76, 71, 70, 68, 67, 53, 52, 50, 49, 44, 43, 41, 40};
static size_t FIVE_KEYS[1024] = {0, 1, 3, 4, 9, 10, 12, 13, 27, 28, 30, 31, 36, 37, 39, 40, 81, 82, 84, 85, 90, 91, 93, 94, 108, 109, 111, 112, 117, 118, 120, 121, 2, 1, 5, 4, 11, 10, 14, 13, 29, 28, 32, 31, 38, 37, 41, 40, 83, 82, 86, 85, 92, 91, 95, 94, 110, 109, 113, 112, 119, 118, 122, 121, 6, 7, 3, 4, 15, 16, 12, 13, 33, 34, 30, 31, 42, 43, 39, 40, 87, 88, 84, 85, 96, 97, 93, 94, 114, 115, 111, 112, 123, 124, 120, 121, 8, 7, 5, 4, 17, 16, 14, 13, 35, 34, 32, 31, 44, 43, 41, 40, 89, 88, 86, 85, 98, 97, 95, 94, 116, 115, 113, 112, 125, 124, 122, 121, 18, 19, 21, 22, 9, 10, 12, 13, 45, 46, 48, 49, 36, 37, 39, 40, 99, 100, 102, 103, 90, 91, 93, 94, 126, 127, 129, 130, 117, 118, 120, 121, 20, 19, 23, 22, 11, 10, 14, 13, 47, 46, 50, 49, 38, 37, 41, 40, 101, 100, 104, 103, 92, 91, 95, 94, 128, 127, 131, 130, 119, 118, 122, 121, 24, 25, 21, 22, 15, 16, 12, 13, 51, 52, 48, 49, 42, 43, 39, 40, 105, 106, 102, 103, 96, 97, 93, 94, 132, 133, 129, 130, 123, 124, 120, 121, 26, 25, 23, 22, 17, 16, 14, 13, 53, 52, 50, 49, 44, 43, 41, 40, 107, 106, 104, 103, 98, 97, 95, 94, 134, 133, 131, 130, 125, 124, 122, 121, 54, 55, 57, 58, 63, 64, 66, 67, 27, 28, 30, 31, 36, 37, 39, 40, 135, 136, 138, 139, 144, 145, 147, 148, 108, 109, 111, 112, 117, 118, 120, 121, 56, 55, 59, 58, 65, 64, 68, 67, 29, 28, 32, 31, 38, 37, 41, 40, 137, 136, 140, 139, 146, 145, 149, 148, 110, 109, 113, 112, 119, 118, 122, 121, 60, 61, 57, 58, 69, 70, 66, 67, 33, 34, 30, 31, 42, 43, 39, 40, 141, 142, 138, 139, 150, 151, 147, 148, 114, 115, 111, 112, 123, 124, 120, 121, 62, 61, 59, 58, 71, 70, 68, 67, 35, 34, 32, 31, 44, 43, 41, 40, 143, 142, 140, 139, 152, 151, 149, 148, 116, 115, 113, 112, 125, 124, 122, 121, 72, 73, 75, 76, 63, 64, 66, 67, 45, 46, 48, 49, 36, 37, 39, 40, 153, 154, 156, 157, 144, 145, 147, 148, 126, 127, 129, 130, 117, 118, 120, 121, 74, 73, 77, 76, 65, 64, 68, 67, 47, 46, 50, 49, 38, 37, 41, 40, 155, 154, 158, 157, 146, 145, 149, 148, 128, 127, 131, 130, 119, 118, 122, 121, 78, 79, 75, 76, 69, 70, 66, 67, 51, 52, 48, 49, 42, 43, 39, 40, 159, 160, 156, 157, 150, 151, 147, 148, 132, 133, 129, 130, 123, 124, 120, 121, 80, 79, 77, 76, 71, 70, 68, 67, 53, 52, 50, 49, 44, 43, 41, 40, 161, 160, 158, 157, 152, 151, 149, 148, 134, 133, 131, 130, 125, 124, 122, 121, 162, 163, 165, 166, 171, 172, 174, 175, 189, 190, 192, 193, 198, 199, 201, 202, 81, 82, 84, 85, 90, 91, 93, 94, 108, 109, 111, 112, 117, 118, 120, 121, 164, 163, 167, 166, 173, 172, 176, 175, 191, 190, 194, 193, 200, 199, 203, 202, 83, 82, 86, 85, 92, 91, 95, 94, 110, 109, 113, 112, 119, 118, 122, 121, 168, 169, 165, 166, 177, 178, 174, 175, 195, 196, 192, 193, 204, 205, 201, 202, 87, 88, 84, 85, 96, 97, 93, 94, 114, 115, 111, 112, 123, 124, 120, 121, 170, 169, 167, 166, 179, 178, 176, 175, 197, 196, 194, 193, 206, 205, 203, 202, 89, 88, 86, 85, 98, 97, 95, 94, 116, 115, 113, 112, 125, 124, 122, 121, 180, 181, 183, 184, 171, 172, 174, 175, 207, 208, 210, 211, 198, 199, 201, 202, 99, 100, 102, 103, 90, 91, 93, 94, 126, 127, 129, 130, 117, 118, 120, 121, 182, 181, 185, 184, 173, 172, 176, 175, 209, 208, 212, 211, 200, 199, 203, 202, 101, 100, 104, 103, 92, 91, 95, 94, 128, 127, 131, 130, 119, 118, 122, 121, 186, 187, 183, 184, 177, 178, 174, 175, 213, 214, 210, 211, 204, 205, 201, 202, 105, 106, 102, 103, 96, 97, 93, 94, 132, 133, 129, 130, 123, 124, 120, 121, 188, 187, 185, 184, 179, 178, 176, 175, 215, 214, 212, 211, 206, 205, 203, 202, 107, 106, 104, 103, 98, 97, 95, 94, 134, 133, 131, 130, 125, 124, 122, 121, 216, 217, 219, 220, 225, 226, 228, 229, 189, 190, 192, 193, 198, 199, 201, 202, 135, 136, 138, 139, 144, 145, 147, 148, 108, 109, 111, 112, 117, 118, 120, 121, 218, 217, 221, 220, 227, 226, 230, 229, 191, 190, 194, 193, 200, 199, 203, 202, 137, 136, 140, 139, 146, 145, 149, 148, 110, 109, 113, 112, 119, 118, 122, 121, 222, 223, 219, 220, 231, 232, 228, 229, 195, 196, 192, 193, 204, 205, 201, 202, 141, 142, 138, 139, 150, 151, 147, 148, 114, 115, 111, 112, 123, 124, 120, 121, 224, 223, 221, 220, 233, 232, 230, 229, 197, 196, 194, 193, 206, 205, 203, 202, 143, 142, 140, 139, 152, 151, 149, 148, 116, 115, 113, 112, 125, 124, 122, 121, 234, 235, 237, 238, 225, 226, 228, 229, 207, 208, 210, 211, 198, 199, 201, 202, 153, 154, 156, 157, 144, 145, 147, 148, 126, 127, 129, 130, 117, 118, 120, 121, 236, 235, 239, 238, 227, 226, 230, 229, 209, 208, 212, 211, 200, 199, 203, 202, 155, 154, 158, 157, 146, 145, 149, 148, 128, 127, 131, 130, 119, 118, 122, 121, 240, 241, 237, 238, 231, 232, 228, 229, 213, 214, 210, 211, 204, 205, 201, 202, 159, 160, 156, 157, 150, 151, 147, 148, 132, 133, 129, 130, 123, 124, 120, 121, 242, 241, 239, 238, 233, 232, 230, 229, 215, 214, 212, 211, 206, 205, 203, 202, 161, 160, 158, 157, 152, 151, 149, 148, 134, 133, 131, 130, 125, 124, 122, 121};

size_t to_key(state *s) {
    if ((s->playing_area & NORTH_WALL) == FOUR_SLICE) {
        size_t key = 0;
        for (int i = (HEIGTH - 1) * V_SHIFT; i >= 0; i -= V_SHIFT) {
            key *= 81;
            key += FOUR_KEYS[((s->player >> i) & FOUR_SLICE) | (((s->opponent >> i) & FOUR_SLICE) << 4)];
        }
        return key;
    }
    else if ((s->playing_area & NORTH_WALL) == FIVE_SLICE) {
        size_t key = 0;
        for (int i = (HEIGTH - 1) * V_SHIFT; i >= 0; i -= V_SHIFT) {
            key *= 243;
            key += FIVE_KEYS[((s->player >> i) & FIVE_SLICE) | (((s->opponent >> i) & FIVE_SLICE) << 5)];
        }
        return key;
    }
    else {
        return to_key_alt(s);
    }
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

#ifndef MAIN
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

    print_stones(0x10204081);
    print_stones(0x408102);
    print_stones(0x10204);
    print_stones(0x408);
    print_stones(0x10);
    print_stones(a);
    print_stones(s_mirror_d(a));
    return 0;
}
#endif
