#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // ===== РЕБЁНОК =====
        // Заменяем свой образ на sequential_min_max
        execl("./sequential_min_max",   // путь к программе
              "sequential_min_max",     // argv[0] — имя программы
              "42",                     // argv[1] — seed
              "100",                    // argv[2] — array_size
              (char *)NULL);            // конец списка аргументов

        // Сюда попадаем только если execl вернулся — значит, ошибка
        perror("execl");
        exit(1);
    }

    // ===== РОДИТЕЛЬ =====
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        printf("Child exited with status %d\n", WEXITSTATUS(status));
    } else {
        printf("Child terminated abnormally\n");
    }

    return 0;
}