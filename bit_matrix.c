#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define FIRST_ROW (0xFFULL)
#define FIRST_COLUMN (0x101010101010101ULL)

typedef unsigned long long int matrix_t;

typedef struct bit_matrix {
    matrix_t *submatrices;
    size_t h_count;
    size_t v_count;
    size_t submatrix_count;
} bit_matrix;

int matrix_popcount(matrix_t m) {
    return __builtin_popcountll(m);
}

bit_matrix* bit_matrix_new(size_t width, size_t height) {
    size_t h_count, v_count;
    if (width & 7) {
        h_count = (width >> 3) + 1;
    }
    else {
        h_count = width >> 3;
    }
    if (height & 7) {
        v_count = (height >> 3) + 1;
    }
    else {
        v_count = height >> 3;
    }
    size_t submatrix_count = h_count * v_count;
    bit_matrix *b = (bit_matrix*) malloc(sizeof(bit_matrix));
    b->submatrices = (matrix_t*) calloc(submatrix_count, sizeof(matrix_t));
    b->h_count = h_count;
    b->v_count = v_count;
    b->submatrix_count = submatrix_count;
    return b;
}

void bit_matrix_free(bit_matrix *b) {
    free(b->submatrices);
    free(b);
}

void bit_matrix_set(bit_matrix *b, size_t i, size_t j) {
    size_t index_i, index_j;
    index_i = i >> 3;
    index_j = j >> 3;
    i &= 7;
    j &= 7;

    b->submatrices[index_i + index_j * b->h_count] |= 1ULL << (i | (j << 3));
}

matrix_t bit_matrix_get(bit_matrix *b, size_t i, size_t j) {
    size_t index_i, index_j;
    index_i = i >> 3;
    index_j = j >> 3;
    i &= 7;
    j &= 7;

    return b->submatrices[index_i + index_j * b->h_count] & (1ULL << (i | (j << 3)));
}

int bit_matrix_row_nonzero(bit_matrix *b, size_t j) {
    size_t index_j_times_h_count = (j >> 3) * b->h_count;
    j &= 7;
    matrix_t mask = FIRST_ROW << (j << 3);
    for (size_t index_i = 0; index_i < b->h_count; index_i++){
        if (b->submatrices[index_i + index_j_times_h_count] & mask){
            return 1;
        }
    }
    return 0;
}

int bit_matrix_column_nonzero(bit_matrix *b, size_t i) {
    size_t index_i = i >> 3;
    i &= 7;
    matrix_t mask = FIRST_COLUMN << i;
    for (
        size_t index_j_times_h_count = 0;
        index_j_times_h_count < b->submatrix_count;
        index_j_times_h_count += b->h_count
    ){
        if (b->submatrices[index_i + index_j_times_h_count] & mask){
            return 1;
        }
    }
    return 0;
}

int bit_matrix_row_popcount(bit_matrix *b, size_t j) {
    size_t index_j_times_h_count = (j >> 3) * b->h_count;
    int sum = 0;
    j &= 7;
    matrix_t mask = FIRST_ROW << (j << 3);
    for (size_t index_i = 0; index_i < b->h_count; index_i++){
        sum += matrix_popcount(b->submatrices[index_i + index_j_times_h_count] & mask);
    }
    return sum;
}

void bit_matrix_clear_row(bit_matrix *b, size_t j) {
    size_t index_j_times_h_count = (j >> 3) * b->h_count;
    j &= 7;
    matrix_t mask = ~(FIRST_ROW << (j << 3));
    for (size_t index_i = 0; index_i < b->h_count; index_i++) {
        b->submatrices[index_i + index_j_times_h_count] &= mask;
    }
}

void bit_matrix_clear_columns_by_row(bit_matrix *b, size_t j) {
    size_t index_j_times_h_count = (j >> 3) * b->h_count;
    j &= 7;
    j <<= 3;
    for (size_t index_i = 0; index_i < b->h_count; index_i++){
        matrix_t clearing_mask = (b->submatrices[index_i + index_j_times_h_count] >> j) & FIRST_ROW;
        clearing_mask |= clearing_mask << 8;
        clearing_mask |= clearing_mask << 16;
        clearing_mask |= clearing_mask << 32;
        for (size_t index_j2_times_h_count = 0; index_j2_times_h_count < b->submatrix_count; index_j2_times_h_count += b->h_count) {
            b->submatrices[index_i + index_j2_times_h_count] &= ~clearing_mask;
        }
    }
}

void print_bit_matrix(bit_matrix *b) {
    for (size_t j = 0; j < 8 * b->v_count; j++) {
        for (size_t i = 0; i < 8 * b->h_count; i++) {
            printf("%d ", !!bit_matrix_get(b, i, j));
        }
        printf("\n");
    }
}

void bit_matrix_test() {
    bit_matrix *bm = bit_matrix_new(17, 17);
    assert(bm->h_count == 3);
    assert(bm->v_count == 3);

    bit_matrix_free(bm);


    bm = bit_matrix_new(16, 16);
    assert(bm->h_count == 2);
    assert(bm->v_count == 2);

    bit_matrix_set(bm, 13, 15);
    assert(bit_matrix_get(bm, 13, 15));

    bit_matrix_free(bm);
    /*
    assert(bm.row_nonzero(15));
    assert(bm.column_nonzero(13));

    bm.set(13, 5);
    assert(bm.row_popcount(5) == 1);
    assert(bm.column_popcount(13) == 2);

    bm.set(2, 15);
    bm.set(9, 5);
    bm.set(7, 7);
    bm.clear_rows_by_column(13);
    assert(!bm.get(2, 15));
    assert(!bm.get(9, 5));
    assert(!bm.get(13, 15));
    assert(!bm.get(13, 5));
    assert(bm.get(7, 7));
    */
}
