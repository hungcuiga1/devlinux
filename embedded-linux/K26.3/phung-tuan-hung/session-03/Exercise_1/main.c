#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DATA_FILE "students.dat"

typedef struct {
    int   id;
    char  name[64];
    int   age;
    float gpa;
} Student;

/* Read exactly n bytes (handles partial reads / EINTR).
 * Return: n on success, 0 on EOF, -1 on error, -2 on truncated record */
static ssize_t read_full(int fd, void *buf, size_t n)
{
    size_t total = 0;
    while (total < n) {
        ssize_t r = read(fd, (char *)buf + total, n - total);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) break;              /* EOF */
        total += (size_t)r;
    }
    if (total == 0) return 0;
    if (total < n)  return -2;
    return (ssize_t)total;
}

/* Write exactly n bytes (handles partial writes / EINTR) */
static ssize_t write_full(int fd, const void *buf, size_t n)
{
    size_t total = 0;
    while (total < n) {
        ssize_t w = write(fd, (const char *)buf + total, n - total);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += (size_t)w;
    }
    return (ssize_t)total;
}

/* ---------- stdin helpers ---------- */
static int read_line(const char *prompt, char *buf, size_t size)
{
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buf, (int)size, stdin) == NULL) return -1;
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    } else {
        int c;                          /* drop rest of an over-long line */
        while ((c = getchar()) != '\n' && c != EOF) { }
    }
    return 0;
}

static int read_int(const char *prompt, int *out)
{
    char buf[64], *end;
    for (;;) {
        if (read_line(prompt, buf, sizeof(buf)) < 0) return -1;
        errno = 0;
        long v = strtol(buf, &end, 10);
        if (end != buf && *end == '\0' && errno == 0) { *out = (int)v; return 0; }
        printf("  Invalid integer, try again.\n");
    }
}

static int read_float(const char *prompt, float *out)
{
    char buf[64], *end;
    for (;;) {
        if (read_line(prompt, buf, sizeof(buf)) < 0) return -1;
        errno = 0;
        float v = strtof(buf, &end);
        if (end != buf && *end == '\0' && errno == 0) { *out = v; return 0; }
        printf("  Invalid number, try again.\n");
    }
}

static void print_student(const Student *s)
{
    printf("  ID: %-6d | Name: %-20s | Age: %-3d | GPA: %.2f\n",
           s->id, s->name, s->age, s->gpa);
}

/* ---------- menu actions ---------- */
static void add_student(int fd)
{
    Student s;
    memset(&s, 0, sizeof(s));           /* no garbage bytes in file */

    if (read_int("ID   : ", &s.id) < 0) return;
    if (read_line("Name : ", s.name, sizeof(s.name)) < 0) return;
    if (read_int("Age  : ", &s.age) < 0) return;
    if (read_float("GPA  : ", &s.gpa) < 0) return;

    if (lseek(fd, 0, SEEK_END) < 0) { perror("lseek"); return; }
    if (write_full(fd, &s, sizeof(s)) < 0) { perror("write"); return; }
    printf("=> Student added.\n");
}

static void list_students(int fd)
{
    Student s;
    ssize_t r;
    int count = 0;

    if (lseek(fd, 0, SEEK_SET) < 0) { perror("lseek"); return; }
    while ((r = read_full(fd, &s, sizeof(s))) == (ssize_t)sizeof(s)) {
        print_student(&s);
        count++;
    }
    if (r == -1) perror("read");
    if (r == -2) printf("  Warning: truncated record at end of file.\n");
    printf("=> Total: %d student(s).\n", count);
}

static void find_student(int fd)
{
    int id;
    Student s;
    ssize_t r;

    if (read_int("Enter ID to find: ", &id) < 0) return;
    if (lseek(fd, 0, SEEK_SET) < 0) { perror("lseek"); return; }

    while ((r = read_full(fd, &s, sizeof(s))) == (ssize_t)sizeof(s)) {
        if (s.id == id) {
            printf("=> Found:\n");
            print_student(&s);
            return;
        }
    }
    if (r == -1) perror("read");
    printf("=> Student with ID %d not found.\n", id);
}

int main(void)
{
    int fd = open(DATA_FILE, O_RDWR | O_CREAT, 0644);
    if (fd < 0) { perror("open " DATA_FILE); return EXIT_FAILURE; }

    for (;;) {
        int choice;
        printf("\n===== STUDENT MANAGER =====\n"
               "1. Add student\n"
               "2. List all students\n"
               "3. Find student by ID\n"
               "4. Exit\n");
        if (read_int("Choice: ", &choice) < 0) choice = 4;

        switch (choice) {
        case 1: add_student(fd);   break;
        case 2: list_students(fd); break;
        case 3: find_student(fd);  break;
        case 4:
            if (close(fd) < 0) perror("close");
            printf("Bye.\n");
            return EXIT_SUCCESS;
        default:
            printf("Invalid choice.\n");
        }
    }
}
