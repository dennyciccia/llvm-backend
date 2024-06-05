void function(int N, int* a, int* b, int* c, int* d){
    for(int i=0; i<N; i++){
        a[i] = 1 / b[i] * c[i];
    }

    for(int i=0; i<N; i++){
        d[i] = a[i] + c[i];
    }
}