#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DATA_FILE "products.dat"

typedef struct {
    int    id;
    char   name[64];
    int    quantity;
    double price;
} Product;

static ssize_t read_full(int fd, void *buf, size_t n)
{
    size_t total = 0;
    while (total < n) {
        ssize_t r = read(fd, (char *)buf + total, n - total);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) break;
        total += (size_t)r;
    }
    if (total == 0) return 0;
    if (total < n)  return -2;
    return (ssize_t)total;
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

/* ---------- stdin helpers ---------- */
static int read_line(const char *prompt, char *buf, size_t size)
{
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buf, (int)size, stdin) == NULL) return -1;
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
    else { int c; while ((c = getchar()) != '\n' && c != EOF) { } }
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

static int read_double(const char *prompt, double *out)
{
    char buf[64], *end;
    for (;;) {
        if (read_line(prompt, buf, sizeof(buf)) < 0) return -1;
        errno = 0;
        double v = strtod(buf, &end);
        if (end != buf && *end == '\0' && errno == 0) { *out = v; return 0; }
        printf("  Invalid number, try again.\n");
    }
}

/* Number of records = file size / sizeof(Product) (via lseek, no loading) */
static long record_count(int fd)
{
    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0) { perror("lseek"); return -1; }
    return (long)(size / (off_t)sizeof(Product));
}

static void print_product(long index, const Product *p)
{
    printf("  [%ld] ID: %-6d | Name: %-20s | Qty: %-6d | Price: %.2f\n",
           index, p->id, p->name, p->quantity, p->price);
}

/* Ask for an index and validate it against the current record count */
static int ask_index(int fd, long *index)
{
    int idx;
    long count = record_count(fd);
    if (count < 0) return -1;
    if (count == 0) { printf("=> File is empty.\n"); return -1; }
    if (read_int("Index: ", &idx) < 0) return -1;
    if (idx < 0 || idx >= count) {
        printf("=> Index out of range (0..%ld).\n", count - 1);
        return -1;
    }
    *index = idx;
    return 0;
}

/* ---------- menu actions ---------- */
static void add_product(int fd)
{
    Product p;
    memset(&p, 0, sizeof(p));           /* zero padding bytes too */

    if (read_int("ID       : ", &p.id) < 0) return;
    if (read_line("Name     : ", p.name, sizeof(p.name)) < 0) return;
    if (read_int("Quantity : ", &p.quantity) < 0) return;
    if (read_double("Price    : ", &p.price) < 0) return;

    off_t end = lseek(fd, 0, SEEK_END);
    if (end < 0) { perror("lseek"); return; }
    if (write_full(fd, &p, sizeof(p)) < 0) { perror("write"); return; }
    printf("=> Product added at index %ld.\n", (long)(end / (off_t)sizeof(Product)));
}

static void show_by_index(int fd)
{
    long index;
    Product p;
    if (ask_index(fd, &index) < 0) return;

    off_t offset = (off_t)index * (off_t)sizeof(Product);
    if (lseek(fd, offset, SEEK_SET) < 0) { perror("lseek"); return; }
    if (read_full(fd, &p, sizeof(p)) != (ssize_t)sizeof(p)) {
        printf("=> Failed to read record.\n");
        return;
    }
    print_product(index, &p);
}

static void update_quantity(int fd)
{
    long index;
    int qty;
    if (ask_index(fd, &index) < 0) return;
    if (read_int("New quantity: ", &qty) < 0) return;

    off_t offset       = (off_t)index * (off_t)sizeof(Product);
    off_t field_offset = offset + (off_t)offsetof(Product, quantity);

    /* Write ONLY the quantity field, not the whole record */
    if (lseek(fd, field_offset, SEEK_SET) < 0) { perror("lseek"); return; }
    if (write_full(fd, &qty, sizeof(qty)) < 0) { perror("write"); return; }
    printf("=> Quantity of index %ld updated to %d (offset %ld, %zu bytes).\n",
           index, qty, (long)field_offset, sizeof(qty));
}

static void list_all(int fd)
{
    Product p;
    ssize_t r;
    long index = 0;

    if (lseek(fd, 0, SEEK_SET) < 0) { perror("lseek"); return; }
    /* Sequential: one record in memory at a time */
    while ((r = read_full(fd, &p, sizeof(p))) == (ssize_t)sizeof(p)) {
        print_product(index++, &p);
    }
    if (r == -1) perror("read");
    if (r == -2) printf("  Warning: truncated record at end of file.\n");
    printf("=> Total: %ld product(s).\n", index);
}

int main(void)
{
    int fd = open(DATA_FILE, O_RDWR | O_CREAT, 0644);
    if (fd < 0) { perror("open " DATA_FILE); return EXIT_FAILURE; }

    for (;;) {
        int choice;
        printf("\n===== PRODUCT MANAGER =====\n"
               "1. Add product\n"
               "2. Show product by index\n"
               "3. Update quantity by index\n"
               "4. List all products\n"
               "5. Exit\n");
        if (read_int("Choice: ", &choice) < 0) choice = 5;

        switch (choice) {
        case 1: add_product(fd);     break;
        case 2: show_by_index(fd);   break;
        case 3: update_quantity(fd); break;
        case 4: list_all(fd);        break;
        case 5:
            if (close(fd) < 0) perror("close");
            printf("Bye.\n");
            return EXIT_SUCCESS;
        default:
            printf("Invalid choice.\n");
        }
    }
}
