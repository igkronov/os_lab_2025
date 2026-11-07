#include "sum_lib.h"

long long SumArray(const int *array, unsigned int n) {
    long long s = 0;
    for (unsigned int i = 0; i < n; i++) {
        s += array[i];
    }
    return s;
}

void *SumWorker(void *arg) {
    SumTask *task = (SumTask *)arg;
    long long s = 0;
    for (unsigned int i = task->begin; i < task->end; i++) {
        s += task->array[i];
    }
    task->partial_sum = s;
    return NULL;
}
