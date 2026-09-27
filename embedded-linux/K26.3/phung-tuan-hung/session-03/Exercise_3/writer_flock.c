#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>   /* flock */

#define LOG_FILE "system.log"
#define LINE_MAX_LEN 1024

/* Build: [PID:12345] [2025-05-21 14:02:33] [INFO] message\n */
static int format_line(char *buf, size_t size, const char *msg)
{
    time_t now = time(NULL);
    struct tm tm_now;
    char ts[32];

    if (localtime_r(&now, &tm_now) == NULL) return -1;
    if (strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_now) == 0) return -1;

    int n = snprintf(buf, size, "[PID:%d] [%s] [INFO] %s\n", (int)getpid(), ts, msg);
    if (n < 0) return -1;
    if ((size_t)n >= size) {            /* truncated: keep trailing newline */
        buf[size - 2] = '\n';
        buf[size - 1] = '\0';
        n = (int)size - 1;
    }
    return n;
}

static ssize_t write_full(int fd, const void *buf, size_t n)
{
    size_t total = 0;
    while (total < n) {
        ssize_t w = write(fd, (const char *)buf + total, n - total);
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        total += (size_t)w;
    }
    return (ssize_t)total;
}

int main(int argc, char *argv[])
{
    char line[LINE_MAX_LEN];
    int  len, ret = EXIT_SUCCESS;

    if (argc != 2) {
        dprintf(STDERR_FILENO, "Usage: %s \"message text\"\n", argv[0]);
        return EXIT_FAILURE;
    }

    int fd = open(LOG_FILE, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0) { perror("open"); return EXIT_FAILURE; }

    /* 1. Acquire exclusive lock (blocks until available) */
    while (flock(fd, LOCK_EX) < 0) {
        if (errno == EINTR) continue;
        perror("flock LOCK_EX");
        close(fd);
        return EXIT_FAILURE;
    }

    /* 2. Format and write exactly one line */
    len = format_line(line, sizeof(line), argv[1]);
    if (len < 0 || write_full(fd, line, (size_t)len) < 0) {
        perror("write");
        ret = EXIT_FAILURE;
    }

    /* 3. Release lock */
    if (flock(fd, LOCK_UN) < 0) { perror("flock LOCK_UN"); ret = EXIT_FAILURE; }

    /* 4. Close file */
    if (close(fd) < 0) { perror("close"); ret = EXIT_FAILURE; }
    return ret;
}
