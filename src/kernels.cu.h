#ifndef RADIX_KERNEL
#define RADIX_KERNEL
#include "types.cu.h"
#include "cub/cub.cuh"


/**
 * This kernel loads and sorts it's b-bit of it's tile on on-chip memory and 
 * writes the histogram and sorted tile to global memory
 *
 * @keys: Array to sort
 * @g_hist: Output array (representing the histogram)
 * @bits: number of bits too look at
 * @elem_pthread: number of keys process by one thread at the time
 * @in_size: Size of @keys
 * @hist_size: Size of @g_hist
 * @it: iteration number
*/
__global__ void compute_histogram(uint32_t* keys, uint32_t* g_hist, 
                                      uint32_t bits, uint32_t elem_pthread, 
                                      uint32_t in_size,uint32_t hist_size, 
                                      uint32_t it) {
    // TODO: 
    // - Sort the block locally
    // - See if we can gain performance by avoiding using atomicAdd
    // - Eventually avoid rewrite if conditions...? 
    uint32_t globalId = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t threadId = threadIdx.x;


    // Assuming histogram is filled with 0
    extern __shared__ uint32_t histogram[];
    if(globalId < hist_size){
        histogram[globalId] = 0;
    }

    for (uint32_t i = 0; i < elem_pthread; ++i) { // handle elements per thread numbner of elements 
        uint32_t next = globalId * elem_pthread + i; // index of element to be handled
        if (next < in_size) { // check that we're in bounds
            // get b bits corresponding to the current iteration
            uint32_t rank = (keys[next] >> (it * bits)) & (hist_size - 1); 
            atomicAdd(&histogram[rank], 1); // atomically increase the value of the bucket at index rank
        }
    }

    // copy from shared memory to global memory
    __syncthreads();
    if (threadId < hist_size) {
       int offset = blockIdx.x * hist_size + threadId; // calculate position in global memory for histogram value
       g_hist[offset] = histogram[threadId]; // copy histogram value to global memory
    }
}

__global__ void exampleKernel(int* out) {
    //printf("Executing Example Kernel\n");
    // Specialize BlockScan for a 1D block of 128 threads of type int
    typedef cub::BlockScan<int, 256> BlockScan;
    // Allocate shared memory for BlockScan
    __shared__ typename BlockScan::TempStorage temp_storage;
    // Obtain a segment of consecutive items that are blocked across threads
    int thread_data[4];
    for(int i = 0; i < 4; i++){
        
        thread_data[i] = threadIdx.x;
        printf("---threadId %d\n", threadIdx.x);
    }
    // Collectively compute the block-wide exclusive prefix sum
    BlockScan(temp_storage).ExclusiveSum(thread_data, thread_data);
    for(int i = 0; i < 4; i++){
        printf("***thread_data[%d] = %d\n", i, thread_data[i]);
        out[i+threadIdx.x] = thread_data[i];
    }
    
}
__global__ void compute_histogram_local_sort(uint32_t* keys, uint32_t* g_hist, 
                                      uint32_t bits, uint32_t elem_pthread, 
                                      uint32_t in_size,uint32_t hist_size, 
                                      uint32_t it) {
    // TODO: 
    // - Sort the block locally
    // - See if we can gain performance by avoiding using atomicAdd
    // - Eventually avoid rewrite if conditions...? 
    uint32_t globalId = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t threadId = threadIdx.x;
    uint32_t width = blockDim.x*blockDim.x;

    // Specialize BlockScan for a 1D block of 128 threads of type int
    //typedef cub::BlockScan<int, blockDim.x> BlockScan;

    extern __shared__ uint32_t histogram[];
    //filling histogram with zeroes
    if(threadId < hist_size){
        //TODO 
    }

    for (uint32_t i = 0; i < elem_pthread; ++i) { // handle elements per thread numbner of elements 
        uint32_t next = globalId * elem_pthread + i; // index of element to be handled
        if (next < in_size) { // check that we're in bounds
            // get b bits corresponding to the current iteration
            uint32_t rank = (keys[next] >> (it * bits)) & (hist_size - 1); 
            histogram[rank*width+threadId] = 1;
        }
    }

    __syncthreads(); // waiting to flag the entire array
    if(threadId < hist_size){
       // BlockScan(temp_storage).ExclusiveSum(histogram[threadId, thread_data]);
    }

    __syncthreads(); // waiting for the scan
    if (threadId < hist_size) {
       int offset = blockIdx.x * hist_size + threadId; // calculate position in global memory for histogram value
       g_hist[offset] = histogram[threadId*width+width-1]; // copy histogram value to global memory
    }
}


/*
 * This kernel transposes a matrix
 * 
 * @odata: transpose matrix
 * @idata: matrix to transpose
*/
#define TILE_DIM 16 
#define BLOCK_ROWS 4
__global__ void transposeNaive(uint32_t *odata, uint32_t *idata) {
  int x = blockIdx.x * TILE_DIM + threadIdx.x;
  int y = blockIdx.y * TILE_DIM + threadIdx.y;
  int width = gridDim.x * TILE_DIM;

  for (int j = 0; j < TILE_DIM; j+= BLOCK_ROWS)
    odata[x*width + (y+j)] = idata[(y+j)*width + x];
}

/*
 * This kernel transposes a matrix
 * 
 * @odata: transpose matrix
 * @idata: matrix to transpose
*/
__global__ void transpose(uint32_t *odata, uint32_t *idata) {
    // no boundary checking needed as we spawn with a block size of 256, and our histogram array size is a multiple of 256

    __shared__ uint32_t tile[256];

    uint32_t globalId = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t index = 16*(threadIdx.x%16) + (threadIdx.x/16);

    tile[index] = idata[globalId];
    __syncthreads();

    odata[globalId] = tile[threadIdx.x];
}


/**
 * This kernel uses a scan histogram to recompute the position of
 * each element of keys
 *
 * @keys: input array to sort
 * @output: output array
 * @hist: scanned histogram
 * @bits: number of bits considered for one keys
 * @elem_pthread: number of elements managed by one thread
 * @key_size: size of @keys
 * @hist_size: size of @hist
 * @it: number of iteration, 
 *  (corresponding to the bits being processed; eg. it = 0 => first 4 bits)
 *
*/
__global__ void scatter(uint32_t *keys, uint32_t *output, 
                        uint32_t *hist, uint32_t bits, 
                        uint32_t elem_pthread,
                        uint32_t key_size, uint32_t hist_size, uint32_t it) {
    uint32_t globalId = blockIdx.x * blockDim.x + threadIdx.x;
    if(!globalId){
        printf("keys GPU: [");
        for(uint32_t i = 0; i < key_size; ++i){
            printf("%d, ", keys[i]);
        }
        printf("]\n");

        printf("hist GPU: [");
        for(uint32_t i = 0; i < hist_size; ++i){
            printf("%d, ", hist[i]);
        }
        printf("]\n");
    }

    for (uint32_t i = 0; i < elem_pthread; ++i) { // handle elements per thread numbner of elements 
        uint32_t next = globalId * elem_pthread + i; // index of element to be handled
        if (next < key_size) { // check that we're in bounds
            // get b bits corresponding to the current iteration
            uint32_t rank = 
                (keys[next] >> (it * bits)) & (hist_size - 1); 
            int index = atomicAdd(&hist[rank], -1); // atomically decrease the value of the bucket at index rank
            //printf("next: %d, key: %d, rank: %d, index: %d\n", next, keys[next], rank, index);
            output[index] = keys[next];
        }
    }
}

__global__ void array_from_scan(uint32_t *odata, uint32_t *idata, int hist_size, int b) {
    if (threadIdx.x < (1 << b)) {
        int width = hist_size / (1 << b);

        int index = width * (threadIdx.x + 1) - 1;
        odata[threadIdx.x] = idata[index];
    }
}

__global__ void scatter_coalesced(uint32_t *keys, uint32_t *output, 
                        uint32_t *hist_T_scanned, uint32_t *hist,
                        uint32_t bits, uint32_t elem_pthread,
                        uint32_t key_size, uint32_t hist_size, uint32_t it) {

    // scan the histogram and save the data in shared memory

    uint32_t num_buckets = 1 << bits;
    __shared__ uint32_t local_hist[16];
    __shared__ uint32_t scanned_hist[16];

    if (threadIdx.x < num_buckets) { // copy histogram corresponding to block into shared memory
        int tmp = hist[num_buckets * blockIdx.x + threadIdx.x];

        // Specialize BlockScan for a 1D block of 16 threads of type int
        typedef cub::BlockScan<int, 16> BlockScan;

        // Allocate shared memory for BlockScan
        __shared__ typename BlockScan::TempStorage temp_storage;

        // Obtain a segment of consecutive items that are blocked across threads
    
        // Collectively compute the block-wide exclusive prefix sum
        BlockScan(temp_storage).ExclusiveSum(tmp, tmp);
        scanned_hist[threadIdx.x] = tmp;
    }
    
    __syncthreads();

    // assumptions for this to work:
    // - presorted array on the block level
    // - scan of the histogram must be exclusive
    // - scan transpose histogram must be exclusive

    for (int i = 0; i < 4; ++i) { // each thread scatters 4 elements
        //TODO: read in coalesced fascion (see gilles kernel 1)
        uint32_t next_local = i * blockDim.x + threadIdx.x; 
        uint32_t next_global = blockIdx.x * blockDim.x * elem_pthread + next_local;

        if (next_global < key_size) {
            uint32_t element = keys[next_global];
            uint32_t rank = (element >> (it * bits)) & (hist_size - 1);
            uint32_t number_of_elems_in_previous_ranks_locally = scanned_hist[rank];
            int local_rank_offset = next_local - number_of_elems_in_previous_ranks_locally; // the nth element with this rank in our block

            // arr: [0, 1]  , hit: [1, 1], scan_hist: [0, 1]
            // index: 1, num_elem_previous_local = 1, local_rank_offset = 0

            //debug
            if (local_rank_offset < 0) {
                printf("--------- local rank offset smaller than 0 ---------");
            } 

            // hist_T_scanned[rank][blockIdx.x]
            int global_rank_index_start = hist_T_scanned[rank * (hist_size / num_buckets) + blockIdx.x];
            int global_index = global_rank_index_start + local_rank_offset;
            output[global_index] = element;
        }
    }
}



/**
 * This kernel transposes a given Matrix in a coalesced fascion
 * source: https://developer.download.nvidia.com/compute/DevZone/C/html_x64/6_Advanced/transpose/doc/MatrixTranspose.pdf
*/
//__global__ void transposeCoalesced(float *odata,
//            float *idata, int width, int height)
//{
//  __shared__ float tile[TILE_DIM][TILE_DIM];
//  int xIndex = blockIdx.x*TILE_DIM + threadIdx.x;
//  int yIndex = blockIdx.y*TILE_DIM + threadIdx.y;
//  int index_in = xIndex + (yIndex)*width;
//  xIndex = blockIdx.y * TILE_DIM + threadIdx.x;
//  yIndex = blockIdx.x * TILE_DIM + threadIdx.y;
//  int index_out = xIndex + (yIndex)*height;
//  for (int i=0; i<TILE_DIM; i+=BLOCK_ROWS) {
//    tile[threadIdx.y+i][threadIdx.x] =
//      idata[index_in+i*width];
//  }
//  __syncthreads();
//  for (int i=0; i<TILE_DIM; i+=BLOCK_ROWS) {
//    odata[index_out+i*height] =
//      tile[threadIdx.x][threadIdx.y+i];
//  }
//}

#endif // !RADIX_KERNEL