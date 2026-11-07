#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // ребёнок: сразу завершается
        printf("Child (pid=%d) exiting\n", getpid());
        exit(0);
    }

    // родитель: НЕ вызывает wait — ребёнок становится зомби
    printf("Parent (pid=%d). Child pid=%d is now a zombie.\n",
           getpid(), pid);
    printf("Run in another terminal: ps -o pid,ppid,stat,comm -p %d\n", pid);

    // держим родителя живым, чтобы зомби был виден
    sleep(30);

    // теперь «пожинаем» зомби
    int status;
    waitpid(pid, &status, 0);
    printf("Reaped zombie %d, exit status=%d\n", pid, WEXITSTATUS(status));
    return 0;
}