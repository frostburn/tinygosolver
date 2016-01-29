#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef unsigned long long slot_t;

// TODO: Check that we're on a 64 bit machine.

typedef struct dict
{
    size_t num_slots;
    size_t max_key;
    slot_t *slots;
} dict;


typedef struct lin_dict
{
    size_t num_keys;
    size_t size;
    size_t *keys;
} lin_dict;

int popcountll(slot_t slot) {
    return __builtin_popcountll(slot);
}

void init_dict(dict *d, size_t max_key) {
    size_t num_slots = max_key >> 6;
    num_slots += 1;  // FIXME
    d->slots = (slot_t*) calloc(num_slots, sizeof(slot_t));
    d->num_slots = num_slots;
    d->max_key = max_key;
}

void resize_dict(dict *d, size_t max_key) {
    size_t num_slots = max_key >> 6;
    num_slots += 1; // FIXME
    d->slots = (slot_t*) realloc(d->slots, sizeof(slot_t) * num_slots);
    if (num_slots > d->num_slots) {
        memset(d->slots + d->num_slots, 0, (num_slots - d->num_slots) * sizeof(slot_t));
    }
    d->num_slots = num_slots;
    d->max_key = max_key;
}

void add_key(dict *d, size_t key) {
    slot_t bit = key & 0xFFFFULL;
    key >>= 6;
    d->slots[key] |= 1ULL << bit;
}

slot_t test_key(dict *d, size_t key) {
    slot_t bit = key & 0xFFFFULL;
    key >>= 6;
    return !!(d->slots[key] & (1ULL << bit));
}

size_t next_key(dict *d, size_t last) {
    while(1) {
        if (test_key(d, ++last)) {
            return last;
        }
    }
}

size_t key_index(dict *d, size_t key) {
    size_t index = 0;
    size_t slot_index = 0;
    while (key >= 64) {
        index += popcountll(d->slots[slot_index++]);
        key -= 64;
    }
    while (key) {
        index += !!(d->slots[slot_index] & (1ULL << --key));
    }
    return index;
}

size_t num_keys(dict *d) {
    size_t num = 0;
    size_t key = 0;
    while (key < d->max_key){
        if (test_key(d, key)){
            num++;
        }
        key++;
    }
    return num;
}

int test_lin_key(lin_dict *ld, size_t key) {
    for (size_t i = 0; i < ld->num_keys; i++) {
        if (ld->keys[i] == key) {
            return 1;
        }
    }
    return 0;
}

void add_lin_key(lin_dict *ld, size_t key) {
    if (test_lin_key(ld, key)){
        return;
    }
    if (!ld->size){
        ld->size = 8;
        ld->keys = (size_t*) malloc(sizeof(size_t) * ld->size);
    }
    else if (ld->num_keys >= ld->size) {
        ld->size <<= 1;
        ld->keys = (size_t*) realloc(ld->keys, sizeof(size_t) * ld->size);
    }
    ld->keys[ld->num_keys++] = key;
}

size_t lin_key_index(lin_dict *ld, size_t key) {
    for (size_t i = 0; i < ld->num_keys; i++) {
        if (key == ld->keys[i]) {
            return i;
        }
    }
    assert(0);
}


/*
int main() {
    dict d_;
    dict *d = &d_;
    init_dict(d, 20);
    add_key(d, 6);
    add_key(d, 11);
    resize_dict(d, 200);
    add_key(d, 198);
    printf("%lld\n", test_key(d, 6));
    printf("%zu\n", next_key(d, 6));
    printf("%zu\n", key_index(d, 11));
    printf("%zu\n", key_index(d, 198));
    printf("%zu\n", num_keys(d));

    lin_dict ld_ = {0, 0, NULL};
    lin_dict *ld = &ld_;
    add_lin_key(ld, 666);
    add_lin_key(ld, 777);
    printf("%zu\n", lin_key_index(ld, 777));
    return 0;
}
*/
