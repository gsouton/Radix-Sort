#ifndef RADIX_TEST
#define RADIX_TEST
#include "radix-sort-cpu.cu.h"


void test_compute_histogram_cpu(uint32_t in_size, uint32_t max_value, 
                            uint32_t bits){

    // 2**bits (give the number of classes)
    const uint32_t number_classes = 1 << bits; 

    //Allocate and Initialize Host (CPU) data with random values
    uint32_t* h_keys  = (uint32_t*) malloc(in_size * sizeof(uint32_t));
    uint32_t* hist  = (uint32_t*) malloc(number_classes * sizeof(uint32_t));
    randomInitNat(h_keys, in_size, max_value);

    compute_histogram(h_keys, hist, bits, in_size, 0);
    log_vec("input array", h_keys, in_size);
    log_vec("hist", hist, number_classes);
    free(h_keys);
    free(hist);
}

bool assert_array_equals(uint32_t* array1, uint32_t *array2, uint32_t size){
    for(uint32_t i = 0; i < size; ++i){
        if(array1[i] != array2[i]){
            return false;
        }
    }
    return true;
}

/**
 * Test the first kernel, that compute the histogram.
 * @input_array_size size of array to sort
 * @bits number of bits to consider for radix sort
 * @max_value maximum value contain in the input_array
 * @elem_pthread number of keys process by one thread at the time
 * @num_thread number of threads running on GPU
 * @return true on success else false
 */
bool test_kernel1(const uint32_t in_size, 
             const uint32_t bits, 
             const uint32_t max_value, 
             const uint32_t elem_pthread,
             const uint32_t num_thread) {

    // 2**bits (give the number of classes)
    const uint32_t number_classes = 1 << bits; 

    // --- CPU Execution
    //Allocate and Initialize Host (CPU) data with random values
    uint32_t* h_keys  = (uint32_t*) malloc(in_size * sizeof(uint32_t));
    uint32_t* h_histogram  = (uint32_t*) malloc(number_classes * sizeof(uint32_t));
    randomInitNat(h_keys, in_size, max_value);
    compute_histogram(h_keys, h_histogram, bits, in_size, 0);


    // --- GPU Execution
    //Allocate and Initialize Device data
    // TODO: Import CUB library to be able to use cudaSucceded?
    uint32_t* d_keys_in;
    uint32_t* d_hist;
    uint32_t* d_histogram = (uint32_t*) malloc(number_classes * sizeof(uint32_t));

    cudaMalloc((void**) &d_keys_in,  in_size * sizeof(uint32_t));
    cudaMemcpy(d_keys_in, h_keys, in_size * sizeof(uint32_t), cudaMemcpyHostToDevice);
    cudaMalloc((void**) &d_hist, number_classes * sizeof(uint32_t));

    //execute first kernel
    uint32_t num_blocks = 
        (in_size + num_thread * elem_pthread - 1) / (num_thread * elem_pthread); 
    size_t hist_size = number_classes * sizeof(uint32_t);
    compute_histogram<<<num_thread, num_blocks, hist_size>>>(d_keys_in, 
                                                      d_hist, 
                                                      bits, 
                                                      elem_pthread, 
                                                      in_size, number_classes,0);

    cudaMemcpy(d_histogram, d_hist, number_classes*sizeof(uint32_t), cudaMemcpyDeviceToHost);
    cudaDeviceSynchronize();
    //cudaCheckError();
    log_vec("host input array", h_keys, in_size);
    log_vec("host histogram", h_histogram, number_classes);
    log_vec("device histogram", d_histogram, number_classes);
    return assert_array_equals(h_histogram, d_histogram, number_classes);
}



#endif // !RADIX_TEST
