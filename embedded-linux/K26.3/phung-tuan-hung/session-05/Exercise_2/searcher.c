#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define LINE_MAX_LEN 256

static const char *classify(double gpa)
{
    if (gpa >= 8.5) return "Excellent";
    if (gpa >= 7.0) return "Good";
    if (gpa >= 5.0) return "Average";
    return "Poor";
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        errno = EINVAL;
        perror("searcher: usage ./searcher <student_id> <data_file>");
        exit(2);
    }

    const char *target = argv[1];
    const char *path   = argv[2];

    printf("[SEARCHER] PID: %d | PPID: %d\n", getpid(), getppid());
    printf("[SEARCHER] Searching for \"%s\" in %s...\n", target, path);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        perror("searcher: fopen");
        exit(2);
    }

    char line[LINE_MAX_LEN];
    while (fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';           /* strip newline */
        if (line[0] == '\0') continue;                /* skip empty lines */

        char *id    = strtok(line, "|");
        char *name  = strtok(NULL, "|");
        char *cls   = strtok(NULL, "|");
        char *gpa_s = strtok(NULL, "|");
        if (id == NULL || name == NULL || cls == NULL || gpa_s == NULL) {
            continue;                                 /* malformed line */
        }

        if (strcmp(id, target) == 0) {
            double gpa = strtod(gpa_s, NULL);
            printf("========== SEARCH RESULT ==========\n");
            printf("  ID      : %s\n", id);
            printf("  Name    : %s\n", name);
            printf("  Class   : %s\n", cls);
            printf("  GPA     : %.1f\n", gpa);
            printf("  Grade   : %s\n", classify(gpa));
            fclose(fp);
            exit(0);
        }
    }

    if (ferror(fp)) {
        perror("searcher: read");
        fclose(fp);
        exit(2);
    }

    fclose(fp);
    printf("[SEARCHER] No student found with ID: %s\n", target);
    exit(1);
}
