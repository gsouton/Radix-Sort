#include <stdio.h>

#include "cpu_functions.cu.h"
#include "test_utils.cu.h"

// from src
#include "kernel_env.cu.h"
#include "kernels.cu.h"
#include "radix-sort.cu.h"
#include "types.cu.h"
#include "utils.cu.h"
#include <limits.h>

bool test_compute_histogram_local(kernel_env env) {
    debug("*** Testing compute histogram local kernel!");
    for (uint32_t it = 0; it < size_in_bits<uint32_t>(); it += env->bits) {
        uint32_t iteration = it / env->bits;
        // compute histogram on CPU
        compute_histogram(env->h_keys, env->h_hist, env->bits, env->h_keys_size,
                          iteration);

        // compute histogram on GPU
        compute_histogram_local(env, iteration);
        uint32_t hist_size = d_hist_size(env), histogram[hist_size];
        reduce_d_hist(env, histogram);

        if (!equal(env->h_hist, histogram, env->number_classes)) {
            if (DEBUG) {
                fprintf(stderr, "Failure at iteration: %d\n", iteration);
                fprintf(stderr, "\n-- Expected histogram (CPU):\n");
                log_vector(env->h_hist, env->h_hist_size);
                fprintf(stderr, "\n-- Result histogram (GPU):\n");
                log_reduce_d_hist(env);
                fprintf(stderr, "\n-- Unreduced histogram (GPU):\n");
                log_d_hist(env);
            }
            return false;
        }
    }
    // log_d_hist(env);
    return true;
}

bool test_transpose(kernel_env env) {
    fprintf(stderr, "*** Testing histogram tranposition kernel!\n");
    transpose_histogram(env);
    log_d_hist_transpose(env);
    return true;
}

bool test_scan(kernel_env env) {
    fprintf(stderr, "*** Testing scan on transposed histogram!\n");
    scan_transposed_histogram(env);
    log_d_hist_transpose(env);
    return false;
}


bool test_radix_sort(kernel_env env) {
    radix_sort(env);
    uint32_t res[env->d_keys_size];
    log_output_result(env);
    d_output(env, res);
    for (int i = 0; i < env->d_keys_size - 1; ++i) {
        if (res[i] > res[i + 1]) {
            printf("res[%i]: %d, res[%i]: %d\n", i, res[i], i+1, res[i + 1]);
            return false;
        }
    }
    return true;
}

int main(int argc, char **argv) {
    bool success = true;
    const int number_keys = parse_args(argc, argv);

    const uint32_t block_size = 256, elem_pthread = 4, bits = 4, max_value = INT_MAX;

    kernel_env env =
        new_kernel_env(block_size, elem_pthread, bits, number_keys, max_value);

    success |= test(test_radix_sort, env);
    cudaDeviceSynchronize();
    cudaCheckError();

    free_env(env);

    return success ? 0 : 1;
}
