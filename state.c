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

#define FOUR_SLICE (0xf)
#define FOUR_SLICE_V (0x41041)
#define FIVE_SLICE (0x1f)

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


stones_t flood(register stones_t source, register stones_t target) {
    source &= target;
    if (!source){
        return source;
    }
    register stones_t temp;
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

stones_t s_mirror_v4(stones_t stones) {
    stones = (stones >> (2 * V_SHIFT)) | ((stones & 0xfff) << (2 * V_SHIFT));
    return ((stones >> V_SHIFT) & 0x3f03f) | ((stones & 0x3f03f) << V_SHIFT);
}

stones_t s_mirror_h(stones_t stones) {
    stones = ((stones >> (3 * H_SHIFT)) & 0x71c71c7) | ((stones & 0x71c71c7) << (3 * H_SHIFT));
    return ((stones >> (2 * H_SHIFT)) & 0x9249249) | (stones & 0x12492492) | ((stones & 0x9249249) << (2 * H_SHIFT));
}

stones_t s_mirror_h5(stones_t stones) {
    return (
        (stones & 0x4104104) |
        ((stones & 0x2082082) << (2 * H_SHIFT)) |
        ((stones >> (2 * H_SHIFT)) & 0x2082082) |
        ((stones & WEST_WALL) << (4 * H_SHIFT)) |
        ((stones >> (4 * H_SHIFT)) & WEST_WALL)
    );
}

stones_t s_mirror_h4(stones_t stones) {
    stones = ((stones >> (2 * H_SHIFT)) & 0x30c30c3) | ((stones & 0x30c30c3) << (2 * H_SHIFT));
    return ((stones >> H_SHIFT) & 0x5145145) | ((stones & 0x5145145) << H_SHIFT);
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
    if ((s->playing_area & WEST_WALL) == FOUR_SLICE_V) {
        s->player = s_mirror_v4(s->player);
        s->opponent = s_mirror_v4(s->opponent);
        s->ko = s_mirror_v4(s->ko);
    }
    else {
        s->player = s_mirror_v(s->player);
        s->opponent = s_mirror_v(s->opponent);
        s->ko = s_mirror_v(s->ko);
        if ((s->playing_area & WEST_WALL) != WEST_WALL) {
            s->playing_area = s_mirror_v(s->playing_area);
            snap(s);
        }
    }
}

void mirror_h(state *s) {
    if ((s->playing_area & NORTH_WALL) == FOUR_SLICE) {
        s->player = s_mirror_h4(s->player);
        s->opponent = s_mirror_h4(s->opponent);
        s->ko = s_mirror_h4(s->ko);
    }
    else if ((s->playing_area & NORTH_WALL) == FIVE_SLICE) {
        s->player = s_mirror_h5(s->player);
        s->opponent = s_mirror_h5(s->opponent);
        s->ko = s_mirror_h5(s->ko);
    }
    else {
        s->player = s_mirror_h(s->player);
        s->opponent = s_mirror_h(s->opponent);
        s->ko = s_mirror_h(s->ko);
        if ((s->playing_area & NORTH_WALL) != NORTH_WALL){
            s->playing_area = s_mirror_h(s->playing_area);
            snap(s);
        }
    }
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
static size_t FOUR_KEYS[256] = {0, 1, 3, 4, 9, 10, 12, 13, 27, 28, 30, 31, 36, 37, 39, 40, 2, 1, 5, 4, 11, 10, 14, 13, 29, 28, 32, 31, 38, 37, 41, 40, 6, 7, 3, 4, 15, 16, 12, 13, 33, 34, 30, 31, 42, 43, 39, 40, 8, 7, 5, 4, 17, 16, 14, 13, 35, 34, 32, 31, 44, 43, 41, 40, 18, 19, 21, 22, 9, 10, 12, 13, 45, 46, 48, 49, 36, 37, 39, 40, 20, 19, 23, 22, 11, 10, 14, 13, 47, 46, 50, 49, 38, 37, 41, 40, 24, 25, 21, 22, 15, 16, 12, 13, 51, 52, 48, 49, 42, 43, 39, 40, 26, 25, 23, 22, 17, 16, 14, 13, 53, 52, 50, 49, 44, 43, 41, 40, 54, 55, 57, 58, 63, 64, 66, 67, 27, 28, 30, 31, 36, 37, 39, 40, 56, 55, 59, 58, 65, 64, 68, 67, 29, 28, 32, 31, 38, 37, 41, 40, 60, 61, 57, 58, 69, 70, 66, 67, 33, 34, 30, 31, 42, 43, 39, 40, 62, 61, 59, 58, 71, 70, 68, 67, 35, 34, 32, 31, 44, 43, 41, 40, 72, 73, 75, 76, 63, 64, 66, 67, 45, 46, 48, 49, 36, 37, 39, 40, 74, 73, 77, 76, 65, 64, 68, 67, 47, 46, 50, 49, 38, 37, 41, 40, 78, 79, 75, 76, 69, 70, 66, 67, 51, 52, 48, 49, 42, 43, 39, 40, 80, 79, 77, 76, 71, 70, 68, 67, 53, 52, 50, 49, 44, 43, 41, 40};
static size_t FIVE_KEYS[1024] = {0, 1, 3, 4, 9, 10, 12, 13, 27, 28, 30, 31, 36, 37, 39, 40, 81, 82, 84, 85, 90, 91, 93, 94, 108, 109, 111, 112, 117, 118, 120, 121, 2, 1, 5, 4, 11, 10, 14, 13, 29, 28, 32, 31, 38, 37, 41, 40, 83, 82, 86, 85, 92, 91, 95, 94, 110, 109, 113, 112, 119, 118, 122, 121, 6, 7, 3, 4, 15, 16, 12, 13, 33, 34, 30, 31, 42, 43, 39, 40, 87, 88, 84, 85, 96, 97, 93, 94, 114, 115, 111, 112, 123, 124, 120, 121, 8, 7, 5, 4, 17, 16, 14, 13, 35, 34, 32, 31, 44, 43, 41, 40, 89, 88, 86, 85, 98, 97, 95, 94, 116, 115, 113, 112, 125, 124, 122, 121, 18, 19, 21, 22, 9, 10, 12, 13, 45, 46, 48, 49, 36, 37, 39, 40, 99, 100, 102, 103, 90, 91, 93, 94, 126, 127, 129, 130, 117, 118, 120, 121, 20, 19, 23, 22, 11, 10, 14, 13, 47, 46, 50, 49, 38, 37, 41, 40, 101, 100, 104, 103, 92, 91, 95, 94, 128, 127, 131, 130, 119, 118, 122, 121, 24, 25, 21, 22, 15, 16, 12, 13, 51, 52, 48, 49, 42, 43, 39, 40, 105, 106, 102, 103, 96, 97, 93, 94, 132, 133, 129, 130, 123, 124, 120, 121, 26, 25, 23, 22, 17, 16, 14, 13, 53, 52, 50, 49, 44, 43, 41, 40, 107, 106, 104, 103, 98, 97, 95, 94, 134, 133, 131, 130, 125, 124, 122, 121, 54, 55, 57, 58, 63, 64, 66, 67, 27, 28, 30, 31, 36, 37, 39, 40, 135, 136, 138, 139, 144, 145, 147, 148, 108, 109, 111, 112, 117, 118, 120, 121, 56, 55, 59, 58, 65, 64, 68, 67, 29, 28, 32, 31, 38, 37, 41, 40, 137, 136, 140, 139, 146, 145, 149, 148, 110, 109, 113, 112, 119, 118, 122, 121, 60, 61, 57, 58, 69, 70, 66, 67, 33, 34, 30, 31, 42, 43, 39, 40, 141, 142, 138, 139, 150, 151, 147, 148, 114, 115, 111, 112, 123, 124, 120, 121, 62, 61, 59, 58, 71, 70, 68, 67, 35, 34, 32, 31, 44, 43, 41, 40, 143, 142, 140, 139, 152, 151, 149, 148, 116, 115, 113, 112, 125, 124, 122, 121, 72, 73, 75, 76, 63, 64, 66, 67, 45, 46, 48, 49, 36, 37, 39, 40, 153, 154, 156, 157, 144, 145, 147, 148, 126, 127, 129, 130, 117, 118, 120, 121, 74, 73, 77, 76, 65, 64, 68, 67, 47, 46, 50, 49, 38, 37, 41, 40, 155, 154, 158, 157, 146, 145, 149, 148, 128, 127, 131, 130, 119, 118, 122, 121, 78, 79, 75, 76, 69, 70, 66, 67, 51, 52, 48, 49, 42, 43, 39, 40, 159, 160, 156, 157, 150, 151, 147, 148, 132, 133, 129, 130, 123, 124, 120, 121, 80, 79, 77, 76, 71, 70, 68, 67, 53, 52, 50, 49, 44, 43, 41, 40, 161, 160, 158, 157, 152, 151, 149, 148, 134, 133, 131, 130, 125, 124, 122, 121, 162, 163, 165, 166, 171, 172, 174, 175, 189, 190, 192, 193, 198, 199, 201, 202, 81, 82, 84, 85, 90, 91, 93, 94, 108, 109, 111, 112, 117, 118, 120, 121, 164, 163, 167, 166, 173, 172, 176, 175, 191, 190, 194, 193, 200, 199, 203, 202, 83, 82, 86, 85, 92, 91, 95, 94, 110, 109, 113, 112, 119, 118, 122, 121, 168, 169, 165, 166, 177, 178, 174, 175, 195, 196, 192, 193, 204, 205, 201, 202, 87, 88, 84, 85, 96, 97, 93, 94, 114, 115, 111, 112, 123, 124, 120, 121, 170, 169, 167, 166, 179, 178, 176, 175, 197, 196, 194, 193, 206, 205, 203, 202, 89, 88, 86, 85, 98, 97, 95, 94, 116, 115, 113, 112, 125, 124, 122, 121, 180, 181, 183, 184, 171, 172, 174, 175, 207, 208, 210, 211, 198, 199, 201, 202, 99, 100, 102, 103, 90, 91, 93, 94, 126, 127, 129, 130, 117, 118, 120, 121, 182, 181, 185, 184, 173, 172, 176, 175, 209, 208, 212, 211, 200, 199, 203, 202, 101, 100, 104, 103, 92, 91, 95, 94, 128, 127, 131, 130, 119, 118, 122, 121, 186, 187, 183, 184, 177, 178, 174, 175, 213, 214, 210, 211, 204, 205, 201, 202, 105, 106, 102, 103, 96, 97, 93, 94, 132, 133, 129, 130, 123, 124, 120, 121, 188, 187, 185, 184, 179, 178, 176, 175, 215, 214, 212, 211, 206, 205, 203, 202, 107, 106, 104, 103, 98, 97, 95, 94, 134, 133, 131, 130, 125, 124, 122, 121, 216, 217, 219, 220, 225, 226, 228, 229, 189, 190, 192, 193, 198, 199, 201, 202, 135, 136, 138, 139, 144, 145, 147, 148, 108, 109, 111, 112, 117, 118, 120, 121, 218, 217, 221, 220, 227, 226, 230, 229, 191, 190, 194, 193, 200, 199, 203, 202, 137, 136, 140, 139, 146, 145, 149, 148, 110, 109, 113, 112, 119, 118, 122, 121, 222, 223, 219, 220, 231, 232, 228, 229, 195, 196, 192, 193, 204, 205, 201, 202, 141, 142, 138, 139, 150, 151, 147, 148, 114, 115, 111, 112, 123, 124, 120, 121, 224, 223, 221, 220, 233, 232, 230, 229, 197, 196, 194, 193, 206, 205, 203, 202, 143, 142, 140, 139, 152, 151, 149, 148, 116, 115, 113, 112, 125, 124, 122, 121, 234, 235, 237, 238, 225, 226, 228, 229, 207, 208, 210, 211, 198, 199, 201, 202, 153, 154, 156, 157, 144, 145, 147, 148, 126, 127, 129, 130, 117, 118, 120, 121, 236, 235, 239, 238, 227, 226, 230, 229, 209, 208, 212, 211, 200, 199, 203, 202, 155, 154, 158, 157, 146, 145, 149, 148, 128, 127, 131, 130, 119, 118, 122, 121, 240, 241, 237, 238, 231, 232, 228, 229, 213, 214, 210, 211, 204, 205, 201, 202, 159, 160, 156, 157, 150, 151, 147, 148, 132, 133, 129, 130, 123, 124, 120, 121, 242, 241, 239, 238, 233, 232, 230, 229, 215, 214, 212, 211, 206, 205, 203, 202, 161, 160, 158, 157, 152, 151, 149, 148, 134, 133, 131, 130, 125, 124, 122, 121};
static size_t SIX_KEYS[4096] = {0, 1, 3, 4, 9, 10, 12, 13, 27, 28, 30, 31, 36, 37, 39, 40, 81, 82, 84, 85, 90, 91, 93, 94, 108, 109, 111, 112, 117, 118, 120, 121, 243, 244, 246, 247, 252, 253, 255, 256, 270, 271, 273, 274, 279, 280, 282, 283, 324, 325, 327, 328, 333, 334, 336, 337, 351, 352, 354, 355, 360, 361, 363, 364, 2, 1, 5, 4, 11, 10, 14, 13, 29, 28, 32, 31, 38, 37, 41, 40, 83, 82, 86, 85, 92, 91, 95, 94, 110, 109, 113, 112, 119, 118, 122, 121, 245, 244, 248, 247, 254, 253, 257, 256, 272, 271, 275, 274, 281, 280, 284, 283, 326, 325, 329, 328, 335, 334, 338, 337, 353, 352, 356, 355, 362, 361, 365, 364, 6, 7, 3, 4, 15, 16, 12, 13, 33, 34, 30, 31, 42, 43, 39, 40, 87, 88, 84, 85, 96, 97, 93, 94, 114, 115, 111, 112, 123, 124, 120, 121, 249, 250, 246, 247, 258, 259, 255, 256, 276, 277, 273, 274, 285, 286, 282, 283, 330, 331, 327, 328, 339, 340, 336, 337, 357, 358, 354, 355, 366, 367, 363, 364, 8, 7, 5, 4, 17, 16, 14, 13, 35, 34, 32, 31, 44, 43, 41, 40, 89, 88, 86, 85, 98, 97, 95, 94, 116, 115, 113, 112, 125, 124, 122, 121, 251, 250, 248, 247, 260, 259, 257, 256, 278, 277, 275, 274, 287, 286, 284, 283, 332, 331, 329, 328, 341, 340, 338, 337, 359, 358, 356, 355, 368, 367, 365, 364, 18, 19, 21, 22, 9, 10, 12, 13, 45, 46, 48, 49, 36, 37, 39, 40, 99, 100, 102, 103, 90, 91, 93, 94, 126, 127, 129, 130, 117, 118, 120, 121, 261, 262, 264, 265, 252, 253, 255, 256, 288, 289, 291, 292, 279, 280, 282, 283, 342, 343, 345, 346, 333, 334, 336, 337, 369, 370, 372, 373, 360, 361, 363, 364, 20, 19, 23, 22, 11, 10, 14, 13, 47, 46, 50, 49, 38, 37, 41, 40, 101, 100, 104, 103, 92, 91, 95, 94, 128, 127, 131, 130, 119, 118, 122, 121, 263, 262, 266, 265, 254, 253, 257, 256, 290, 289, 293, 292, 281, 280, 284, 283, 344, 343, 347, 346, 335, 334, 338, 337, 371, 370, 374, 373, 362, 361, 365, 364, 24, 25, 21, 22, 15, 16, 12, 13, 51, 52, 48, 49, 42, 43, 39, 40, 105, 106, 102, 103, 96, 97, 93, 94, 132, 133, 129, 130, 123, 124, 120, 121, 267, 268, 264, 265, 258, 259, 255, 256, 294, 295, 291, 292, 285, 286, 282, 283, 348, 349, 345, 346, 339, 340, 336, 337, 375, 376, 372, 373, 366, 367, 363, 364, 26, 25, 23, 22, 17, 16, 14, 13, 53, 52, 50, 49, 44, 43, 41, 40, 107, 106, 104, 103, 98, 97, 95, 94, 134, 133, 131, 130, 125, 124, 122, 121, 269, 268, 266, 265, 260, 259, 257, 256, 296, 295, 293, 292, 287, 286, 284, 283, 350, 349, 347, 346, 341, 340, 338, 337, 377, 376, 374, 373, 368, 367, 365, 364, 54, 55, 57, 58, 63, 64, 66, 67, 27, 28, 30, 31, 36, 37, 39, 40, 135, 136, 138, 139, 144, 145, 147, 148, 108, 109, 111, 112, 117, 118, 120, 121, 297, 298, 300, 301, 306, 307, 309, 310, 270, 271, 273, 274, 279, 280, 282, 283, 378, 379, 381, 382, 387, 388, 390, 391, 351, 352, 354, 355, 360, 361, 363, 364, 56, 55, 59, 58, 65, 64, 68, 67, 29, 28, 32, 31, 38, 37, 41, 40, 137, 136, 140, 139, 146, 145, 149, 148, 110, 109, 113, 112, 119, 118, 122, 121, 299, 298, 302, 301, 308, 307, 311, 310, 272, 271, 275, 274, 281, 280, 284, 283, 380, 379, 383, 382, 389, 388, 392, 391, 353, 352, 356, 355, 362, 361, 365, 364, 60, 61, 57, 58, 69, 70, 66, 67, 33, 34, 30, 31, 42, 43, 39, 40, 141, 142, 138, 139, 150, 151, 147, 148, 114, 115, 111, 112, 123, 124, 120, 121, 303, 304, 300, 301, 312, 313, 309, 310, 276, 277, 273, 274, 285, 286, 282, 283, 384, 385, 381, 382, 393, 394, 390, 391, 357, 358, 354, 355, 366, 367, 363, 364, 62, 61, 59, 58, 71, 70, 68, 67, 35, 34, 32, 31, 44, 43, 41, 40, 143, 142, 140, 139, 152, 151, 149, 148, 116, 115, 113, 112, 125, 124, 122, 121, 305, 304, 302, 301, 314, 313, 311, 310, 278, 277, 275, 274, 287, 286, 284, 283, 386, 385, 383, 382, 395, 394, 392, 391, 359, 358, 356, 355, 368, 367, 365, 364, 72, 73, 75, 76, 63, 64, 66, 67, 45, 46, 48, 49, 36, 37, 39, 40, 153, 154, 156, 157, 144, 145, 147, 148, 126, 127, 129, 130, 117, 118, 120, 121, 315, 316, 318, 319, 306, 307, 309, 310, 288, 289, 291, 292, 279, 280, 282, 283, 396, 397, 399, 400, 387, 388, 390, 391, 369, 370, 372, 373, 360, 361, 363, 364, 74, 73, 77, 76, 65, 64, 68, 67, 47, 46, 50, 49, 38, 37, 41, 40, 155, 154, 158, 157, 146, 145, 149, 148, 128, 127, 131, 130, 119, 118, 122, 121, 317, 316, 320, 319, 308, 307, 311, 310, 290, 289, 293, 292, 281, 280, 284, 283, 398, 397, 401, 400, 389, 388, 392, 391, 371, 370, 374, 373, 362, 361, 365, 364, 78, 79, 75, 76, 69, 70, 66, 67, 51, 52, 48, 49, 42, 43, 39, 40, 159, 160, 156, 157, 150, 151, 147, 148, 132, 133, 129, 130, 123, 124, 120, 121, 321, 322, 318, 319, 312, 313, 309, 310, 294, 295, 291, 292, 285, 286, 282, 283, 402, 403, 399, 400, 393, 394, 390, 391, 375, 376, 372, 373, 366, 367, 363, 364, 80, 79, 77, 76, 71, 70, 68, 67, 53, 52, 50, 49, 44, 43, 41, 40, 161, 160, 158, 157, 152, 151, 149, 148, 134, 133, 131, 130, 125, 124, 122, 121, 323, 322, 320, 319, 314, 313, 311, 310, 296, 295, 293, 292, 287, 286, 284, 283, 404, 403, 401, 400, 395, 394, 392, 391, 377, 376, 374, 373, 368, 367, 365, 364, 162, 163, 165, 166, 171, 172, 174, 175, 189, 190, 192, 193, 198, 199, 201, 202, 81, 82, 84, 85, 90, 91, 93, 94, 108, 109, 111, 112, 117, 118, 120, 121, 405, 406, 408, 409, 414, 415, 417, 418, 432, 433, 435, 436, 441, 442, 444, 445, 324, 325, 327, 328, 333, 334, 336, 337, 351, 352, 354, 355, 360, 361, 363, 364, 164, 163, 167, 166, 173, 172, 176, 175, 191, 190, 194, 193, 200, 199, 203, 202, 83, 82, 86, 85, 92, 91, 95, 94, 110, 109, 113, 112, 119, 118, 122, 121, 407, 406, 410, 409, 416, 415, 419, 418, 434, 433, 437, 436, 443, 442, 446, 445, 326, 325, 329, 328, 335, 334, 338, 337, 353, 352, 356, 355, 362, 361, 365, 364, 168, 169, 165, 166, 177, 178, 174, 175, 195, 196, 192, 193, 204, 205, 201, 202, 87, 88, 84, 85, 96, 97, 93, 94, 114, 115, 111, 112, 123, 124, 120, 121, 411, 412, 408, 409, 420, 421, 417, 418, 438, 439, 435, 436, 447, 448, 444, 445, 330, 331, 327, 328, 339, 340, 336, 337, 357, 358, 354, 355, 366, 367, 363, 364, 170, 169, 167, 166, 179, 178, 176, 175, 197, 196, 194, 193, 206, 205, 203, 202, 89, 88, 86, 85, 98, 97, 95, 94, 116, 115, 113, 112, 125, 124, 122, 121, 413, 412, 410, 409, 422, 421, 419, 418, 440, 439, 437, 436, 449, 448, 446, 445, 332, 331, 329, 328, 341, 340, 338, 337, 359, 358, 356, 355, 368, 367, 365, 364, 180, 181, 183, 184, 171, 172, 174, 175, 207, 208, 210, 211, 198, 199, 201, 202, 99, 100, 102, 103, 90, 91, 93, 94, 126, 127, 129, 130, 117, 118, 120, 121, 423, 424, 426, 427, 414, 415, 417, 418, 450, 451, 453, 454, 441, 442, 444, 445, 342, 343, 345, 346, 333, 334, 336, 337, 369, 370, 372, 373, 360, 361, 363, 364, 182, 181, 185, 184, 173, 172, 176, 175, 209, 208, 212, 211, 200, 199, 203, 202, 101, 100, 104, 103, 92, 91, 95, 94, 128, 127, 131, 130, 119, 118, 122, 121, 425, 424, 428, 427, 416, 415, 419, 418, 452, 451, 455, 454, 443, 442, 446, 445, 344, 343, 347, 346, 335, 334, 338, 337, 371, 370, 374, 373, 362, 361, 365, 364, 186, 187, 183, 184, 177, 178, 174, 175, 213, 214, 210, 211, 204, 205, 201, 202, 105, 106, 102, 103, 96, 97, 93, 94, 132, 133, 129, 130, 123, 124, 120, 121, 429, 430, 426, 427, 420, 421, 417, 418, 456, 457, 453, 454, 447, 448, 444, 445, 348, 349, 345, 346, 339, 340, 336, 337, 375, 376, 372, 373, 366, 367, 363, 364, 188, 187, 185, 184, 179, 178, 176, 175, 215, 214, 212, 211, 206, 205, 203, 202, 107, 106, 104, 103, 98, 97, 95, 94, 134, 133, 131, 130, 125, 124, 122, 121, 431, 430, 428, 427, 422, 421, 419, 418, 458, 457, 455, 454, 449, 448, 446, 445, 350, 349, 347, 346, 341, 340, 338, 337, 377, 376, 374, 373, 368, 367, 365, 364, 216, 217, 219, 220, 225, 226, 228, 229, 189, 190, 192, 193, 198, 199, 201, 202, 135, 136, 138, 139, 144, 145, 147, 148, 108, 109, 111, 112, 117, 118, 120, 121, 459, 460, 462, 463, 468, 469, 471, 472, 432, 433, 435, 436, 441, 442, 444, 445, 378, 379, 381, 382, 387, 388, 390, 391, 351, 352, 354, 355, 360, 361, 363, 364, 218, 217, 221, 220, 227, 226, 230, 229, 191, 190, 194, 193, 200, 199, 203, 202, 137, 136, 140, 139, 146, 145, 149, 148, 110, 109, 113, 112, 119, 118, 122, 121, 461, 460, 464, 463, 470, 469, 473, 472, 434, 433, 437, 436, 443, 442, 446, 445, 380, 379, 383, 382, 389, 388, 392, 391, 353, 352, 356, 355, 362, 361, 365, 364, 222, 223, 219, 220, 231, 232, 228, 229, 195, 196, 192, 193, 204, 205, 201, 202, 141, 142, 138, 139, 150, 151, 147, 148, 114, 115, 111, 112, 123, 124, 120, 121, 465, 466, 462, 463, 474, 475, 471, 472, 438, 439, 435, 436, 447, 448, 444, 445, 384, 385, 381, 382, 393, 394, 390, 391, 357, 358, 354, 355, 366, 367, 363, 364, 224, 223, 221, 220, 233, 232, 230, 229, 197, 196, 194, 193, 206, 205, 203, 202, 143, 142, 140, 139, 152, 151, 149, 148, 116, 115, 113, 112, 125, 124, 122, 121, 467, 466, 464, 463, 476, 475, 473, 472, 440, 439, 437, 436, 449, 448, 446, 445, 386, 385, 383, 382, 395, 394, 392, 391, 359, 358, 356, 355, 368, 367, 365, 364, 234, 235, 237, 238, 225, 226, 228, 229, 207, 208, 210, 211, 198, 199, 201, 202, 153, 154, 156, 157, 144, 145, 147, 148, 126, 127, 129, 130, 117, 118, 120, 121, 477, 478, 480, 481, 468, 469, 471, 472, 450, 451, 453, 454, 441, 442, 444, 445, 396, 397, 399, 400, 387, 388, 390, 391, 369, 370, 372, 373, 360, 361, 363, 364, 236, 235, 239, 238, 227, 226, 230, 229, 209, 208, 212, 211, 200, 199, 203, 202, 155, 154, 158, 157, 146, 145, 149, 148, 128, 127, 131, 130, 119, 118, 122, 121, 479, 478, 482, 481, 470, 469, 473, 472, 452, 451, 455, 454, 443, 442, 446, 445, 398, 397, 401, 400, 389, 388, 392, 391, 371, 370, 374, 373, 362, 361, 365, 364, 240, 241, 237, 238, 231, 232, 228, 229, 213, 214, 210, 211, 204, 205, 201, 202, 159, 160, 156, 157, 150, 151, 147, 148, 132, 133, 129, 130, 123, 124, 120, 121, 483, 484, 480, 481, 474, 475, 471, 472, 456, 457, 453, 454, 447, 448, 444, 445, 402, 403, 399, 400, 393, 394, 390, 391, 375, 376, 372, 373, 366, 367, 363, 364, 242, 241, 239, 238, 233, 232, 230, 229, 215, 214, 212, 211, 206, 205, 203, 202, 161, 160, 158, 157, 152, 151, 149, 148, 134, 133, 131, 130, 125, 124, 122, 121, 485, 484, 482, 481, 476, 475, 473, 472, 458, 457, 455, 454, 449, 448, 446, 445, 404, 403, 401, 400, 395, 394, 392, 391, 377, 376, 374, 373, 368, 367, 365, 364, 486, 487, 489, 490, 495, 496, 498, 499, 513, 514, 516, 517, 522, 523, 525, 526, 567, 568, 570, 571, 576, 577, 579, 580, 594, 595, 597, 598, 603, 604, 606, 607, 243, 244, 246, 247, 252, 253, 255, 256, 270, 271, 273, 274, 279, 280, 282, 283, 324, 325, 327, 328, 333, 334, 336, 337, 351, 352, 354, 355, 360, 361, 363, 364, 488, 487, 491, 490, 497, 496, 500, 499, 515, 514, 518, 517, 524, 523, 527, 526, 569, 568, 572, 571, 578, 577, 581, 580, 596, 595, 599, 598, 605, 604, 608, 607, 245, 244, 248, 247, 254, 253, 257, 256, 272, 271, 275, 274, 281, 280, 284, 283, 326, 325, 329, 328, 335, 334, 338, 337, 353, 352, 356, 355, 362, 361, 365, 364, 492, 493, 489, 490, 501, 502, 498, 499, 519, 520, 516, 517, 528, 529, 525, 526, 573, 574, 570, 571, 582, 583, 579, 580, 600, 601, 597, 598, 609, 610, 606, 607, 249, 250, 246, 247, 258, 259, 255, 256, 276, 277, 273, 274, 285, 286, 282, 283, 330, 331, 327, 328, 339, 340, 336, 337, 357, 358, 354, 355, 366, 367, 363, 364, 494, 493, 491, 490, 503, 502, 500, 499, 521, 520, 518, 517, 530, 529, 527, 526, 575, 574, 572, 571, 584, 583, 581, 580, 602, 601, 599, 598, 611, 610, 608, 607, 251, 250, 248, 247, 260, 259, 257, 256, 278, 277, 275, 274, 287, 286, 284, 283, 332, 331, 329, 328, 341, 340, 338, 337, 359, 358, 356, 355, 368, 367, 365, 364, 504, 505, 507, 508, 495, 496, 498, 499, 531, 532, 534, 535, 522, 523, 525, 526, 585, 586, 588, 589, 576, 577, 579, 580, 612, 613, 615, 616, 603, 604, 606, 607, 261, 262, 264, 265, 252, 253, 255, 256, 288, 289, 291, 292, 279, 280, 282, 283, 342, 343, 345, 346, 333, 334, 336, 337, 369, 370, 372, 373, 360, 361, 363, 364, 506, 505, 509, 508, 497, 496, 500, 499, 533, 532, 536, 535, 524, 523, 527, 526, 587, 586, 590, 589, 578, 577, 581, 580, 614, 613, 617, 616, 605, 604, 608, 607, 263, 262, 266, 265, 254, 253, 257, 256, 290, 289, 293, 292, 281, 280, 284, 283, 344, 343, 347, 346, 335, 334, 338, 337, 371, 370, 374, 373, 362, 361, 365, 364, 510, 511, 507, 508, 501, 502, 498, 499, 537, 538, 534, 535, 528, 529, 525, 526, 591, 592, 588, 589, 582, 583, 579, 580, 618, 619, 615, 616, 609, 610, 606, 607, 267, 268, 264, 265, 258, 259, 255, 256, 294, 295, 291, 292, 285, 286, 282, 283, 348, 349, 345, 346, 339, 340, 336, 337, 375, 376, 372, 373, 366, 367, 363, 364, 512, 511, 509, 508, 503, 502, 500, 499, 539, 538, 536, 535, 530, 529, 527, 526, 593, 592, 590, 589, 584, 583, 581, 580, 620, 619, 617, 616, 611, 610, 608, 607, 269, 268, 266, 265, 260, 259, 257, 256, 296, 295, 293, 292, 287, 286, 284, 283, 350, 349, 347, 346, 341, 340, 338, 337, 377, 376, 374, 373, 368, 367, 365, 364, 540, 541, 543, 544, 549, 550, 552, 553, 513, 514, 516, 517, 522, 523, 525, 526, 621, 622, 624, 625, 630, 631, 633, 634, 594, 595, 597, 598, 603, 604, 606, 607, 297, 298, 300, 301, 306, 307, 309, 310, 270, 271, 273, 274, 279, 280, 282, 283, 378, 379, 381, 382, 387, 388, 390, 391, 351, 352, 354, 355, 360, 361, 363, 364, 542, 541, 545, 544, 551, 550, 554, 553, 515, 514, 518, 517, 524, 523, 527, 526, 623, 622, 626, 625, 632, 631, 635, 634, 596, 595, 599, 598, 605, 604, 608, 607, 299, 298, 302, 301, 308, 307, 311, 310, 272, 271, 275, 274, 281, 280, 284, 283, 380, 379, 383, 382, 389, 388, 392, 391, 353, 352, 356, 355, 362, 361, 365, 364, 546, 547, 543, 544, 555, 556, 552, 553, 519, 520, 516, 517, 528, 529, 525, 526, 627, 628, 624, 625, 636, 637, 633, 634, 600, 601, 597, 598, 609, 610, 606, 607, 303, 304, 300, 301, 312, 313, 309, 310, 276, 277, 273, 274, 285, 286, 282, 283, 384, 385, 381, 382, 393, 394, 390, 391, 357, 358, 354, 355, 366, 367, 363, 364, 548, 547, 545, 544, 557, 556, 554, 553, 521, 520, 518, 517, 530, 529, 527, 526, 629, 628, 626, 625, 638, 637, 635, 634, 602, 601, 599, 598, 611, 610, 608, 607, 305, 304, 302, 301, 314, 313, 311, 310, 278, 277, 275, 274, 287, 286, 284, 283, 386, 385, 383, 382, 395, 394, 392, 391, 359, 358, 356, 355, 368, 367, 365, 364, 558, 559, 561, 562, 549, 550, 552, 553, 531, 532, 534, 535, 522, 523, 525, 526, 639, 640, 642, 643, 630, 631, 633, 634, 612, 613, 615, 616, 603, 604, 606, 607, 315, 316, 318, 319, 306, 307, 309, 310, 288, 289, 291, 292, 279, 280, 282, 283, 396, 397, 399, 400, 387, 388, 390, 391, 369, 370, 372, 373, 360, 361, 363, 364, 560, 559, 563, 562, 551, 550, 554, 553, 533, 532, 536, 535, 524, 523, 527, 526, 641, 640, 644, 643, 632, 631, 635, 634, 614, 613, 617, 616, 605, 604, 608, 607, 317, 316, 320, 319, 308, 307, 311, 310, 290, 289, 293, 292, 281, 280, 284, 283, 398, 397, 401, 400, 389, 388, 392, 391, 371, 370, 374, 373, 362, 361, 365, 364, 564, 565, 561, 562, 555, 556, 552, 553, 537, 538, 534, 535, 528, 529, 525, 526, 645, 646, 642, 643, 636, 637, 633, 634, 618, 619, 615, 616, 609, 610, 606, 607, 321, 322, 318, 319, 312, 313, 309, 310, 294, 295, 291, 292, 285, 286, 282, 283, 402, 403, 399, 400, 393, 394, 390, 391, 375, 376, 372, 373, 366, 367, 363, 364, 566, 565, 563, 562, 557, 556, 554, 553, 539, 538, 536, 535, 530, 529, 527, 526, 647, 646, 644, 643, 638, 637, 635, 634, 620, 619, 617, 616, 611, 610, 608, 607, 323, 322, 320, 319, 314, 313, 311, 310, 296, 295, 293, 292, 287, 286, 284, 283, 404, 403, 401, 400, 395, 394, 392, 391, 377, 376, 374, 373, 368, 367, 365, 364, 648, 649, 651, 652, 657, 658, 660, 661, 675, 676, 678, 679, 684, 685, 687, 688, 567, 568, 570, 571, 576, 577, 579, 580, 594, 595, 597, 598, 603, 604, 606, 607, 405, 406, 408, 409, 414, 415, 417, 418, 432, 433, 435, 436, 441, 442, 444, 445, 324, 325, 327, 328, 333, 334, 336, 337, 351, 352, 354, 355, 360, 361, 363, 364, 650, 649, 653, 652, 659, 658, 662, 661, 677, 676, 680, 679, 686, 685, 689, 688, 569, 568, 572, 571, 578, 577, 581, 580, 596, 595, 599, 598, 605, 604, 608, 607, 407, 406, 410, 409, 416, 415, 419, 418, 434, 433, 437, 436, 443, 442, 446, 445, 326, 325, 329, 328, 335, 334, 338, 337, 353, 352, 356, 355, 362, 361, 365, 364, 654, 655, 651, 652, 663, 664, 660, 661, 681, 682, 678, 679, 690, 691, 687, 688, 573, 574, 570, 571, 582, 583, 579, 580, 600, 601, 597, 598, 609, 610, 606, 607, 411, 412, 408, 409, 420, 421, 417, 418, 438, 439, 435, 436, 447, 448, 444, 445, 330, 331, 327, 328, 339, 340, 336, 337, 357, 358, 354, 355, 366, 367, 363, 364, 656, 655, 653, 652, 665, 664, 662, 661, 683, 682, 680, 679, 692, 691, 689, 688, 575, 574, 572, 571, 584, 583, 581, 580, 602, 601, 599, 598, 611, 610, 608, 607, 413, 412, 410, 409, 422, 421, 419, 418, 440, 439, 437, 436, 449, 448, 446, 445, 332, 331, 329, 328, 341, 340, 338, 337, 359, 358, 356, 355, 368, 367, 365, 364, 666, 667, 669, 670, 657, 658, 660, 661, 693, 694, 696, 697, 684, 685, 687, 688, 585, 586, 588, 589, 576, 577, 579, 580, 612, 613, 615, 616, 603, 604, 606, 607, 423, 424, 426, 427, 414, 415, 417, 418, 450, 451, 453, 454, 441, 442, 444, 445, 342, 343, 345, 346, 333, 334, 336, 337, 369, 370, 372, 373, 360, 361, 363, 364, 668, 667, 671, 670, 659, 658, 662, 661, 695, 694, 698, 697, 686, 685, 689, 688, 587, 586, 590, 589, 578, 577, 581, 580, 614, 613, 617, 616, 605, 604, 608, 607, 425, 424, 428, 427, 416, 415, 419, 418, 452, 451, 455, 454, 443, 442, 446, 445, 344, 343, 347, 346, 335, 334, 338, 337, 371, 370, 374, 373, 362, 361, 365, 364, 672, 673, 669, 670, 663, 664, 660, 661, 699, 700, 696, 697, 690, 691, 687, 688, 591, 592, 588, 589, 582, 583, 579, 580, 618, 619, 615, 616, 609, 610, 606, 607, 429, 430, 426, 427, 420, 421, 417, 418, 456, 457, 453, 454, 447, 448, 444, 445, 348, 349, 345, 346, 339, 340, 336, 337, 375, 376, 372, 373, 366, 367, 363, 364, 674, 673, 671, 670, 665, 664, 662, 661, 701, 700, 698, 697, 692, 691, 689, 688, 593, 592, 590, 589, 584, 583, 581, 580, 620, 619, 617, 616, 611, 610, 608, 607, 431, 430, 428, 427, 422, 421, 419, 418, 458, 457, 455, 454, 449, 448, 446, 445, 350, 349, 347, 346, 341, 340, 338, 337, 377, 376, 374, 373, 368, 367, 365, 364, 702, 703, 705, 706, 711, 712, 714, 715, 675, 676, 678, 679, 684, 685, 687, 688, 621, 622, 624, 625, 630, 631, 633, 634, 594, 595, 597, 598, 603, 604, 606, 607, 459, 460, 462, 463, 468, 469, 471, 472, 432, 433, 435, 436, 441, 442, 444, 445, 378, 379, 381, 382, 387, 388, 390, 391, 351, 352, 354, 355, 360, 361, 363, 364, 704, 703, 707, 706, 713, 712, 716, 715, 677, 676, 680, 679, 686, 685, 689, 688, 623, 622, 626, 625, 632, 631, 635, 634, 596, 595, 599, 598, 605, 604, 608, 607, 461, 460, 464, 463, 470, 469, 473, 472, 434, 433, 437, 436, 443, 442, 446, 445, 380, 379, 383, 382, 389, 388, 392, 391, 353, 352, 356, 355, 362, 361, 365, 364, 708, 709, 705, 706, 717, 718, 714, 715, 681, 682, 678, 679, 690, 691, 687, 688, 627, 628, 624, 625, 636, 637, 633, 634, 600, 601, 597, 598, 609, 610, 606, 607, 465, 466, 462, 463, 474, 475, 471, 472, 438, 439, 435, 436, 447, 448, 444, 445, 384, 385, 381, 382, 393, 394, 390, 391, 357, 358, 354, 355, 366, 367, 363, 364, 710, 709, 707, 706, 719, 718, 716, 715, 683, 682, 680, 679, 692, 691, 689, 688, 629, 628, 626, 625, 638, 637, 635, 634, 602, 601, 599, 598, 611, 610, 608, 607, 467, 466, 464, 463, 476, 475, 473, 472, 440, 439, 437, 436, 449, 448, 446, 445, 386, 385, 383, 382, 395, 394, 392, 391, 359, 358, 356, 355, 368, 367, 365, 364, 720, 721, 723, 724, 711, 712, 714, 715, 693, 694, 696, 697, 684, 685, 687, 688, 639, 640, 642, 643, 630, 631, 633, 634, 612, 613, 615, 616, 603, 604, 606, 607, 477, 478, 480, 481, 468, 469, 471, 472, 450, 451, 453, 454, 441, 442, 444, 445, 396, 397, 399, 400, 387, 388, 390, 391, 369, 370, 372, 373, 360, 361, 363, 364, 722, 721, 725, 724, 713, 712, 716, 715, 695, 694, 698, 697, 686, 685, 689, 688, 641, 640, 644, 643, 632, 631, 635, 634, 614, 613, 617, 616, 605, 604, 608, 607, 479, 478, 482, 481, 470, 469, 473, 472, 452, 451, 455, 454, 443, 442, 446, 445, 398, 397, 401, 400, 389, 388, 392, 391, 371, 370, 374, 373, 362, 361, 365, 364, 726, 727, 723, 724, 717, 718, 714, 715, 699, 700, 696, 697, 690, 691, 687, 688, 645, 646, 642, 643, 636, 637, 633, 634, 618, 619, 615, 616, 609, 610, 606, 607, 483, 484, 480, 481, 474, 475, 471, 472, 456, 457, 453, 454, 447, 448, 444, 445, 402, 403, 399, 400, 393, 394, 390, 391, 375, 376, 372, 373, 366, 367, 363, 364, 728, 727, 725, 724, 719, 718, 716, 715, 701, 700, 698, 697, 692, 691, 689, 688, 647, 646, 644, 643, 638, 637, 635, 634, 620, 619, 617, 616, 611, 610, 608, 607, 485, 484, 482, 481, 476, 475, 473, 472, 458, 457, 455, 454, 449, 448, 446, 445, 404, 403, 401, 400, 395, 394, 392, 391, 377, 376, 374, 373, 368, 367, 365, 364};

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
    else if ((s->playing_area & NORTH_WALL) == NORTH_WALL) {
        size_t key = 0;
        for (int i = (HEIGTH - 1) * V_SHIFT; i >= 0; i -= V_SHIFT) {
            key *= 729;
            key += SIX_KEYS[((s->player >> i) & NORTH_WALL) | (((s->opponent >> i) & NORTH_WALL) << 6)];
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

    print_stones(0x30c30c3);
    print_stones(0x5145145);
    return 0;
}
#endif
