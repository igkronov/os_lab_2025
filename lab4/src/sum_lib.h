#ifndef SUM_LIB_H
#define SUM_LIB_H

#include <pthread.h>

/* Аргументы для одного потока */
typedef struct {
    const int *array;
    unsigned int begin;
    unsigned int end;
    long long partial_sum;
} SumTask;

/* Считает сумму array[begin..end) и кладёт в task->partial_sum */
void *SumWorker(void *arg);

/* Последовательная сумма — удобно для сверки/отладки */
long long SumArray(const int *array, unsigned int n);

#endif