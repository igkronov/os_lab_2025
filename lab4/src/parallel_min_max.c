#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <signal.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

/* Массив PID-ов дочерних процессов. Заполняется родителем после fork.
 * Нужен обработчику on_alarm, чтобы знать, кому посылать SIGKILL. */
static pid_t *child_pids = NULL;

/* Сколько всего детей (размер массива child_pids). */
static int child_count = 0;

/* Флаг: сработал ли таймаут.
 * volatile — компилятор не должен кешировать значение в регистре,
 *            обработчик меняет его асинхронно.
 * sig_atomic_t — гарантирует атомарную запись/чтение в обработчике. */
static volatile sig_atomic_t timeout_fired = 0;

/* Обработчик SIGALRM — вызывается ядром через timeout_sec секунд
 * после alarm(timeout_sec).
 * Внутри можно вызывать только async-signal-safe функции:
 * kill безопасен, printf/malloc/free/exit — нет. */
static void on_alarm(int sig) {
    (void)sig;              /* параметр не используем */
    timeout_fired = 1;      /* отметить, что таймаут сработал */

    /* Разослать SIGKILL всем детям.
     * SIGKILL нельзя перехватить — процесс умирает гарантированно. */
    for (int i = 0; i < child_count; i++) {
        if (child_pids[i] > 0) {
            kill(child_pids[i], SIGKILL);
        }
    }
}

int main(int argc, char **argv) {
    /* Параметры командной строки. */
    int seed = -1;              /* зерно генератора массива */
    int array_size = -1;        /* размер массива */
    int pnum = -1;              /* число дочерних процессов */
    int timeout_sec = -1;       /* -1 означает таймаут не задан */
    bool with_files = false;    /* true = файлы, false = pipe */

    /* Разбор аргументов через getopt_long. */
    while (true) {
        int current_optind = optind ? optind : 1;

        /* Описание длинных опций.
         * val = 0 означает: getopt_long вернёт 0,
         * а реальный индекс опции будет в option_index. */
        static struct option options[] = {
            {"seed",       required_argument, 0, 0},   /* --seed N */
            {"array_size", required_argument, 0, 0},   /* --array_size N */
            {"pnum",       required_argument, 0, 0},   /* --pnum N */
            {"by_files",   no_argument,       0, 'f'}, /* --by_files */
            {"timeout",    required_argument, 0, 0},   /* --timeout N */
            {0, 0, 0, 0}                                /* терминатор */
        };

        int option_index = 0;
        /* "f" в optstring — короткая опция -f (то же, что --by_files) */
        int c = getopt_long(argc, argv, "f", options, &option_index);

        if (c == -1) break;     /* аргументы кончились */

        switch (c) {
            case 0:
                /* Сработала длинная опция — смотрим option_index */
                switch (option_index) {
                    case 0: seed = atoi(optarg); break;
                    case 1: array_size = atoi(optarg); break;
                    case 2: pnum = atoi(optarg); break;
                    case 3: with_files = true; break;
                    case 4:
                        /* --timeout N: должен быть положительным,
                         * alarm(0) отменяет таймер, поэтому 0 бессмысленен. */
                        timeout_sec = atoi(optarg);
                        if (timeout_sec <= 0) {
                            fprintf(stderr, "timeout must be positive\n");
                            return 1;
                        }
                        break;
                    default:
                        printf("Index %d is out of options\n", option_index);
                }
                break;
            case 'f':
                with_files = true;
                break;
            case '?':
                break;
            default:
                printf("getopt returned character code 0%o?\n", c);
        }
    }

    /* Проверки аргументов. */
    if (optind < argc) {
        printf("Has at least one no option argument\n");
        return 1;
    }
    if (seed == -1 || array_size == -1 || pnum == -1) {
        printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--by_files] [--timeout N]\n",
               argv[0]);
        return 1;
    }
    if (pnum <= 0 || array_size <= 0) {
        printf("pnum and array_size must be positive\n");
        return 1;
    }

    /* Генерация массива. */
    int *array = malloc(sizeof(int) * array_size);
    if (!array) { perror("malloc"); return 1; }
    GenerateArray(array, array_size, seed);         /* из utils.c */

    /* Подготовка каналов или файлов ДО fork.
     * pipes[i] — pipe i-го ребёнка: [0] — чтение, [1] — запись. */
    int (*pipes)[2] = NULL;
    char **filenames = NULL;

    if (!with_files) {
        pipes = malloc(sizeof(int[2]) * pnum);
        for (int i = 0; i < pnum; i++) {
            if (pipe(pipes[i]) == -1) { perror("pipe"); return 1; }
        }
    } else {
        filenames = malloc(sizeof(char*) * pnum);
        for (int i = 0; i < pnum; i++) {
            filenames[i] = malloc(64);
            snprintf(filenames[i], 64, "minmax_part_%d.txt", i);
        }
    }

    /* Массив PID-ов для обработчика.
     * Заполняем -1, чтобы on_alarm не убил случайный PID
     * до того, как fork заполнит настоящие. */
    child_pids = malloc(sizeof(pid_t) * pnum);
    child_count = pnum;
    for (int i = 0; i < pnum; i++) child_pids[i] = -1;

    int active_child_processes = 0;   /* сколько детей ещё живо */

    /* Старт замера времени. */
    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    /* Создаём pnum детей. */
    for (int i = 0; i < pnum; i++) {
        pid_t child_pid = fork();
        if (child_pid >= 0) {
            child_pids[i] = child_pid;      /* запомнить PID для on_alarm */
            active_child_processes += 1;

            if (child_pid == 0) {
                /* ===== РЕБЁНОК ===== */

                /* Границы i-го куска массива: [begin, end) */
                unsigned int begin = (unsigned int)(array_size * i) / pnum;
                unsigned int end   = (unsigned int)(array_size * (i + 1)) / pnum;

                /* Считаем min/max своего куска */
                struct MinMax mm = GetMinMax(array, begin, end);

                if (with_files) {
                    FILE *f = fopen(filenames[i], "w");
                    if (!f) { perror("fopen"); exit(1); }
                    fprintf(f, "%d %d\n", mm.min, mm.max);
                    fclose(f);
                } else {
                    /* Закрываем чужие концы, чтобы не было утечек
                     * и родитель увидел EOF, когда все дети закроют запись. */
                    for (int j = 0; j < pnum; j++) {
                        close(pipes[j][0]);
                        if (j != i) close(pipes[j][1]);
                    }
                    write(pipes[i][1], &mm, sizeof(mm));
                    close(pipes[i][1]);
                }

                free(array);
                exit(0);
            }
        } else {
            printf("Fork failed!\n");
            return 1;
        }
    }

    /* Родитель: закрываем концы записи pipe.
     * Иначе read не получит EOF, когда дети завершатся. */
    if (!with_files) {
        for (int i = 0; i < pnum; i++) close(pipes[i][1]);
    }

    /* Установка обработчика и таймера. */
    if (timeout_sec > 0) {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = on_alarm;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;                /* БЕЗ SA_RESTART:
                                         * waitpid прервётся с EINTR,
                                         * и цикл увидит таймаут */
        sigaction(SIGALRM, &sa, NULL);

        alarm((unsigned int)timeout_sec);
    }

    /* Ожидание детей (неблокирующее). */
    while (active_child_processes > 0) {
        int status;
        pid_t w = waitpid(-1, &status, WNOHANG);

        if (w > 0) {
            active_child_processes -= 1;
        } else if (w == 0) {
            usleep(1000);   /* 1 мс — не грузить CPU в холостом цикле */
        } else {
            if (errno == ECHILD) break;   /* детей больше нет */
            if (errno == EINTR)  continue;/* прервано SIGALRM */
            perror("waitpid");
            break;
        }
    }

    /* Отмена таймера, если дети успели раньше. */
    if (timeout_sec > 0) {
        alarm(0);
    }

    /* Чтение результатов. */
    struct MinMax min_max;
    min_max.min = INT_MAX;
    min_max.max = INT_MIN;

    for (int i = 0; i < pnum; i++) {
        struct MinMax part;
        part.min = INT_MAX;
        part.max = INT_MIN;

        if (with_files) {
            FILE *f = fopen(filenames[i], "r");
            if (f) {
                if (fscanf(f, "%d %d", &part.min, &part.max) != 2) {
                    part.min = INT_MAX;
                    part.max = INT_MIN;
                }
                fclose(f);
            }
            unlink(filenames[i]);
            free(filenames[i]);
        } else {
            ssize_t n = read(pipes[i][0], &part, sizeof(part));
            if (n != (ssize_t)sizeof(part)) {
                /* ребёнок убит — нейтральные значения */
                part.min = INT_MAX;
                part.max = INT_MIN;
            }
            close(pipes[i][0]);
        }

        if (part.min < min_max.min) min_max.min = part.min;
        if (part.max > min_max.max) min_max.max = part.max;
    }

    /* Подсчёт прошедшего времени. */
    struct timeval finish_time;
    gettimeofday(&finish_time, NULL);
    double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
    elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

    /* Освобождение памяти. */
    free(array);
    free(pipes);        /* free(NULL) безопасен */
    free(filenames);    /* free(NULL) безопасен */
    free(child_pids);

    /* Вывод результата. */
    if (timeout_fired) {
        printf("Timeout fired: some children were killed by SIGKILL\n");
    }

    printf("Min: %d\n", min_max.min);
    printf("Max: %d\n", min_max.max);
    printf("Elapsed time: %fms\n", elapsed_time);

    fflush(NULL);
    return 0;
}