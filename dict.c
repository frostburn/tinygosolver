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
    size_t *checkpoints;
} dict;


typedef struct lin_dict
{
    size_t num_keys;
    size_t size;
    size_t sorted_size;
    size_t *keys;
} lin_dict;

typedef struct vertex
{
    void *left;
    void *right;
} vertex;

int popcountll(slot_t slot) {
    return __builtin_popcountll(slot);
}

void init_dict(dict *d, size_t max_key) {
    size_t num_slots = max_key / 64;
    num_slots += 1;  // FIXME
    d->slots = (slot_t*) calloc(num_slots, sizeof(slot_t));
    d->num_slots = num_slots;
    d->max_key = max_key;
}

void resize_dict(dict *d, size_t max_key) {
    size_t num_slots = max_key / 64;
    num_slots += 1; // FIXME
    d->slots = (slot_t*) realloc(d->slots, sizeof(slot_t) * num_slots);
    if (num_slots > d->num_slots) {
        memset(d->slots + d->num_slots, 0, (num_slots - d->num_slots) * sizeof(slot_t));
    }
    d->num_slots = num_slots;
    d->max_key = max_key;
}

void finalize_dict(dict *d) {
    d->checkpoints = malloc(((d->num_slots / 16) + 1) * sizeof(size_t));  // FIXME
    size_t checkpoint = 0;
    for (size_t i = 0; i < d->num_slots; i++) {
        if (i % 16 == 0) {
            d->checkpoints[i / 16] = checkpoint;
        }
        checkpoint += popcountll(d->slots[i]);
    }
}

void add_key(dict *d, size_t key) {
    slot_t bit = key % 64;
    key /= 64;
    d->slots[key] |= 1ULL << bit;
}

slot_t test_key(dict *d, size_t key) {
    slot_t bit = key % 64;
    key /= 64;
    return !!(d->slots[key] & (1ULL << bit));
}

size_t next_key(dict *d, size_t last) {
    while(last < d->max_key) {
        if (test_key(d, ++last)) {
            return last;
        }
    }
    return last;
}

size_t key_index_alt(dict *d, size_t key) {
    size_t index = 0;
    for (size_t i = 0; i < key; i++) {
        index += test_key(d, i);
    }
    return index;
}

size_t key_index(dict *d, size_t key) {
    size_t index = 0;
    size_t slot_index = 0;
    index += d->checkpoints[key / 1024];
    slot_index = (key / 1024) * 16;
    key %= 1024;
    while (key >= 64) {
        index += popcountll(d->slots[slot_index++]);
        key -= 64;
    }
    index += popcountll(((1ULL << key) - 1) & d->slots[slot_index]);
    return index;
}

size_t num_keys(dict *d) {
    size_t num = 0;
    for (size_t i = 0; i < d->num_slots; i++) {
        num += popcountll(d->slots[i]);
    }
    return num;
}

int _compar(const void *a_, const void *b_) {
    size_t a = *((size_t*) a_);
    size_t b = *((size_t*) b_);
    if (a < b) {
        return -1;
    }
    else if (a > b) {
        return 1;
    }
    return 0;
}

int test_lin_key(lin_dict *ld, size_t key) {
    if (bsearch(&key, (void*) ld->keys, ld->sorted_size, sizeof(size_t), _compar) != NULL) {
        return 1;
    }
    for (size_t i = ld->sorted_size; i < ld->num_keys; i++) {
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
        ld->sorted_size = 1;
        ld->keys = (size_t*) malloc(sizeof(size_t) * ld->size);
    }
    else if (ld->num_keys >= ld->size) {
        ld->sorted_size = ld->size;
        qsort((void*) ld->keys, ld->sorted_size, sizeof(size_t), _compar);
        ld->size = ld->size * 3;
        ld->size >>= 1;
        ld->keys = (size_t*) realloc(ld->keys, sizeof(size_t) * ld->size);
    }
    ld->keys[ld->num_keys++] = key;
}

size_t lin_key_index(lin_dict *ld, size_t key) {
    void *result = bsearch(&key, (void*) ld->keys, ld->num_keys, sizeof(size_t), _compar);
    return ((size_t*) result) - ld->keys;
}

void finalize_lin_dict(lin_dict *ld) {
    ld->keys = (size_t*) realloc(ld->keys, sizeof(size_t) * ld->num_keys);
    qsort((void*) ld->keys, ld->num_keys, sizeof(size_t), _compar);
}

void* btree_get(vertex *root, int depth, size_t key) {
    for (int i = depth - 1; i >= 0; i--) {
        if (root == NULL) {
            return NULL;
        }
        if ((key >> i) & 1ULL) {
            root = (vertex*) (root->right);
        }
        else {
            root = (vertex*) (root->left);
        }
    }
    return root;
}

void* btree_set(vertex *root, int depth, size_t key, void *value, int protected) {
    for (int i = depth - 1; i > 0; i--) {
        if ((key >> i) & 1ULL) {
            if (root->right == NULL) {
                root->right = calloc(1, sizeof(vertex));
                assert(root->right);
            }
            root = (vertex*) (root->right);
        }
        else {
            if (root->left == NULL) {
                root->left = calloc(1, sizeof(vertex));
                assert(root->left);
            }
            root = (vertex*) (root->left);
        }
    }
    void *old_value;
    if (key & 1ULL) {
        old_value = root->right;
        if (!protected || root->right == NULL) {
            root->right = value;
        }
    }
    else {
        old_value = root->left;
        if (!protected || root->left == NULL) {
            root->left = value;
        }
    }
    return old_value;
}

void btree_traverse(vertex *root, int depth, size_t key, void visit(size_t key, void *value)) {
    if (root == NULL) {
        return;
    }
    if (depth == 0) {
        visit(key, root);
        return;
    }
    btree_traverse(root->left, depth - 1, 2 * key, visit);
    btree_traverse(root->right, depth - 1, 2 * key + 1, visit);
}

size_t btree_num_keys(vertex *root, int depth) {
    size_t keys = 0;
    void visit(size_t key, void *value) {
        keys++;
    }
    btree_traverse(root, depth, 0, visit);
    return keys;
}

#ifndef MAIN
int main() {
    dict d_;
    dict *d = &d_;
    init_dict(d, 20);
    add_key(d, 6);
    add_key(d, 11);
    resize_dict(d, 2000);
    add_key(d, 198);
    add_key(d, 1777);
    finalize_dict(d);
    printf("%zu, 3\n", d->checkpoints[1]);
    printf("%lld, 1\n", test_key(d, 6));
    printf("%zu, 11\n", next_key(d, 6));
    printf("%zu, %zu, 1\n", key_index(d, 11), key_index_alt(d, 11));
    printf("%zu, %zu, 2\n", key_index(d, 198), key_index_alt(d, 198));
    printf("%zu, %zu, 3\n", key_index(d, 1777), key_index_alt(d, 1777));
    printf("%zu, 4\n", num_keys(d));

    lin_dict ld_ = {0, 0, 0, NULL};
    lin_dict *ld = &ld_;
    add_lin_key(ld, 666);
    add_lin_key(ld, 777);
    printf("%zu, 1\n", lin_key_index(ld, 777));

    vertex v_ = {NULL, NULL};
    vertex *v = &v_;
    int depth = 3;

    double val0 = 3.14;
    double val3 = 2.6;
    double true_val3 = 2.7;
    double val6 = 1.4;
    double not_val6 = 6.9;
    double result;

    assert(btree_set(v, depth, 0, (void*) &val0, 0) == NULL);
    assert(btree_set(v, depth, 3, (void*) &val3, 0) == NULL);
    assert(btree_set(v, depth, 3, (void*) &true_val3, 0) != NULL);
    assert(btree_set(v, depth, 6, (void*) &val6, 1) == NULL);
    assert(btree_set(v, depth, 6, (void*) &not_val6, 1) != NULL);
    result = *((double*) btree_get(v, depth, 0));
    printf("%g\n", result);
    result = *((double*) btree_get(v, depth, 3));
    printf("%g\n", result);
    printf("%p\n", btree_get(v, depth, 1));
    printf("%p\n", btree_get(v, depth, 2));

    char name[] = "Simon";
    void say(size_t key, void *value) {
        double dvalue = *((double *) value);
        printf("%s says that %zu holds the value of %g\n", name, key, dvalue);
    }
    btree_traverse(v, depth, 0, say);

    printf("%zu\n", btree_num_keys(v, depth));

    return 0;
}
#endif
