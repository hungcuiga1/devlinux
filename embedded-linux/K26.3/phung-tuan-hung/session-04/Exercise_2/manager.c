#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SEARCHER_PATH "./searcher"
#define DATA_FILE     "students.txt"
#define ID_MAX_LEN    64

extern char **environ;

/* Remove leading/trailing whitespace in place */
static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len  1] == '\n' || s[len  1] == '\r' || s[len  1] == ' '  || s[len  1] == '\t')) {
        s[--len] = '\0';
    }
    return s;
}

int main(void)
{
    char line[ID_MAX_LEN];

    printf("=============================================\n");
    printf("   STUDENT LOOKUP SYSTEM — MANAGER\n");
    printf("   (fork + execve | file: %s)\n", DATA_FILE);
    printf("[MANAGER] PID: %d\n", getpid());
    printf("Enter student ID ('quit' to exit).\n");

    for (;;) {
        printf("\n");
        printf("Student ID: ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {   /* EOF / Ctrl+D */
            printf("\n[MANAGER] Exiting. Goodbye!\n");
            break;
        }
        if (strchr(line, '\n') == NULL) {                 /* drop overlong input */
            int c;
            while ((c = getchar()) != '\n' && c != EOF) { }
        }

        char *id = trim(line);
        if (*id == '\0') continue;                        /* empty line */
        if (strcmp(id, "quit") == 0) {
            printf("[MANAGER] Exiting. Goodbye!\n");
            break;
        }

        /* Flush before fork so the child does not inherit buffered output */
        fflush(stdout);

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        }

        if (pid == 0) {                                   /*  child  */
            char *args[] = { SEARCHER_PATH, id, DATA_FILE, NULL };
            execve(SEARCHER_PATH, args, environ);

            /* This line is normally NEVER reached: on success execve() replaces
             * the whole process image (code, data, stack) with ./searcher, so
             * there is no "return" back to this code. We only get here if
             * execve() FAILED (e.g. ./searcher missing or not executable). */
            perror("execve failed");
            exit(2);
        }

        /*  parent  */
        printf("[MANAGER] fork() → child PID: %d\n", pid);
        printf("[MANAGER] Waiting for child (waitpid)...\n");
        fflush(stdout);

        int status;
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
            continue;
        }

        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            const char *msg;
            switch (code) {
            case 0:  msg = "Found";                    break;
            case 1:  msg = "Not found";                break;
            case 2:  msg = "Error (file/argument)";    break;
            default: msg = "Unknown exit code";        break;
            }
            printf("[MANAGER] Child (PID %d) exited. code=%d → %s\n", pid, code, msg);
        } else if (WIFSIGNALED(status)) {
            printf("[MANAGER] Child (PID %d) killed by signal %d\n",
                   pid, WTERMSIG(status));
        }
    }

    return EXIT_SUCCESS;
}
