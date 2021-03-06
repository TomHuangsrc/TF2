/* Copyright 2019 Inspur Corporation. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0
    
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef __RESNET50_PRUNED__
#define __RESNET50_PRUNED__

//---------------------------------------------------------------------//
//                                                                     //
//            DEVICE/HOST COMMON RESNET50_PRUNED PARAMETERS            //
//                                                                     //
//---------------------------------------------------------------------//                                                                  

//
// Debug Parameters
//

//#define CONCAT_LAYER_DEBUG

//#define STATIC_CYCLE
//#define PRINT_N 1
//#define PRINT_CYCLE
#define PRINT_SEQUENCER_INDEX
//#define PRINT_IPOOL_INPUT
//#define PRINT_PE_INPUT
//#define PRINT_PE_OUTPUT
//#define PRINT_POOL_INPUT
//#define PRINT_POOL_OUTPUT

//
// Configuration Parameters
//

#define NUM_LAYER 54 
#define NUM_CONVOLUTIONS 54
#define NUM_Q_LAYERS (NUM_CONVOLUTIONS + 1) // 1 is for input data Quantization value.

#define INPUT_IMAGE_C 3
#define INPUT_IMAGE_H 224
#define INPUT_IMAGE_W 224
#define FIRST_FILTER_SIZE 7

#define MAX_OUT_CHANNEL 2048
#define MAX_POOL_OUTPUT_WVEC CEIL(56, W_VECTOR)

// the maximum pool window size
#define POOL_WINDOW_MAX 3

// set size of feature map DDR
#define DDR_PAGE_SIZE0 (CEIL(256, C_VECTOR) * 56 * CEIL(56, W_VECTOR))
#define DDR_PAGE_SIZE1 (CEIL(256, C_VECTOR) * 56 * CEIL(56, W_VECTOR))
#define DDR_SIZE (DDR_PAGE_SIZE0 + DDR_PAGE_SIZE1)

// set size of feature map cache
#define CACHE_PAGE_SIZE (CEIL(256, C_VECTOR) * 56 * CEIL(56, W_VECTOR)) // single buffer size: be calculated from the layer of max slice size
#define CACHE_SIZE (CACHE_PAGE_SIZE * 2)

// set size of filter buffer
#define FILTER_CACHE_PAGE_SIZE1 (NEXT_DIVISIBLE(512, C_VECTOR) * 3 * CEIL(3, FW_VECTOR))
#define FILTER_CACHE_PAGE_SIZE2 (NEXT_DIVISIBLE(2048, C_VECTOR * FW_VECTOR))
#define FILTER_CACHE_PAGE_SIZE  (MYMAX2(FILTER_CACHE_PAGE_SIZE1, FILTER_CACHE_PAGE_SIZE2))

#define FILTER_CACHE_PAGE_DEPTH (NEXT_POWER_OF_2(CEIL(FILTER_CACHE_PAGE_SIZE, C_VECTOR))) // size of every cache is FW_VECTOR * C_VECTOR
#define FILTER_CACHE_DEPTH (FILTER_CACHE_PAGE_DEPTH * DOUBLE_BUFFER_DIM)

// read filter data from ddr with this many cycle intervals in order not to
// get stalled because of ddr bandwidth bottleneck
// N_VECTOR kernels are each reading in round-robin order C_VECTOR*FW_VECTOR floats.
// We want to wait ceil( (C_VECTOR * FW_VECTOR) / DDR_BANDWIDTH_IN_FLOATS ) cycles between each ddr access from kernels
#define FILTER_DDR_READ_STEP1 (CEIL((C_VECTOR * FW_VECTOR), DDR_BANDWIDTH_IN_BYTES))
#define FILTER_DDR_READ_STEP2 (CEIL((C_VECTOR * 1), DDR_BANDWIDTH_IN_BYTES))

#define FEATURE_DDR_READ_STEP 1

// Set size of host filter and bias buffer of each layer.
#define MAX_FILTER_SIZE1 (NEXT_POWER_OF_2(CEIL(2048, C_VECTOR) * 1 * CEIL(1, FW_VECTOR) * NEXT_DIVISIBLE(2048, N_VECTOR) * NEXT_POWER_OF_2(FW_VECTOR * C_VECTOR)))
#define MAX_FILTER_SIZE2 (NEXT_POWER_OF_2(CEIL(1024, C_VECTOR) * 3 * CEIL(3, FW_VECTOR) * NEXT_DIVISIBLE(1024, N_VECTOR) * NEXT_POWER_OF_2(FW_VECTOR * C_VECTOR)))
#define MAX_FILTER_SIZE_TEMP ((MAX_FILTER_SIZE1 > MAX_FILTER_SIZE2) ? MAX_FILTER_SIZE1 : MAX_FILTER_SIZE2)
#define MAX_FILTER_SIZE (CEIL(MAX_FILTER_SIZE_TEMP, NEXT_POWER_OF_2(FW_VECTOR * C_VECTOR)))

#define MAX_BIAS_SIZE NEXT_DIVISIBLE(2048, N_VECTOR)

// used by pool.cl
#define EDGE_H (POOL_WINDOW_MAX - 1)
#define EDGE_W (POOL_WINDOW_MAX - 1)
#define WVEC_ITER (CEIL(kOwEndWithOffsetMax, OW_VECTOR))
#define NNVEC_ITER (CEIL(N_VECTOR, NARROW_N_VECTOR))
#define EDGE_H_BUFFER_SIZE (WVEC_ITER * NNVEC_ITER)
#define EDGE_W_BUFFER_SIZE (NNVEC_ITER)

#define DDR_BLOCK_SIZE DDR_PAGE_SIZE0
#define D0 0
#define D1 0
#define D2 DDR_BLOCK_SIZE 

#define OUTPUT_OFFSET (2 * DDR_BLOCK_SIZE * NEXT_POWER_OF_2(W_VECTOR * NARROW_N_VECTOR))

#define C1 0
#define C2 CACHE_PAGE_SIZE

//
// Convolution Parametres of each layer
//

CONSTANT int kCacheReadBase[NUM_CONVOLUTIONS] = {
  C1,         // conv1
  C2,         // res2a_branch1
  C2, C1, C2, // res2a 
  C1, C2, C1, // res2b 
  C2, C1, C2, // res2c 
  C1,         // res3a_branch1
  C1, C2, C1, // res3a 
  C2, C1, C2, // res3b 
  C1, C2, C1, // res3c 
  C2, C1, C2, // res3d
  C1,         // res4a_branch1
  C1, C2, C1, // res4a 
  C2, C1, C2, // res4b 
  C1, C2, C1, // res4c 
  C2, C1, C2, // res4d 
  C1, C2, C1, // res4e 
  C2, C1, C2, // res4f
  C1,         // res5a_branch1
  C1, C2, C1, // res5a 
  C2, C1, C2, // res5b
  C1, C2, C1, // res5c 
  C2          // FC1000
};

CONSTANT int kCacheWriteBase[NUM_CONVOLUTIONS] = {
  C2,         // conv1
  C1,         // res2a_branch1
  C1, C2, C1, // res2a 
  C2, C1, C2, // res2b 
  C1, C2, C1, // res2c 
  C2,         // res3a_branch1
  C2, C1, C2, // res3a 
  C1, C2, C1, // res3b 
  C2, C1, C2, // res3c 
  C1, C2, C1, // res3d
  C2,         // res4a_branch1
  C2, C1, C2, // res4a 
  C1, C2, C1, // res4b 
  C2, C1, C2, // res4c 
  C1, C2, C1, // res4d 
  C2, C1, C2, // res4e 
  C1, C2, C1, // res4f
  C2,         // res5a_branch1
  C2, C1, C2, // res5a 
  C1, C2, C1, // res5b
  C2, C1, C2, // res5c 
  C1          // FC1000
};

// This is for residual addition in pool_tail rather than preload for feature writer.
CONSTANT int kDDRReadBase[NUM_CONVOLUTIONS] = { 
  D0,         // conv1
  D0,         // res2a_branch1
  D0, D0, D1, // res2a 
  D0, D0, D2, // res2b 
  D0, D0, D1, // res2c 
  D2,         // res3a_branch1
  D0, D0, D1, // res3a 
  D0, D0, D2, // res3b 
  D0, D0, D1, // res3c 
  D0, D0, D2, // res3d
  D1,         // res4a_branch1
  D0, D0, D2, // res4a 
  D0, D0, D1, // res4b 
  D0, D0, D2, // res4c 
  D0, D0, D1, // res4d 
  D0, D0, D2, // res4e 
  D0, D0, D1, // res4f
  D2,         // res5a_branch1
  D0, D0, D1, // res5a 
  D0, D0, D2, // res5b
  D0, D0, D1, // res5c 
  D0          // FC1000
};

CONSTANT int kDDRWriteBase[NUM_CONVOLUTIONS] = {
  D0,         // conv1
  D1,         // res2a_branch1
  D0, D0, D2, // res2a 
  D0, D0, D1, // res2b 
  D0, D0, D2, // res2c 
  D1,         // res3a_branch1
  D0, D0, D2, // res3a 
  D0, D0, D1, // res3b 
  D0, D0, D2, // res3c 
  D0, D0, D1, // res3d
  D2,         // res4a_branch1
  D0, D0, D1, // res4a 
  D0, D0, D2, // res4b 
  D0, D0, D1, // res4c 
  D0, D0, D2, // res4d 
  D0, D0, D1, // res4e 
  D0, D0, D2, // res4f
  D1,         // res5a_branch1
  D0, D0, D2, // res5a 
  D0, D0, D1, // res5b
  D0, D0, D2, // res5c 
  D0          // FC1000
};

CONSTANT bool kCacheWriteEnable[NUM_CONVOLUTIONS] = {
  1,       // conv1
  0,       // res2a_branch1
  1, 1, 1, // res2a 
  1, 1, 1, // res2b 
  1, 1, 1, // res2c 
  0,       // res3a_branch1
  1, 1, 1, // res3a 
  1, 1, 1, // res3b 
  1, 1, 1, // res3c 
  1, 1, 1, // res3d
  0,       // res4a_branch1
  1, 1, 1, // res4a 
  1, 1, 1, // res4b 
  1, 1, 1, // res4c 
  1, 1, 1, // res4d 
  1, 1, 1, // res4e 
  1, 1, 1, // res4f
  0,       // res5a_branch1
  1, 1, 1, // res5a 
  1, 1, 1, // res5b
  1, 1, 0, // res5c 
  1        // FC1000
};

CONSTANT bool kDDRWriteEnable[NUM_CONVOLUTIONS] = {
  0,       // conv1
  1,       // res2a_branch1
  0, 0, 1, // res2a 
  0, 0, 1, // res2b 
  0, 0, 1, // res2c 
  1,       // res3a_branch1
  0, 0, 1, // res3a 
  0, 0, 1, // res3b 
  0, 0, 1, // res3c 
  0, 0, 1, // res3d
  1,       // res4a_branch1
  0, 0, 1, // res4a 
  0, 0, 1, // res4b 
  0, 0, 1, // res4c 
  0, 0, 1, // res4d 
  0, 0, 1, // res4e 
  0, 0, 1, // res4f
  1,       // res5a_branch1
  0, 0, 1, // res5a 
  0, 0, 1, // res5b
  0, 0, 1, // res5c 
  0        // FC1000
};

CONSTANT bool kEndPoolEnable[NUM_CONVOLUTIONS] = {
  0,       // conv1
  0,       // res2a_branch1
  0, 0, 0, // res2a 
  0, 0, 0, // res2b 
  0, 0, 0, // res2c 
  0,       // res3a_branch1
  0, 0, 0, // res3a 
  0, 0, 0, // res3b 
  0, 0, 0, // res3c 
  0, 0, 0, // res3d
  0,       // res4a_branch1
  0, 0, 0, // res4a 
  0, 0, 0, // res4b 
  0, 0, 0, // res4c 
  0, 0, 0, // res4d 
  0, 0, 0, // res4e 
  0, 0, 0, // res4f
  0,       // res5a_branch1
  0, 0, 0, // res5a 
  0, 0, 0, // res5b
  0, 0, 1, // res5c 
  0        // FC1000
};

CONSTANT bool kAdditionEnable[NUM_CONVOLUTIONS] = {
  0,       // conv1
  0,       // res2a_branch1
  0, 0, 1, // res2a 
  0, 0, 1, // res2b 
  0, 0, 1, // res2c 
  0,       // res3a_branch1
  0, 0, 1, // res3a 
  0, 0, 1, // res3b 
  0, 0, 1, // res3c 
  0, 0, 1, // res3d
  0,       // res4a_branch1
  0, 0, 1, // res4a 
  0, 0, 1, // res4b 
  0, 0, 1, // res4c 
  0, 0, 1, // res4d 
  0, 0, 1, // res4e 
  0, 0, 1, // res4f
  0,       // res5a_branch1
  0, 0, 1, // res5a 
  0, 0, 1, // res5b
  0, 0, 1, // res5c 
  0        // FC1000
};

CONSTANT bool kAdditionReluEnable[NUM_CONVOLUTIONS] = {
  0,       // conv1
  0,       // res2a_branch1
  0, 0, 1, // res2a 
  0, 0, 1, // res2b 
  0, 0, 1, // res2c 
  0,       // res3a_branch1
  0, 0, 1, // res3a 
  0, 0, 1, // res3b 
  0, 0, 1, // res3c 
  0, 0, 1, // res3d
  0,       // res4a_branch1
  0, 0, 1, // res4a 
  0, 0, 1, // res4b 
  0, 0, 1, // res4c 
  0, 0, 1, // res4d 
  0, 0, 1, // res4e 
  0, 0, 1, // res4f
  0,       // res5a_branch1
  0, 0, 1, // res5a 
  0, 0, 1, // res5b
  0, 0, 1, // res5c 
  0        // FC1000
};

CONSTANT bool kReluEnable[NUM_CONVOLUTIONS] = {
  true,             // conv1
  false,             // res2a_branch1
  true, true, false, // res2a 
  true, true, false, // res2b 
  true, true, false, // res2c 
  false,             // res3a_branch1
  true, true, false, // res3a 
  true, true, false, // res3b 
  true, true, false, // res3c 
  true, true, false, // res3d
  false,             // res4a_branch1
  true, true, false, // res4a 
  true, true, false, // res4b 
  true, true, false, // res4c 
  true, true, false, // res4d 
  true, true, false, // res4e 
  true, true, false, // res4f
  false,             // res5a_branch1
  true, true, false, // res5a 
  true, true, false, // res5b
  true, true, false, // res5c 
  false             // fc1000
};

CONSTANT int kFilterSize[NUM_CONVOLUTIONS] = {
  3, 
  1, 
  1, 3, 1, 
  1, 3, 1,
  1, 3, 1, 
  1,
  1, 3, 1,
  1, 3, 1, 
  1, 3, 1,
  1, 3, 1, 
  1, 
  1, 3, 1,
  1, 3, 1, 
  1, 3, 1,
  1, 3, 1, 
  1, 3, 1,
  1, 3, 1, 
  1, 
  1, 3, 1,
  1, 3, 1, 
  1, 3, 1,
  1, // fc1000
};
CONSTANT int kFilterSizeMax = 3;

// Conv pad
CONSTANT int kPadWidth[NUM_CONVOLUTIONS] = {
  0, 
  0, 
  0, 1, 0, 
  0, 1, 0,
  0, 1, 0, 
  0,
  0, 1, 0,
  0, 1, 0, 
  0, 1, 0,
  0, 1, 0, 
  0, 
  0, 1, 0,
  0, 1, 0, 
  0, 1, 0,
  0, 1, 0, 
  0, 1, 0,
  0, 1, 0, 
  0, 
  0, 1, 0,
  0, 1, 0, 
  0, 1, 0,
  0  // fc1000
};

// Conv pad
CONSTANT int kPadHeight[NUM_CONVOLUTIONS] = {
  0, 
  0, 
  0, 1, 0, 
  0, 1, 0,
  0, 1, 0, 
  0,
  0, 1, 0,
  0, 1, 0, 
  0, 1, 0,
  0, 1, 0, 
  0, 
  0, 1, 0,
  0, 1, 0, 
  0, 1, 0,
  0, 1, 0, 
  0, 1, 0,
  0, 1, 0, 
  0, 
  0, 1, 0,
  0, 1, 0, 
  0, 1, 0,
  0  // fc1000
};

// input image of each convolution stage
CONSTANT int kInputWidth[NUM_CONVOLUTIONS] = {
  114,
  56,
  56, 56, 56, 
  56, 56, 56, 
  56, 56, 56,
  56,  
  56, 28, 28, 
  28, 28, 28, 
  28, 28, 28, 
  28, 28, 28,
  28,
  28, 14, 14,
  14, 14, 14,
  14, 14, 14,
  14, 14, 14,
  14, 14, 14,
  14, 14, 14,
  14,
  14, 7,  7,
  7,  7,  7,
  7,  7,  7,
  1   // fc1000
};
CONSTANT int kInputWidthMax = 114;

CONSTANT int kInputHeight[NUM_CONVOLUTIONS] = {
  114,
  56,
  56, 56, 56, 
  56, 56, 56, 
  56, 56, 56,
  56,  
  56, 28, 28, 
  28, 28, 28, 
  28, 28, 28, 
  28, 28, 28,
  28,
  28, 14, 14,
  14, 14, 14,
  14, 14, 14,
  14, 14, 14,
  14, 14, 14,
  14, 14, 14,
  14,
  14, 7,  7,
  7,  7,  7,
  7,  7,  7,
  1   // fc1000
};
CONSTANT int kInputHeightMax = 114;

// output image of each convolution stage
CONSTANT int kOutputWidth[NUM_CONVOLUTIONS] = {
  112,
  56,
  56, 56, 56, 
  56, 56, 56,
  56, 56, 56,
  56,
  56, 28, 28,
  28, 28, 28, 
  28, 28, 28,
  28, 28, 28,
  28,
  28, 14, 14,
  14, 14, 14, 
  14, 14, 14,
  14, 14, 14, 
  14, 14, 14,
  14, 14, 14,
  14,
  14, 7,  7,
  7,  7,  7,  
  7,  7,  7,
  1   // fc1000
};
CONSTANT int kOutputWidthMax = 112;

CONSTANT int kOutputHeight[NUM_CONVOLUTIONS] = {
  112,
  56,
  56, 56, 56, 
  56, 56, 56,
  56, 56, 56,
  56,
  56, 28, 28,
  28, 28, 28, 
  28, 28, 28,
  28, 28, 28,
  28,
  28, 14, 14,
  14, 14, 14, 
  14, 14, 14,
  14, 14, 14, 
  14, 14, 14,
  14, 14, 14,
  14,
  14, 7,  7,
  7,  7,  7,  
  7,  7,  7,
  1   // fc1000
};

CONSTANT int kOutputHeightMax = 112;

CONSTANT int kInputChannels[NUM_CONVOLUTIONS] = {
  27, // pool1
  64,
  64,   32,  32, // res2a
  256,  32,  48, // res2b
  256,  32,  64, // res2c
  256,      
  256,  64,  80,// res3a
  512,  64,  96,// res3b
  512,  32,  64,// res3c
  512,  80,  96,// res3d
  512,      
  512,  96,  128,// res4a
  1024, 96,  128,// res4b
  1024, 112, 144,// res4c
  1024, 96,  144,// res4d
  1024, 160, 160,// res4e
  1024, 192, 160,// res4f
  1024,     
  1024, 240, 288,// res5a
  2048, 240, 224,// res5b
  2048, 448, 336,// res5c
  2048  // fc1000
};

// how much filter data (number of float_vec_t reads) we need to prefetch at each stage of convolution
// formula : kCvecEnd * R * kFWvecEnd
CONSTANT int kFilterLoadSize[NUM_CONVOLUTIONS] = {
  CEIL(27,   C_VECTOR) * 3 * CEIL(3, FW_VECTOR),   // conv1
  CEIL(64,   C_VECTOR * FW_VECTOR) ,   // res2a_branch1
  CEIL(64,   C_VECTOR * FW_VECTOR),   // res2a
  CEIL(32,   C_VECTOR) * 3 * CEIL(3, FW_VECTOR),   
  CEIL(32,   C_VECTOR * FW_VECTOR), 
  CEIL(256,  C_VECTOR * FW_VECTOR),  // res2b
  CEIL(32,   C_VECTOR) * 3 * CEIL(3, FW_VECTOR), 
  CEIL(48,   C_VECTOR * FW_VECTOR), 
  CEIL(256,  C_VECTOR * FW_VECTOR),  // res2c
  CEIL(32,   C_VECTOR) * 3 * CEIL(3, FW_VECTOR), 
  CEIL(64,   C_VECTOR * FW_VECTOR), 
  CEIL(256,  C_VECTOR * FW_VECTOR),  // res3a_branch1
  CEIL(256,  C_VECTOR * FW_VECTOR),  // res3a
  CEIL(64,   C_VECTOR) * 3 * CEIL(3, FW_VECTOR),   
  CEIL(80,   C_VECTOR * FW_VECTOR),  
  CEIL(512,  C_VECTOR * FW_VECTOR),  // res3b
  CEIL(64,   C_VECTOR) * 3 * CEIL(3, FW_VECTOR),  
  CEIL(96,   C_VECTOR * FW_VECTOR),  
  CEIL(512,  C_VECTOR * FW_VECTOR),  // res3c
  CEIL(32,   C_VECTOR) * 3 * CEIL(3, FW_VECTOR),  
  CEIL(64,   C_VECTOR * FW_VECTOR),  
  CEIL(512,  C_VECTOR * FW_VECTOR),  // res3d
  CEIL(80,   C_VECTOR) * 3 * CEIL(3, FW_VECTOR),  
  CEIL(96,   C_VECTOR * FW_VECTOR),  
  CEIL(512,  C_VECTOR * FW_VECTOR),  // res4a_branch1 
  CEIL(512,  C_VECTOR * FW_VECTOR),  // res4a
  CEIL(96,   C_VECTOR) * 3 * CEIL(3, FW_VECTOR), 
  CEIL(128,  C_VECTOR * FW_VECTOR),  
  CEIL(1024, C_VECTOR * FW_VECTOR), // res4b
  CEIL(96,   C_VECTOR) * 3 * CEIL(3, FW_VECTOR), 
  CEIL(128,  C_VECTOR * FW_VECTOR), 
  CEIL(1024, C_VECTOR * FW_VECTOR), // res4c
  CEIL(112,  C_VECTOR) * 3 * CEIL(3, FW_VECTOR), 
  CEIL(144,  C_VECTOR * FW_VECTOR), 
  CEIL(1024, C_VECTOR * FW_VECTOR), // res4d
  CEIL(96,   C_VECTOR) * 3 * CEIL(3, FW_VECTOR), 
  CEIL(144,  C_VECTOR * FW_VECTOR), 
  CEIL(1024, C_VECTOR * FW_VECTOR), // res4e
  CEIL(160,  C_VECTOR) * 3 * CEIL(3, FW_VECTOR), 
  CEIL(160,  C_VECTOR * FW_VECTOR), 
  CEIL(1024, C_VECTOR * FW_VECTOR), // res4f
  CEIL(192,  C_VECTOR) * 3 * CEIL(3, FW_VECTOR), 
  CEIL(160,  C_VECTOR * FW_VECTOR), 
  CEIL(1024, C_VECTOR * FW_VECTOR), // res5a_branch1
  CEIL(1024, C_VECTOR * FW_VECTOR), // res5a
  CEIL(240,  C_VECTOR) * 3 * CEIL(3, FW_VECTOR), 
  CEIL(288,  C_VECTOR * FW_VECTOR), 
  CEIL(2048, C_VECTOR * FW_VECTOR), // res5b
  CEIL(240,  C_VECTOR) * 3 * CEIL(3, FW_VECTOR), 
  CEIL(224,  C_VECTOR * FW_VECTOR), 
  CEIL(2048, C_VECTOR * FW_VECTOR), // res5c
  CEIL(448,  C_VECTOR) * 3 * CEIL(3, FW_VECTOR), 
  CEIL(336,  C_VECTOR * FW_VECTOR), 
  CEIL(2048, C_VECTOR * FW_VECTOR)  // fc1000
};

CONSTANT int kOutputChannels[NUM_CONVOLUTIONS] = {
  64,  // pool1
  256,
  32,   32,  256, // res2a
  32,   48,  256, // res2b
  32,   64,  256, // res2c
  512,
  64,   80,  512, // res3a
  64,   96,  512, // res3b
  32,   64,  512, // res3c
  80,   96,  512, // res3d
  1024, 
  96,   128, 1024, // res4a
  96,   128, 1024, // res4b
  112,  144, 1024, // res4c
  96,   144, 1024, // res4d
  160,  160, 1024, // res4e
  192,  160, 1024, // res4f
  2048, 
  240,  288, 2048, // res5a
  240,  224, 2048, // res5b
  448,  336, 2048, // res5c
  1000   // fc1000
};
CONSTANT int kOutputChannelsMax = 2048;

CONSTANT int kWvecEnd[NUM_CONVOLUTIONS] = {
  CEIL(114, W_VECTOR),
  CEIL(56, W_VECTOR),
  CEIL(56, W_VECTOR), CEIL(56, W_VECTOR), CEIL(56, W_VECTOR), 
  CEIL(56, W_VECTOR), CEIL(56, W_VECTOR), CEIL(56, W_VECTOR), 
  CEIL(56, W_VECTOR), CEIL(56, W_VECTOR), CEIL(56, W_VECTOR),
  CEIL(56, W_VECTOR),  
  CEIL(56, W_VECTOR), CEIL(28, W_VECTOR), CEIL(28, W_VECTOR), 
  CEIL(28, W_VECTOR), CEIL(28, W_VECTOR), CEIL(28, W_VECTOR), 
  CEIL(28, W_VECTOR), CEIL(28, W_VECTOR), CEIL(28, W_VECTOR), 
  CEIL(28, W_VECTOR), CEIL(28, W_VECTOR), CEIL(28, W_VECTOR),
  CEIL(28, W_VECTOR),
  CEIL(28, W_VECTOR), CEIL(14, W_VECTOR), CEIL(14, W_VECTOR),
  CEIL(14, W_VECTOR), CEIL(14, W_VECTOR), CEIL(14, W_VECTOR),
  CEIL(14, W_VECTOR), CEIL(14, W_VECTOR), CEIL(14, W_VECTOR),
  CEIL(14, W_VECTOR), CEIL(14, W_VECTOR), CEIL(14, W_VECTOR),
  CEIL(14, W_VECTOR), CEIL(14, W_VECTOR), CEIL(14, W_VECTOR),
  CEIL(14, W_VECTOR), CEIL(14, W_VECTOR), CEIL(14, W_VECTOR),
  CEIL(14, W_VECTOR),
  CEIL(14, W_VECTOR), CEIL(7, W_VECTOR), CEIL(7, W_VECTOR),
  CEIL(7, W_VECTOR), CEIL(7, W_VECTOR), CEIL(7, W_VECTOR),
  CEIL(7, W_VECTOR), CEIL(7, W_VECTOR), CEIL(7, W_VECTOR),
  CEIL(1, W_VECTOR)  // fc1000
};
CONSTANT int kWvecEndMax = CEIL(114, W_VECTOR);

CONSTANT int kConvStride[NUM_CONVOLUTIONS] = {
  1,       // conv1
  1,       // res2a_branch1
  1, 1, 1, // res2a 
  1, 1, 1, // res2b 
  1, 1, 1, // res2c 
  2,       // res3a_branch1
  2, 1, 1, // res3a 
  1, 1, 1, // res3b 
  1, 1, 1, // res3c 
  1, 1, 1, // res3d
  2,       // res4a_branch1
  2, 1, 1, // res4a 
  1, 1, 1, // res4b 
  1, 1, 1, // res4c 
  1, 1, 1, // res4d 
  1, 1, 1, // res4e 
  1, 1, 1, // res4f
  2,       // res5a_branch1
  2, 1, 1, // res5a 
  1, 1, 1, // res5b
  1, 1, 1, // res5c 
  1        // FC1000
};

//
// POOL
//

CONSTANT bool kIpoolEnable[NUM_CONVOLUTIONS] = {
  false,             // conv1
  false,             // res2a_branch1
  false, false, false, // res2a 
  false, false, false, // res2b 
  false, false, false, // res2c 
  false,             // res3a_branch1
  false, false, false, // res3a 
  false, false, false, // res3b 
  false, false, false, // res3c 
  false, false, false, // res3d
  false,             // res4a_branch1
  false, false, false, // res4a 
  false, false, false, // res4b 
  false, false, false, // res4c 
  false, false, false, // res4d 
  false, false, false, // res4e 
  false, false, false, // res4f
  false,             // res5a_branch1
  false, false, false, // res5a 
  false, false, false, // res5b
  false, false, false, // res5c 
  false             // fc1000
};

CONSTANT bool kPoolEnable[NUM_CONVOLUTIONS] = {
  true,             // conv1
  false,             // res2a_branch1
  false, false, false, // res2a 
  false, false, false, // res2b 
  false, false, false, // res2c 
  false,             // res3a_branch1
  false, false, false, // res3a 
  false, false, false, // res3b 
  false, false, false, // res3c 
  false, false, false, // res3d
  false,             // res4a_branch1
  false, false, false, // res4a 
  false, false, false, // res4b 
  false, false, false, // res4c 
  false, false, false, // res4d 
  false, false, false, // res4e 
  false, false, false, // res4f
  false,             // res5a_branch1
  false, false, false, // res5a 
  false, false, false, // res5b
  false, false, false, // res5c 
  false             // fc1000
};

CONSTANT bool kBiasEnable[NUM_CONVOLUTIONS] = {
	true,
	false,
	false,false,false,
	false,false,false,
	false,false,false,
  false,
	false,false,false,
	false,false,false,
	false,false,false,
	false,false,false,
 	false,
	false,false,false,
	false,false,false,
	false,false,false,
	false,false,false,
	false,false,false,
	false,false,false,
	false,
	false,false,false,
	false,false,false,
	false,false,false,
	true
};

// window over which to pool
CONSTANT int kPoolWindow[NUM_CONVOLUTIONS] = {
  3, 
  3, 
  3, 3, 3, 
  3, 3, 3,
  3, 3, 3, 
  3,
  3, 3, 3,
  3, 3, 3, 
  3, 3, 3,
  3, 3, 3, 
  3, 
  3, 3, 3,
  3, 3, 3, 
  3, 3, 3,
  3, 3, 3, 
  3, 3, 3,
  3, 3, 3, 
  3, 
  3, 3, 3,
  3, 3, 3, 
  3, 3, 3,
  3  // fc1000
};

// 0 - max pooling
// 1 - average pooling
CONSTANT int kPoolType[NUM_CONVOLUTIONS] = {
  0, 
  0, 
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0,
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0, 
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0, 
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0  // fc1000
};

CONSTANT bool kPoolStride2[NUM_CONVOLUTIONS] = {
  true,             // conv1
  false,             // res2a_branch1
  false, false, false, // res2a 
  false, false, false, // res2b 
  false, false, false, // res2c 
  false,             // res3a_branch1
  false, false, false, // res3a 
  false, false, false, // res3b 
  false, false, false, // res3c 
  false, false, false, // res3d
  false,             // res4a_branch1
  false, false, false, // res4a 
  false, false, false, // res4b 
  false, false, false, // res4c 
  false, false, false, // res4d 
  false, false, false, // res4e 
  false, false, false, // res4f
  false,             // res5a_branch1
  false, false, false, // res5a 
  false, false, false, // res5b
  false, false, false, // res5c 
  false             // fc1000
};

CONSTANT int kPoolOutputWidth[NUM_CONVOLUTIONS] = {
  56,
  56,
  56, 56, 56, 
  56, 56, 56,
  56, 56, 56,// res2c
  28,
  28, 28, 28,
  28, 28, 28,
  28, 28, 28,
  28, 28, 28,// res3d
  14,
  14, 14, 14,
  14, 14, 14,
  14, 14, 14,
  14, 14, 14,
  14, 14, 14,
  14, 14, 14,// res4f
  7,
  7, 7, 7,
  7, 7, 7,
  7, 7, 7, // res5c
  1   // fc1000
};
CONSTANT int kPoolOutputWidthMax = 56;

CONSTANT int kPoolOutputHeight[NUM_CONVOLUTIONS] = {
  56,
  56,
  56, 56, 56, 
  56, 56, 56,
  56, 56, 56,// res2c
  28,
  28, 28, 28,
  28, 28, 28,
  28, 28, 28,
  28, 28, 28,// res3d
  14,
  14, 14, 14,
  14, 14, 14,
  14, 14, 14,
  14, 14, 14,
  14, 14, 14,
  14, 14, 14,// res4f
  7,
  7, 7, 7,
  7, 7, 7,
  7, 7, 7, // res5c
  1   // fc1000
};
CONSTANT int kPoolOutputHeightMax = 56;

CONSTANT int kPoolOutputWvecEnd[NUM_CONVOLUTIONS] = {
  CEIL(56, W_VECTOR),
  CEIL(56, W_VECTOR), 
  CEIL(56, W_VECTOR), CEIL(56, W_VECTOR), CEIL(56, W_VECTOR), 
  CEIL(56, W_VECTOR), CEIL(56, W_VECTOR), CEIL(56, W_VECTOR), 
  CEIL(56, W_VECTOR), CEIL(56, W_VECTOR), CEIL(56, W_VECTOR), // res2c
  CEIL(28, W_VECTOR), 
  CEIL(28, W_VECTOR), CEIL(28, W_VECTOR), CEIL(28, W_VECTOR),
  CEIL(28, W_VECTOR), CEIL(28, W_VECTOR), CEIL(28, W_VECTOR),
  CEIL(28, W_VECTOR), CEIL(28, W_VECTOR), CEIL(28, W_VECTOR),
  CEIL(28, W_VECTOR), CEIL(28, W_VECTOR), CEIL(28, W_VECTOR), // res3d
  CEIL(14, W_VECTOR), 
  CEIL(14, W_VECTOR), CEIL(14, W_VECTOR), CEIL(14, W_VECTOR),
  CEIL(14, W_VECTOR), CEIL(14, W_VECTOR), CEIL(14, W_VECTOR),
  CEIL(14, W_VECTOR), CEIL(14, W_VECTOR), CEIL(14, W_VECTOR),
  CEIL(14, W_VECTOR), CEIL(14, W_VECTOR), CEIL(14, W_VECTOR),
  CEIL(14, W_VECTOR), CEIL(14, W_VECTOR), CEIL(14, W_VECTOR),
  CEIL(14, W_VECTOR), CEIL(14, W_VECTOR), CEIL(14, W_VECTOR), // res4f
  CEIL(7,  W_VECTOR), 
  CEIL(7, W_VECTOR),  CEIL(7, W_VECTOR), CEIL(7, W_VECTOR),
  CEIL(7,  W_VECTOR), CEIL(7, W_VECTOR), CEIL(7, W_VECTOR),
  CEIL(7,  W_VECTOR), CEIL(7, W_VECTOR), CEIL(7, W_VECTOR), // res5c
  CEIL(1, W_VECTOR) // fc1000
};

//pool output feature map hight
CONSTANT int kOhEndWithOffset[NUM_CONVOLUTIONS] = {
  112 + POOL_OFFSET_P,
  56 + POOL_OFFSET_P,
  56 + POOL_OFFSET_P, 56 + POOL_OFFSET_P, 56 + POOL_OFFSET_P, 
  56 + POOL_OFFSET_P, 56 + POOL_OFFSET_P, 56 + POOL_OFFSET_P,
  56 + POOL_OFFSET_P, 56 + POOL_OFFSET_P, 56 + POOL_OFFSET_P, 
  28 + POOL_OFFSET_P,
  28 + POOL_OFFSET_P, 28 + POOL_OFFSET_P, 28 + POOL_OFFSET_P,
  28 + POOL_OFFSET_P, 28 + POOL_OFFSET_P, 28 + POOL_OFFSET_P, 
  28 + POOL_OFFSET_P, 28 + POOL_OFFSET_P, 28 + POOL_OFFSET_P,
  28 + POOL_OFFSET_P, 28 + POOL_OFFSET_P, 28 + POOL_OFFSET_P, 
  14 + POOL_OFFSET_P,
  14 + POOL_OFFSET_P, 14 + POOL_OFFSET_P, 14 + POOL_OFFSET_P,
  14 + POOL_OFFSET_P, 14 + POOL_OFFSET_P, 14 + POOL_OFFSET_P, 
  14 + POOL_OFFSET_P, 14 + POOL_OFFSET_P, 14 + POOL_OFFSET_P,
  14 + POOL_OFFSET_P, 14 + POOL_OFFSET_P, 14 + POOL_OFFSET_P, 
  14 + POOL_OFFSET_P, 14 + POOL_OFFSET_P, 14 + POOL_OFFSET_P,
  14 + POOL_OFFSET_P, 14 + POOL_OFFSET_P, 14 + POOL_OFFSET_P,
   7 + POOL_OFFSET_P,
   7 + POOL_OFFSET_P,  7 + POOL_OFFSET_P,  7 + POOL_OFFSET_P,
   7 + POOL_OFFSET_P,  7 + POOL_OFFSET_P,  7 + POOL_OFFSET_P,  
   7 + POOL_OFFSET_P,  7 + POOL_OFFSET_P,  7 + POOL_OFFSET_P,
   1 + POOL_OFFSET_P
};
CONSTANT int kOhEndWithOffsetMax = 112 + POOL_OFFSET_P;

//pool output feature map width
CONSTANT int kOwEndWithOffset[NUM_CONVOLUTIONS] = {
  112 + POOL_OFFSET_Q,
  56 + POOL_OFFSET_Q,
  56 + POOL_OFFSET_Q, 56 + POOL_OFFSET_Q, 56 + POOL_OFFSET_Q, 
  56 + POOL_OFFSET_Q, 56 + POOL_OFFSET_Q, 56 + POOL_OFFSET_Q,
  56 + POOL_OFFSET_Q, 56 + POOL_OFFSET_Q, 56 + POOL_OFFSET_Q, 
  56 + POOL_OFFSET_Q,
  56 + POOL_OFFSET_Q, 28 + POOL_OFFSET_Q, 28 + POOL_OFFSET_Q,
  28 + POOL_OFFSET_Q, 28 + POOL_OFFSET_Q, 28 + POOL_OFFSET_Q, 
  28 + POOL_OFFSET_Q, 28 + POOL_OFFSET_Q, 28 + POOL_OFFSET_Q,
  28 + POOL_OFFSET_Q, 28 + POOL_OFFSET_Q, 28 + POOL_OFFSET_Q, 
  28 + POOL_OFFSET_Q,
  28 + POOL_OFFSET_Q, 14 + POOL_OFFSET_Q, 14 + POOL_OFFSET_Q,
  14 + POOL_OFFSET_Q, 14 + POOL_OFFSET_Q, 14 + POOL_OFFSET_Q, 
  14 + POOL_OFFSET_Q, 14 + POOL_OFFSET_Q, 14 + POOL_OFFSET_Q,
  14 + POOL_OFFSET_Q, 14 + POOL_OFFSET_Q, 14 + POOL_OFFSET_Q, 
  14 + POOL_OFFSET_Q, 14 + POOL_OFFSET_Q, 14 + POOL_OFFSET_Q,
  14 + POOL_OFFSET_Q, 14 + POOL_OFFSET_Q, 14 + POOL_OFFSET_Q,
  14 + POOL_OFFSET_Q,
  14 + POOL_OFFSET_Q,  7 + POOL_OFFSET_Q,  7 + POOL_OFFSET_Q,
   7 + POOL_OFFSET_Q,  7 + POOL_OFFSET_Q,  7 + POOL_OFFSET_Q,  
   7 + POOL_OFFSET_Q,  7 + POOL_OFFSET_Q,  7 + POOL_OFFSET_Q,
   1 + POOL_OFFSET_Q
};
CONSTANT int kOwEndWithOffsetMax = 112 + POOL_OFFSET_Q;

CONSTANT int kFWvecEnd[NUM_CONVOLUTIONS] = {
  CEIL(3, FW_VECTOR),
  CEIL(1, FW_VECTOR), 
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR), 
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR), 
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR), 
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR), CEIL(3, FW_VECTOR), CEIL(1, FW_VECTOR),
  CEIL(1, FW_VECTOR)  // fc1000
};
CONSTANT int kFWvecEndMax = CEIL(3, FW_VECTOR);

CONSTANT int kCvecEnd[NUM_CONVOLUTIONS] = {
  CEIL(27,  C_VECTOR),
  CEIL(64,  C_VECTOR),
  CEIL(64,  C_VECTOR),  CEIL(32, C_VECTOR),  CEIL(32, C_VECTOR), 
  CEIL(256, C_VECTOR),  CEIL(32, C_VECTOR),  CEIL(48, C_VECTOR), 
  CEIL(256, C_VECTOR),  CEIL(32, C_VECTOR),  CEIL(64, C_VECTOR), 
  CEIL(256, C_VECTOR),
  CEIL(256, C_VECTOR),  CEIL(64,  C_VECTOR), CEIL(80,  C_VECTOR),
  CEIL(512, C_VECTOR),  CEIL(64,  C_VECTOR), CEIL(96,  C_VECTOR), 
  CEIL(512, C_VECTOR),  CEIL(32,  C_VECTOR), CEIL(64,  C_VECTOR),
  CEIL(512, C_VECTOR),  CEIL(80,  C_VECTOR), CEIL(96,  C_VECTOR),
  CEIL(512, C_VECTOR),
  CEIL(512, C_VECTOR),  CEIL(96,  C_VECTOR), CEIL(128, C_VECTOR),
  CEIL(1024, C_VECTOR), CEIL(96,  C_VECTOR), CEIL(128, C_VECTOR), 
  CEIL(1024, C_VECTOR), CEIL(112, C_VECTOR), CEIL(144, C_VECTOR),
  CEIL(1024, C_VECTOR), CEIL(96,  C_VECTOR), CEIL(144, C_VECTOR), 
  CEIL(1024, C_VECTOR), CEIL(160, C_VECTOR), CEIL(160, C_VECTOR),
  CEIL(1024, C_VECTOR), CEIL(192, C_VECTOR), CEIL(160, C_VECTOR),
  CEIL(1024, C_VECTOR),
  CEIL(1024, C_VECTOR), CEIL(240, C_VECTOR), CEIL(288, C_VECTOR),
  CEIL(2048, C_VECTOR), CEIL(240, C_VECTOR), CEIL(224, C_VECTOR), 
  CEIL(2048, C_VECTOR), CEIL(448, C_VECTOR), CEIL(336, C_VECTOR),
  CEIL(2048, C_VECTOR)  // fc1000
};
CONSTANT int kCvecEndMax = CEIL(2048, C_VECTOR);

CONSTANT int kFilterCvecEnd[NUM_CONVOLUTIONS] = {
  CEIL(27,  C_VECTOR),
  CEIL(64,  C_VECTOR * FW_VECTOR),
  CEIL(64,  C_VECTOR * FW_VECTOR),  CEIL(32, C_VECTOR),  CEIL(32, C_VECTOR * FW_VECTOR),
  CEIL(256, C_VECTOR * FW_VECTOR),  CEIL(32, C_VECTOR),  CEIL(48, C_VECTOR * FW_VECTOR),
  CEIL(256, C_VECTOR * FW_VECTOR),  CEIL(32, C_VECTOR),  CEIL(64, C_VECTOR * FW_VECTOR),
  CEIL(256, C_VECTOR * FW_VECTOR),                                    
  CEIL(256, C_VECTOR * FW_VECTOR),  CEIL(64,  C_VECTOR), CEIL(80,  C_VECTOR * FW_VECTOR),
  CEIL(512, C_VECTOR * FW_VECTOR),  CEIL(64,  C_VECTOR), CEIL(96,  C_VECTOR * FW_VECTOR),
  CEIL(512, C_VECTOR * FW_VECTOR),  CEIL(32,  C_VECTOR), CEIL(64,  C_VECTOR * FW_VECTOR),
  CEIL(512, C_VECTOR * FW_VECTOR),  CEIL(80,  C_VECTOR), CEIL(96,  C_VECTOR * FW_VECTOR),
  CEIL(512, C_VECTOR * FW_VECTOR),                                    
  CEIL(512, C_VECTOR * FW_VECTOR),  CEIL(96,  C_VECTOR), CEIL(128, C_VECTOR * FW_VECTOR),
  CEIL(1024, C_VECTOR * FW_VECTOR), CEIL(96,  C_VECTOR), CEIL(128, C_VECTOR * FW_VECTOR),
  CEIL(1024, C_VECTOR * FW_VECTOR), CEIL(112, C_VECTOR), CEIL(144, C_VECTOR * FW_VECTOR),
  CEIL(1024, C_VECTOR * FW_VECTOR), CEIL(96,  C_VECTOR), CEIL(144, C_VECTOR * FW_VECTOR),
  CEIL(1024, C_VECTOR * FW_VECTOR), CEIL(160, C_VECTOR), CEIL(160, C_VECTOR * FW_VECTOR),
  CEIL(1024, C_VECTOR * FW_VECTOR), CEIL(192, C_VECTOR), CEIL(160, C_VECTOR * FW_VECTOR),
  CEIL(1024, C_VECTOR * FW_VECTOR),                                   
  CEIL(1024, C_VECTOR * FW_VECTOR), CEIL(240, C_VECTOR), CEIL(288, C_VECTOR * FW_VECTOR),
  CEIL(2048, C_VECTOR * FW_VECTOR), CEIL(240, C_VECTOR), CEIL(224, C_VECTOR * FW_VECTOR),
  CEIL(2048, C_VECTOR * FW_VECTOR), CEIL(448, C_VECTOR), CEIL(336, C_VECTOR * FW_VECTOR),
  CEIL(2048, C_VECTOR * FW_VECTOR)  // fc1000
};
CONSTANT int kFilterCvecEndMax = CEIL(2048, C_VECTOR * FW_VECTOR);

// input
CONSTANT int END_WW_MAX_INPUT_READER = CEIL(114, FW_VECTOR);

CONSTANT int kNvecEnd[NUM_CONVOLUTIONS] = {
  CEIL(64,   N_VECTOR),
  CEIL(256,  N_VECTOR),
  CEIL(32,   N_VECTOR), CEIL(32,  N_VECTOR), CEIL(256,  N_VECTOR),
  CEIL(32,   N_VECTOR), CEIL(48,  N_VECTOR), CEIL(256,  N_VECTOR),
  CEIL(32,   N_VECTOR), CEIL(64,  N_VECTOR), CEIL(256,  N_VECTOR),
  CEIL(512,  N_VECTOR),            
  CEIL(64,   N_VECTOR), CEIL(80,  N_VECTOR), CEIL(512,  N_VECTOR),
  CEIL(64,   N_VECTOR), CEIL(96,  N_VECTOR), CEIL(512,  N_VECTOR),
  CEIL(32,   N_VECTOR), CEIL(64,  N_VECTOR), CEIL(512,  N_VECTOR),
  CEIL(80,   N_VECTOR), CEIL(96,  N_VECTOR), CEIL(512,  N_VECTOR),
  CEIL(1024, N_VECTOR),        
  CEIL(96,   N_VECTOR), CEIL(128, N_VECTOR), CEIL(1024, N_VECTOR),
  CEIL(96,   N_VECTOR), CEIL(128, N_VECTOR), CEIL(1024, N_VECTOR),
  CEIL(112,  N_VECTOR), CEIL(144, N_VECTOR), CEIL(1024, N_VECTOR),
  CEIL(96,   N_VECTOR), CEIL(144, N_VECTOR), CEIL(1024, N_VECTOR),
  CEIL(160,  N_VECTOR), CEIL(160, N_VECTOR), CEIL(1024, N_VECTOR),
  CEIL(192,  N_VECTOR), CEIL(160, N_VECTOR), CEIL(1024, N_VECTOR),
  CEIL(2048, N_VECTOR),        
  CEIL(240,  N_VECTOR), CEIL(288, N_VECTOR), CEIL(2048, N_VECTOR),
  CEIL(240,  N_VECTOR), CEIL(224, N_VECTOR), CEIL(2048, N_VECTOR),
  CEIL(448,  N_VECTOR), CEIL(336, N_VECTOR), CEIL(2048, N_VECTOR),
  CEIL(1000, N_VECTOR)  // fc1000
};
CONSTANT int kNvecEndMax = CEIL(2048, N_VECTOR);

CONSTANT int kNEndWithOffset[NUM_CONVOLUTIONS] = { 
  64,  // pool1
  256,
  32,   32,  256, // res2a
  32,   48,  256, // res2b
  32,   64,  256, // res2c
  512,
  64,   80,  512, // res3a
  64,   96,  512, // res3b
  32,   64,  512, // res3c
  80,   96,  512, // res3d
  1024, 
  96,   128, 1024, // res4a
  96,   128, 1024, // res4b
  112,  144, 1024, // res4c
  96,   144, 1024, // res4d
  160,  160, 1024, // res4e
  192,  160, 1024, // res4f
  2048, 
  240,  288, 2048, // res5a
  240,  224, 2048, // res5b
  448,  336, 2048, // res5c
  1000   // fc1000
  };
CONSTANT int kNEndWithOffsetMax = 2048;

CONSTANT int kNStart[NUM_CONVOLUTIONS] = {
  0,
  0, 
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0,
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0, 
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0, 
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0  // fc1000
};

CONSTANT int kNEnd[NUM_CONVOLUTIONS] = {
  64,  // pool1
  256,
  32,   32,  256, // res2a
  32,   48,  256, // res2b
  32,   64,  256, // res2c
  512,
  64,   80,  512, // res3a
  64,   96,  512, // res3b
  32,   64,  512, // res3c
  80,   96,  512, // res3d
  1024, 
  96,   128, 1024, // res4a
  96,   128, 1024, // res4b
  112,  144, 1024, // res4c
  96,   144, 1024, // res4d
  160,  160, 1024, // res4e
  192,  160, 1024, // res4f
  2048, 
  240,  288, 2048, // res5a
  240,  224, 2048, // res5b
  448,  336, 2048, // res5c
  1000   // fc1000
};

CONSTANT int kPoolPad[NUM_CONVOLUTIONS] = {
  0,
	0,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	0,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	0, 
	0, 0,	0,
	0, 0, 0,
	0, 0,	0,
	0, 0,	0,
	0, 0,	0,
	0, 0,	0,
	0, 
  0, 0, 0,
	0, 0, 0,
  0, 0, 0,
	0
};

CONSTANT bool kBnEnable[NUM_CONVOLUTIONS] = {
  1,
	1,
	1, 1, 1,
	1, 1, 1,
	1, 1, 1,
	1,
	1, 1, 1,
	1, 1, 1,
	1, 1, 1,
	1, 1, 1,
	1,
	1, 1, 1,
	1, 1, 1,
  1, 1, 1,
	1, 1, 1,
	1, 1, 1,
	1, 1, 1,
	1,
	1, 1, 1,
  1, 1, 1,
	1, 1, 1,
  0

};

//
// only for host code
//

CONSTANT int kInputLayer[NUM_CONVOLUTIONS] = {
	0,
	1 ,
	1 ,	3 ,	4 ,
	5,	6 ,	7 ,
	8 ,	9 ,	10,
	11,
	11,	13,	14,
	15,	16,	17,
	18,	19,	20,
	21,	22,	23,
	24,
	24,	26,	27,
	28,	29,	30,
	31,	32,	33,
	34,	35,	36,
	37,	38,	39,
	40,	41,	42,
	43,
	43,	45,	46,
	47,	48,	49,
	50,	51,	52,
	53
};

CONSTANT bool kBranchTail[NUM_CONVOLUTIONS] = {
  0,
  0, 
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0,
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0, 
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0, 
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0  // fc1000
};

CONSTANT int kConcatLayer[NUM_CONVOLUTIONS] = {
  0,
  0, 
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0,
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0, 
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0, 0, 0, 
  0, 
  0, 0, 0,
  0, 0, 0, 
  0, 0, 0,
  0  // fc1000
};

CONSTANT int kSequencerIdleCycle[NUM_CONVOLUTIONS] = { 
  1000, 
  0, 
  0,    0,    0,
  0,    0,    0, 
  0,    0, 1000,
  0,          
  0,    0,    0, 
  0,    0,    0,
  0,    0,    0,
  0,    0, 1000,
  0,          
  0,    0,    0,
  0,    0,    0,
  0,    0,    0,
  0,    0,    0,
  0,    0,    0,
  0,    0, 1000,
  0,
  0, 1000,    0,
  0, 1000,    0,
  0, 1000,    0,
  0
};

#ifdef STATIC_CYCLE

CONSTANT int feature_writer_cycles[NUM_CONVOLUTIONS] = {
  1792,
  7168,
  896 ,
  896 ,
  7168,
  896 ,
  1344,
  7168,
  896 ,
  1792,
	7168,
	3584,
	448 ,
	560 ,
	3584,
	448 ,
	672 ,
	3584,
	224 ,
	448 ,
	3584,
	560 ,
	672 ,
	3584,
	1792,
	168 ,
	224 ,
	1792,
	168 ,
	224 ,
	1792,
	196 ,
	252 ,
	1792,
	168 ,
	252 ,
	1792,
	280 ,
	280 ,
	1792,
	336 ,
	280 ,
	1792,
	896 ,
	105 ,
	126 ,
	896 ,
	105 ,
	98  ,
	896 ,
	196 ,
	147 ,
	896 ,
	63
};

CONSTANT int filter_reader_conv_cycles[NUM_CONVOLUTIONS] = {
  384  ,
  512  ,
  64   ,
  192  ,
  256  ,
  192  ,
  288  ,
  256  ,
  192  ,
  384  ,
	512  ,
	3072 ,
	384  ,
	960  ,
	1024 ,
	704  ,
	1152 ,
	1024 ,
	352  ,
	384  ,
	1024 ,
	880  ,
	1440 ,
	1024 ,
	11264,
	1056 ,
	2304 ,
	3072 ,
	2112 ,
	2304 ,
	3072 ,
	2464 ,
	3024 ,
	3072 ,
	2112 ,
	2592 ,
	3072 ,
	3520 ,
	4800 ,
	4096 ,
	4224 ,
	5760 ,
	4096 ,
	45056,
	5280 ,
	12960,
	12288,
	10320,
	10080,
	10240,
	19264,
	28224,
	14336,
	43344
};

CONSTANT int conv_cycles[NUM_CONVOLUTIONS] = {
	61824 ,
	28672 ,
	3584  ,
	8064  ,
	14336 ,
	14336 ,
	12096 ,
	21504 ,
	14336 ,
	16128 ,
	28672 ,
	114688,
	14336 ,
	10080 ,
	17920 ,
	14336 ,
	12096 ,
	21504 ,
	7168  ,
	4032  ,
	14336 ,
	17920 ,
	15120 ,
	21504 ,
	114688,
	10752 ,
	6048  ,
	14464 ,
	10752 ,
	6048  ,
	14464 ,
	12544 ,
	7938  ,
	16228 ,
	10752 ,
	6804  ,
	16228 ,
	17920 ,
	12600 ,
	17992 ,
	21504 ,
	15120 ,
	17992 ,
	114688,
	13440 ,
	12870 ,
	16690 ,
	13440 ,
	9990  ,
	13134 ,
	25536 ,
	28056 ,
	19357 ,
	42784
};

CONSTANT int pool_cycles[NUM_CONVOLUTIONS] = {
  10488,
  8352 ,
  1044 ,
  1392 ,
  8352 ,
  1044 ,
  2088 ,
  8352 ,
  1044 ,
  2784 ,
	8352 ,
	8640 ,
	1080 ,
	900  ,
	4800 ,
	600  ,
	1080 ,
	4800 ,
	300  ,
	720  ,
	4800 ,
	750  ,
	1080 ,
	4800 ,
	5120 ,
	480  ,
	512  ,
	3072 ,
	288  ,
	512  ,
	3072 ,
	336  ,
	576  ,
	3072 ,
	288  ,
	576  ,
	3072 ,
	480  ,
	640  ,
	3072 ,
	576  ,
	640  ,
	3072 ,
	3456 ,
	405  ,
	324  ,
	2304 ,
	270  ,
	252  ,
	2304 ,
	504  ,
	378  ,
	2304 ,
	189
};

#define FEATURE_WRITER_CYCLE(i) feature_writer_cycles[i]
#define FILTER_READER_CONV_CYCLE(i) filter_reader_conv_cycles[i] 
#define CONV_CYCLE(i) conv_cycles[i]
#define POOL_CYCLE(i) pool_cycles[i]

#define CONV_TOTAL_CYCLE 1169415
#define INPUT_READER_CYCLE 3876 
#define FILTER_PRELOAD_CYCLE 96 
#define FILTER_READER_CONV_TOTAL_CYCLE 296064 
#define CONV_TOTAL_WRITE_CACHE 98756 
#define POOL_TOTAL_CYCLE 129788 
#define FEATURE_WRITER_TOTAL_CYCLE 78932 
#define END_POOL_TOTAL_CYCLE 896 

#endif

#ifndef STATIC_CYCLE

#define FEATURE_WRITER_CYCLE(i) FindFeatureWriterCycles(i)
#define FILTER_READER_CONV_CYCLE(i) FindFilterReaderConvCycles(i)
#define CONV_CYCLE(i) FindConvCycles(i)
#define POOL_CYCLE(i) FindPoolCycles(i)

#define CONV_TOTAL_CYCLE FindConvTotalCycles()
#define INPUT_READER_CYCLE FindInputReaderCycles()
#define FILTER_PRELOAD_CYCLE FindFilterPreloadCycles()
#define FILTER_READER_CONV_TOTAL_CYCLE FindFilterReaderConvTotalCycles()
#define CONV_TOTAL_WRITE_CACHE FindConvTotalWriteCache() 
#define POOL_TOTAL_CYCLE FindPoolTotalCycles() 
#define FEATURE_WRITER_TOTAL_CYCLE FindFeatureWriterTotalCycles() 
#define END_POOL_TOTAL_CYCLE FindEndPoolTotalCycles() 

#endif

#endif // __RESNET50_PRUNED__
