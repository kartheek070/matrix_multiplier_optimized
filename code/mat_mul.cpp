#include "mat_mul.h"

void load_A(
    data_t *A,
    data_t local_A[MAX_M][MAX_K],
    int M,
    int K);

void load_B(
    data_t *B,
    data_t local_B[MAX_K][MAX_N],
    int K,
    int N);

void build_products(
    data_t local_A[MAX_M][MAX_K],
    data_t local_B[MAX_K][MAX_N],
    acc_t products[MAX_K],
    int r,
    int c);

acc_t reduce_tree(
    acc_t products[MAX_K]);

void compute(
    data_t local_A[MAX_M][MAX_K],
    data_t local_B[MAX_K][MAX_N],
    data_t *C,
    int M,
    int N);
void reduce_level1(
    acc_t in[MAX_K],
    acc_t out[MAX_K/2]);

void reduce_level2(
    acc_t in[MAX_K/2],
    acc_t out[MAX_K/4]);

void reduce_level3(
    acc_t in[MAX_K/4],
    acc_t out[MAX_K/8]);

void reduce_level4(
    acc_t in[MAX_K/8],
    acc_t out[MAX_K/16]);

acc_t reduce_level5(
    acc_t in[MAX_K/16]);

void matrix_mul(
    data_t *A,
    data_t *B,
    data_t *C,
    int M,
    int K,
    int N
){
#pragma HLS INTERFACE m_axi port=A offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=B offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=C offset=slave bundle=gmem2
#pragma HLS INTERFACE s_axilite port=M bundle=control
#pragma HLS INTERFACE s_axilite port=K bundle=control
#pragma HLS INTERFACE s_axilite port=N bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
	   data_t local_A[MAX_M][MAX_K];
	   data_t local_B[MAX_K][MAX_N];
//#pragma HLS DATAFLOW
	    load_A(A, local_A, M, K);

	    load_B(B, local_B, K, N);

	    compute(local_A, local_B, C, M, N);
}

void load_A(
    data_t *A,
    data_t local_A[MAX_M][MAX_K],
    int M,
    int K)
{
	#pragma HLS ARRAY_PARTITION variable=local_A type=complete dim=2
	    load_A:
	    for (int r = 0; r < MAX_M; r++) {
	#pragma HLS LOOP_TRIPCOUNT min=1 max=MAX_M avg=16
	        for (int k = 0; k < MAX_K; k++) {
	#pragma HLS LOOP_TRIPCOUNT min=1 max=MAX_K avg=16
	            local_A[r][k] = (r < M && k < K) ? A[r*K + k] : (data_t)0;
	        }
	    }
}


void load_B(
    data_t *B,
    data_t local_B[MAX_K][MAX_N],
    int K,
    int N)
{
#pragma HLS ARRAY_PARTITION variable=local_B type=complete dim=1
   load_B:
   for (int r = 0; r < MAX_K; r++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=MAX_K avg=16
       for (int k = 0; k < MAX_N; k++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=MAX_N avg=16
           local_B[r][k] = (r < K && k < N) ? B[r*N + k] : (data_t)0;
       }
   }
}





void compute(
    data_t local_A[MAX_M][MAX_K],
    data_t local_B[MAX_K][MAX_N],
    data_t *C,
    int M,
    int N)
{
	  row_loop: for (int r = 0; r < M; r++) {
	#pragma HLS LOOP_TRIPCOUNT min=1 max=MAX_M avg=16
	        column_loop: for (int c = 0; c < N; c++) {
	#pragma HLS LOOP_TRIPCOUNT min=1 max=MAX_N avg=16
	#pragma HLS PIPELINE II=1
//#pragma HLS DATAFLOW
	        	acc_t products[MAX_K];

	        	acc_t level1[MAX_K/2];
	        	acc_t level2[MAX_K/4];
	        	acc_t level3[MAX_K/8];
	        	acc_t level4[MAX_K/16];

	        	#pragma HLS ARRAY_PARTITION variable=level1 complete
	        	#pragma HLS ARRAY_PARTITION variable=level2 complete
	        	#pragma HLS ARRAY_PARTITION variable=level3 complete
	        	#pragma HLS ARRAY_PARTITION variable=level4 complete

	        	build_products(local_A,local_B,products,r,c);

	        	reduce_level1(products,level1);

	        	reduce_level2(level1,level2);

	        	reduce_level3(level2,level3);

	        	reduce_level4(level3,level4);

	        	acc_t sum = reduce_level5(level4);

	        	C[r*N+c]=sum;

	        	C[r*N+c] = (data_t)sum;
	        }
	    }
}

void build_products(
    data_t local_A[MAX_M][MAX_K],
    data_t local_B[MAX_K][MAX_N],
    acc_t products[MAX_K],
    int r,
    int c)
{
#pragma HLS ARRAY_PARTITION variable=products type=complete dim=1
            build_products: for (int k = 0; k < MAX_K; k++) {
#pragma HLS UNROLL
                products[k] = local_A[r][k] * local_B[k][c];
            }
}

//acc_t reduce_tree(
//    acc_t products[MAX_K])
//{
//	 acc_t level1[MAX_K/2];
//	#pragma HLS ARRAY_PARTITION variable=level1 type=complete dim=1
//	            L1: for (int i = 0; i < MAX_K/2; i++) {
//	#pragma HLS UNROLL
//	                level1[i] = products[2*i] + products[2*i + 1];
//	            }
//
//	            acc_t level2[MAX_K/4];
//	#pragma HLS ARRAY_PARTITION variable=level2 type=complete dim=1
//	            L2: for (int i = 0; i < MAX_K/4; i++) {
//	#pragma HLS UNROLL
//	                level2[i] = level1[2*i] + level1[2*i + 1];
//	            }
//
//	            acc_t level3[MAX_K/8];
//	#pragma HLS ARRAY_PARTITION variable=level3 type=complete dim=1
//	            L3: for (int i = 0; i < MAX_K/8; i++) {
//	#pragma HLS UNROLL
//	                level3[i] = level2[2*i] + level2[2*i + 1];
//	            }
//
//	            acc_t level4[MAX_K/16];
//	#pragma HLS ARRAY_PARTITION variable=level4 type=complete dim=1
//	            L4: for (int i = 0; i < MAX_K/16; i++) {
//	#pragma HLS UNROLL
//	                level4[i] = level3[2*i] + level3[2*i + 1];
//	            }
//
//	            acc_t sum = level4[0] + level4[1];
//	            return sum;
//}
void reduce_level1(
    acc_t in[MAX_K],
    acc_t out[MAX_K/2])
{
#pragma HLS INLINE off
#pragma HLS ARRAY_PARTITION variable=in complete
#pragma HLS ARRAY_PARTITION variable=out complete

L1:
    for(int i=0;i<MAX_K/2;i++)
    {
#pragma HLS UNROLL
        out[i]=in[2*i]+in[2*i+1];
    }
}

void reduce_level2(
    acc_t in[MAX_K/2],
    acc_t out[MAX_K/4])
{
#pragma HLS INLINE off
#pragma HLS ARRAY_PARTITION variable=in complete
#pragma HLS ARRAY_PARTITION variable=out complete

L2:
    for(int i=0;i<MAX_K/4;i++)
    {
#pragma HLS UNROLL
        out[i]=in[2*i]+in[2*i+1];
    }
}

void reduce_level3(
    acc_t in[MAX_K/4],
    acc_t out[MAX_K/8])
{
#pragma HLS INLINE off
#pragma HLS ARRAY_PARTITION variable=in complete
#pragma HLS ARRAY_PARTITION variable=out complete

L3:
    for(int i=0;i<MAX_K/8;i++)
    {
#pragma HLS UNROLL
        out[i]=in[2*i]+in[2*i+1];
    }
	}
void reduce_level4(
    acc_t in[MAX_K/8],
    acc_t out[MAX_K/16])
{
#pragma HLS INLINE off
#pragma HLS ARRAY_PARTITION variable=in complete
#pragma HLS ARRAY_PARTITION variable=out complete

L4:
    for(int i=0;i<MAX_K/16;i++)
    {
#pragma HLS UNROLL
        out[i]=in[2*i]+in[2*i+1];
    }
	}
acc_t reduce_level5(
    acc_t in[MAX_K/16])
{
#pragma HLS INLINE off

    return in[0]+in[1];
}
