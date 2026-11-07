#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


#define SHW_ADR(ID, I) (printf("ID %s \t is at virtual address: %p\n", ID, (void*)&I))

extern int etext, edata, end;

char *cptr = "This message is output by the function showit()\n";
char buffer1[25];
int showit(char *p); // Современный прототип

int main(void) {
    int i = 0;

    printf("\nAddress etext: %p\n", (void*)&etext);
    printf("Address edata: %p\n", (void*)&edata);
    printf("Address end  : %p\n", (void*)&end);

    SHW_ADR("main", main);
    SHW_ADR("showit", showit);
    SHW_ADR("cptr", cptr);
    SHW_ADR("buffer1", buffer1);
    SHW_ADR("i", i);

    strcpy(buffer1, "A demonstration\n");
    write(1, buffer1, strlen(buffer1) + 1);
    
    showit(cptr);
    return 0;
}

int showit(char *p) {
    char *buffer2;
    SHW_ADR("buffer2", buffer2);
    
    if ((buffer2 = (char *)malloc(strlen(p) + 1)) != NULL) {
        printf("Allocated memory at %p\n", (void*)buffer2);
        strcpy(buffer2, p);
        printf("%s", buffer2);
        free(buffer2);
    } else {
        printf("Allocation error\n");
        exit(1);
    }
    return 0;
}