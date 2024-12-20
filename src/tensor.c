#include <time.h>
#include <string.h>
#include <assert.h>

#include "tensor.h"
#include "mat_ops.h"

struct tensor{
    Data * data;
    int shape[MAX_DIM];
    int ndims;
    int length;
    int offset;

    // not needed at the moment
    int strides[2];
};

typedef void (*point_wise_bin_op)(const double *, const double *, double *, unsigned int);
typedef void (*point_wise_ord_op)(double *, int);

static inline void _shape_check(int axis_size){
    if(axis_size < 1){
        PANIC("Axis length should at least be 1\n");
    }
}

static inline void check_size(int ndims, int shape[MAX_DIM]){
    TENSOR_CHECK(ndims < 1 || ndims > MAX_DIM, "Dimensions should be at least 1 and not greater than %d\n", MAX_DIM)

    for(int i = 0; i < ndims; i++){
        _shape_check(shape[i]);
    }
}

static inline int calculate_length(int ndims, int shape[MAX_DIM]){
    int length = 1;
    for(int i = 0; i < ndims; i++){
        length *= shape[i];
    }

    return length;
}

static inline void tensor_set_size(tensor * self, int ndims, int * shape, int length){
    self->ndims = ndims;
    for(int i = 0; i < ndims; i++){
        self->shape[i] = shape[i];
    }

    self->length = calculate_length(ndims, shape);
    assert(self->length == length);
}

/*
Initialize a tensor without creating memory for its data
*/
static inline tensor * tensor_shallow_init(int ndims, int shape[MAX_DIM]){
    check_size(ndims, shape);

    tensor * t = (tensor *)SAFE_MALLOC(sizeof(tensor));
    t->ndims = ndims;

    for(int i = 0; i < ndims; i++){
        t->shape[i] = shape[i];
    }

    t->length = calculate_length(ndims, shape);
    t->data = NULL;
    t->offset = 0;

    // will be needed later
    t->strides[0] = 1;
    t->strides[1] = 0;

    return t;
}

static inline void tensor_set_at_(tensor * self, double value, int pos){
    assert(pos < self->length);
    data_insert(self->data, value, pos + self->offset);
}

static inline double tensor_get_at_(tensor * self, int pos){
    assert(pos < self->length);
    return data_get(self->data, pos + self->offset);
}

static inline void tensor_copy_data_(tensor * dest, tensor * src, int length, int offset){
    for(int i = 0; i < length; i++){
        tensor_set_at_(dest, tensor_get_at_(src, i), i + offset);
    }
}

static inline void tensor_copy_from_data(tensor * self, Data * data, int shape[], int ndims, int length, int offset){
    TENSOR_CHECK(offset < 0, "Invalid tensor offset");

    if(self->data != data){
        if(self->data != NULL){
            data_dec(self->data);
        }

        self->data = data;
        data_inc(self->data);
    }

    tensor_set_size(self, ndims, shape, length);
    self->offset = offset;
}

static inline void tensor_unary_point_wise_op(tensor * self, tensor * in, point_wise_ord_op op){
    TENSOR_EXIST(self);
    TENSOR_EXIST(in);

    TENSOR_CHECK(self->length != in->length, "Tensor size mismatch %d != %d", self->length, in->length);
    tensor_clone(self, in);
    op(tensor_data(self), self->length);
}

tensor * tensor_init(int ndims, int shape[MAX_DIM]){
    tensor * t = tensor_shallow_init(ndims, shape);
    t->data = data_init(t->length);

    return t;
}

tensor * tensor_init_with_(int ndims, int shape[MAX_DIM], double value){
    tensor * t = tensor_init(ndims, shape);

    for(int i = 0; i < t->length; i++){
        tensor_set_at_(t, value, i);                
    }

    return t;
}

tensor * _tensor_zeros(int ndims, int shape[MAX_DIM]){
    return tensor_init_with_(ndims, shape, 0);
}

tensor * _tensor_ones(int ndims, int shape[MAX_DIM]){
    return tensor_init_with_(ndims, shape, 1);
}

/*
Generates a random number between -1 and 1
 */
tensor * tensor_rand(int ndims, int shape[MAX_DIM]){
    tensor * t = tensor_init(ndims, shape);

    for(int i = 0; i < t->length; i++){
        tensor_set_at_(t, randn(), i);               
    }

    return t;
}

tensor * tensor_concat(tensor * self, tensor * t1, tensor * t2){
    TENSOR_CHECK(t1->shape[1] != t2->shape[1], "Tensor size mismatch")
    TENSOR_CHECK(self == NULL, "Tensor undefined");
    TENSOR_CHECK(self->shape[0] != (t1->shape[0] + t2->shape[0]), "Tensor mismatch in dim 0, %d != %d + %d", self->shape[0], t1->shape[0], t2->shape[0]);
    TENSOR_CHECK(self->shape[1] != t1->shape[1], "Mismatch in dim 1, %d != %d", self->shape[1], t1->shape[1]);

    tensor_copy_data_(self, t1, t1->length, 0);
    tensor_copy_data_(self, t2, t2->length, t1->length);


    return self;
}

double * tensor_data(tensor * self){
    return data_raw_ptr(self->data) + self->offset;
}

tensor * tensor_binary_point_wise_op(tensor * self, tensor * t1, tensor * t2, point_wise_bin_op op){
    if((t1->shape[0] != t2->shape[0]) || (t1->shape[1] != t2->shape[1])){
        PANIC("Tensor size mismatch");
    }

    op(tensor_data(t1), tensor_data(t2), tensor_data(self), t1->length);

    return self; 
}

tensor * tensor_plus(tensor * self, tensor * t1, tensor * t2){
    TENSOR_EXIST(self);

    return tensor_binary_point_wise_op(self, t1, t2, matrix_addition);
}

tensor * tensor_mul(tensor * self, tensor * t1, tensor * t2){
    TENSOR_EXIST(self);

    return tensor_binary_point_wise_op(self, t1, t2, hadamard_product);
}

int * tensor_shape(tensor * self){
    return self->shape;
}

tensor * tensor_select(tensor * self, tensor * src, int index){
    TENSOR_CHECK(index >= src->shape[0], "Index %d out of bounds, shape size %d", index, src->shape[0]);

    if(src == NULL){
        src = self;
    }

    tensor_clone(self, src);

    self->offset += index * src->shape[1];
    self->shape[0] = src->shape[1];
    self->shape[1] = 1;
    self->length = calculate_length(self->ndims, self->shape);

    return self;
}


void tensor_printf(tensor * self){
    printf("Tensor(");
    printf("[");
    for(int i = 0; i < self->shape[0]; i++){
        printf("[");
        for(int j = 0; j < self->shape[1]; j++){
            int index = j + (i * self->shape[1]);
            printf("%g", tensor_get_at_(self, index));

            if(j < self->shape[1] - 1){
                printf(",");
            }
        }
        printf("]");

        if(i < self->shape[0] - 1){
            printf(",");
        }
    }
    printf("]");
    printf(")\n");
}



tensor * tensor_mat_mul(tensor * self, tensor * t1, tensor * t2){
    TENSOR_EXIST(self);

    TENSOR_CHECK(t1->shape[1] != t2->shape[0],
        "Mismatch tensor sizes [%d, %d] x [%d, %d]\n", t1->shape[0], t1->shape[1], t2->shape[0], t2->shape[1]
    );

    TENSOR_CHECK(self->shape[0] != t1->shape[0] && self->shape[1] != t2->shape[1], 
        "Mismatch tensor sizes: Expected [%d, %d], Got [%d, %d]\n", t1->shape[0], t2->shape[1], self->shape[0], self->shape[1]
    );

    if(self->length == 1){
        double output = vector_dot_product(tensor_data(t1), tensor_data(t2), t1->length);
        tensor_set_at_(self, output, 0);
    }else{
        //perform multiplication and write results to the stored pointer for the result tensor
        matrix_multiplication(
                tensor_data(t1), 
                tensor_data(t2), 
                tensor_data(self),
                t1->shape[0], 
                t1->shape[1], 
                t2->shape[1]
        );
    }


    return self;
}

void tensor_clone(tensor * self, tensor * src){
    if(self != src){
        tensor_copy_from_data(self, src->data, src->shape, src->ndims, src->length, src->offset);
    }
}

void tensor_sigmoid(tensor * self, tensor * in){
    tensor_unary_point_wise_op(self, in, vector_sigmoid);
}

void tensor_tanh(tensor * self, tensor * in){
    tensor_unary_point_wise_op(self, in, vector_tanh);
}

void tensor_cleanup(tensor * self){
    if(self == NULL){
        return;
    }

    if(self->data != NULL){
        data_dec(self->data);
    }
    
    SAFE_FREE(self);
}