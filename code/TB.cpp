#include <iostream>
#include "mat_mul.h"

using namespace std;

int main()
{
    const int M = 2;
    const int K = 3;
    const int N = 2;

    data_t A[M*K] = {
        1,2,3,
        4,5,6
    };

    data_t B[K*N] = {
        1,2,
        3,4,
        5,6
    };

    data_t C[M*N];

    data_t expected[M*N] = {
        22,28,
        49,64
    };

    matrix_mul(A,B,C,M,K,N);

    bool pass = true;

    printf("Output Matrix\n");

    for(int i=0;i<M;i++)
    {
        for(int j=0;j<N;j++)
        {
            cout << C[i*N+j] << " ";

            if(C[i*N+j] != expected[i*N+j])
                pass = false;
        }
        cout << endl;
    }

    if(pass)
        printf("\nTEST PASSED\n");
    else
        printf("\nTEST FAILED\n");

    return 0;
}
