#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <getopt.h>
#include <sys/time.h>

#include "sum_lib.h"
#include "utils.h"

int main(int argc, char **argv) {
    /* Параметры командной строки: -1 означает "не задан". */
    int threads_num = -1;   /* количество потоков */
    int seed = -1;          /* зерно генератора массива */
    int array_size = -1;    /* размер массива */

    /* Описание длинных опций для getopt_long.
     * Каждая запись: имя, нужен ли аргумент, куда писать, что вернуть.
     * val = 0 означает: getopt_long вернёт 0,
     * а реальный индекс опции положит в option_index. */
    static struct option options[] = {
        {"threads_num", required_argument, 0, 0},   /* --threads_num N */
        {"seed",        required_argument, 0, 0},   /* --seed N */
        {"array_size",  required_argument, 0, 0},   /* --array_size N */
        {0, 0, 0, 0}                                 /* терминатор массива — обязателен */
    };

    /* Разбор аргументов.
     * getopt_long вызывается в цикле: каждый вызов — одна опция.
     * Возврат: 0 — длинная опция (индекс в option_index),
     *          '?' — ошибка, -1 — аргументы кончились. */
    int option_index = 0;
    int c;
    while ((c = getopt_long(argc, argv, "", options, &option_index)) != -1) {
        switch (c) {
            case 0:
                /* Сработала длинная опция — смотрим option_index:
                 * 0 — threads_num, 1 — seed, 2 — array_size.
                 * optarg — строка значения, atoi — строка в int. */
                switch (option_index) {
                    case 0: threads_num = atoi(optarg); break;
                    case 1: seed        = atoi(optarg); break;
                    case 2: array_size  = atoi(optarg); break;
                }
                break;
            case '?':
                /* Неизвестная опция — getopt_long уже напечатал сообщение. */
                return 1;
        }
    }

    /* Проверки: все параметры заданы и положительные. */
    if (threads_num <= 0 || seed < 0 || array_size <= 0) {
        fprintf(stderr,
            "Usage: %s --threads_num \"num\" --seed \"num\" --array_size \"num\"\n",
            argv[0]);
        return 1;
    }
    /* threads_num не больше array_size — иначе часть потоков
     * получит пустой диапазон [begin, end) с begin == end. */
    if (threads_num > array_size) {
        fprintf(stderr, "threads_num must be <= array_size\n");
        return 1;
    }

    /* Генерация массива — ВНЕ замера времени.
     * malloc может вернуть NULL — проверяем.
     * GenerateArray — из utils.c лабы №3 (заполняет по seed). */
    int *array = malloc(sizeof(int) * array_size);
    if (!array) { perror("malloc"); return 1; }
    GenerateArray(array, array_size, seed);

    /* threads[i] — ID i-го потока, нужен для pthread_join.
     * tasks[i]   — аргумент для i-го потока. */
    pthread_t *threads = malloc(sizeof(pthread_t) * threads_num);
    SumTask   *tasks   = malloc(sizeof(SumTask)   * threads_num);
    if (!threads || !tasks) { perror("malloc"); return 1; }

    /* Каждому потоку — свой непересекающийся диапазон [begin, end).
     * Формула делит [0, array_size) на threads_num частей без пропусков
     * и пересечений. (long long) — чтобы array_size * i не переполнил int.
     * partial_sum = 0 — начальное значение. */
    for (int i = 0; i < threads_num; i++) {
        tasks[i].array = array;
        tasks[i].begin = (unsigned int)((long long)array_size * i / threads_num);
        tasks[i].end   = (unsigned int)((long long)array_size * (i + 1) / threads_num);
        tasks[i].partial_sum = 0;
    }

    /* Старт замера — строго после генерации и подготовки задач,
     * чтобы в elapsed попал ТОЛЬКО подсчёт суммы. */
    struct timeval start, finish;
    gettimeofday(&start, NULL);

    /* Создаём потоки.
     * pthread_create(&tid, attr, start_routine, arg):
     *   &threads[i]  — куда записать ID;
     *   NULL         — атрибуты по умолчанию (joinable, стандартный стек);
     *   SumWorker    — функция потока void* f(void*);
     *   &tasks[i]    — один void* аргумент (попадает в arg внутри потока).
     * Возврат: 0 — успех, код ошибки — неудача (errno НЕ ставится). */
    for (int i = 0; i < threads_num; i++) {
        int err = pthread_create(&threads[i], NULL, SumWorker, &tasks[i]);
        if (err != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(err));
            return 1;
        }
    }

    /* Ждём завершения всех потоков и суммируем их partial_sum.
     * pthread_join(tid, NULL) блокируется, пока поток не завершится.
     * После join гарантированно видно всё, что поток записал
     * (точка синхронизации памяти). total — long long,
     * чтобы сумма не переполнила int. */
    long long total = 0;
    for (int i = 0; i < threads_num; i++) {
        pthread_join(threads[i], NULL);
        total += tasks[i].partial_sum;
    }

    /* Конец замера — после всех join, когда сумма реально посчитана. */
    gettimeofday(&finish, NULL);

    /* Время в миллисекундах: секунды*1000 + микросекунды/1000. */
    double elapsed_ms = (finish.tv_sec - start.tv_sec) * 1000.0
                      + (finish.tv_usec - start.tv_usec) / 1000.0;

    /* Вывод: %lld для long long, %f для double. */
    printf("Sum: %lld\n", total);
    printf("Elapsed time: %fms\n", elapsed_ms);

    /* Освобождение — после всех join, иначе потоки могли бы
     * обратиться к уже освобождённой памяти. */
    free(tasks);
    free(threads);
    free(array);
    return 0;
}