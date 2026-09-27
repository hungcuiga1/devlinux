#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define NUM_ORDERS 3

typedef struct {
    int   id;
    char  name[50];
    int   quantity;
    float unit_price;
} Order;

void process_order(Order o)
{
    float total = o.quantity * o.unit_price;
    printf("[CHILD-%d] PID: %d | PPID: %d\n", o.id, getpid(), getppid());
    printf("[CHILD-%d] %s x%d — Total: %.0f VND\n",
           o.id, o.name, o.quantity, total);
    printf("[CHILD-%d] Processing... (sleep 2s)\n\n", o.id);
    fflush(stdout);
    sleep(2);
}

/* Format an integer amount with thousands separators: 1560000 -> "1,560,000" */
static void format_vnd(long long amount, char *buf, size_t size)
{
    char raw[32];
    int  len = snprintf(raw, sizeof(raw), "%lld", amount < 0 ? -amount : amount);
    size_t j = 0;

    if (amount < 0 && j < size - 1) buf[j++] = '-';
    for (int i = 0; i < len && j < size - 1; i++) {
        if (i > 0 && (len - i) % 3 == 0 && j < size - 1) buf[j++] = ',';
        if (j < size - 1) buf[j++] = raw[i];
    }
    buf[j] = '\0';
}

int main(void)
{
    Order orders[NUM_ORDERS] = {
        {1, "Backpack", 2, 350000},
        {2, "Shoes",    1, 500000},
        {3, "Hat",      3, 120000}
    };
    pid_t pids[NUM_ORDERS];
    int   success = 0, failed = 0;
    long long revenue = 0;

    printf("===================================================\n");
    printf("   ORDER PROCESSING SYSTEM — MANAGER (fork+wait)\n");
    printf("===================================================\n");
    printf("[MANAGER] PID: %d — spawning %d child processes...\n",
           getpid(), NUM_ORDERS);

    /*  Loop 1: spawn all children  */
    for (int i = 0; i < NUM_ORDERS; i++) {
        /* Flush BEFORE fork: otherwise the child inherits the parent's
         * unflushed stdio buffer and prints it a second time. */
        fflush(stdout);

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            pids[i] = -1;                   /* mark as failed */
            continue;
        }
        if (pid == 0) {                     /*  child  */
            process_order(orders[i]);
            exit(0);
        }
        pids[i] = pid;                      /*  parent  */
        printf("[MANAGER] fork() order #%d → child PID: %d\n",
               orders[i].id, pid);
    }

    printf("[MANAGER] All %d children spawned. Starting waitpid()...\n\n",
           NUM_ORDERS);
    fflush(stdout);

    /*  Loop 2: wait for each child by PID  */
    for (int i = 0; i < NUM_ORDERS; i++) {
        int status;

        if (pids[i] < 0) {                  /* fork failed earlier */
            printf("[MANAGER] order #%d: fork failed → FAILED\n", orders[i].id);
            failed++;
            continue;
        }
        if (waitpid(pids[i], &status, 0) < 0) {
            perror("waitpid");
            failed++;
            continue;
        }

        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            printf("[MANAGER] waitpid(%d) — order #%d: exit code=%d → %s\n",
                   pids[i], orders[i].id, code, code == 0 ? "SUCCESS" : "FAILED");
            if (code == 0) {
                success++;
                revenue += (long long)(orders[i].quantity * orders[i].unit_price);
            } else {
                failed++;
            }
        } else if (WIFSIGNALED(status)) {
            printf("[MANAGER] waitpid(%d) — order #%d: killed by signal %d → FAILED\n",
                   pids[i], orders[i].id, WTERMSIG(status));
            failed++;
        } else {
            failed++;
        }
    }

    char rev_str[32];
    format_vnd(revenue, rev_str, sizeof(rev_str));

    printf("\n================= SUMMARY =================\n");
    printf("  Total orders    : %d\n", NUM_ORDERS);
    printf("  Successful      : %d\n", success);
    printf("  Failed          : %d\n", failed);
    printf("  Total revenue   : %s VND\n", rev_str);

    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
