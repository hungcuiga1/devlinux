/*
 * Exercise_1 - Bus ticket booking system (5 agents / 5 threads, 10 seats)
 *
 * ============================================================================
 * WHY THE CHECK AND THE DEDUCT MUST BE IN THE SAME LOCK/UNLOCK BLOCK
 * ============================================================================
 * "Check seats_available >= wanted" and "seats_available -= wanted" together
 * form ONE logical operation (check-then-act). If they are protected by two
 * separate lock acquisitions:
 *
 *     lock();   ok = (seats_available >= wanted);   unlock();   // (1) check
 *                     <-- another thread can run here -->
 *     lock();   if (ok) seats_available -= wanted;  unlock();   // (2) deduct
 *
 * then between (1) and (2) other agents can also pass the check using the
 * SAME old value. Example with 3 seats left:
 *     Agent A checks: 3 >= 2 -> OK      (unlock)
 *     Agent B checks: 3 >= 2 -> OK      (unlock)
 *     Agent A deducts: 3 - 2 = 1
 *     Agent B deducts: 1 - 2 = -1       -> OVERBOOKING, negative seats!
 *
 * Each individual access is "thread-safe", but the decision made in (1) is
 * already stale when (2) runs -> this is a TOCTOU (time-of-check to
 * time-of-use) race condition. Only holding the mutex across BOTH steps makes
 * check + deduct atomic: no other thread can change seats_available between
 * the moment we decide and the moment we act.
 * ============================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define NUM_AGENTS  5
#define TOTAL_SEATS 10

typedef struct {
    int  agent_id;
    char customer[50];
    int  seats_wanted;
} BookingRequest;

int seats_available = TOTAL_SEATS;
int failed_bookings = 0;            /* also protected by seat_lock */
pthread_mutex_t seat_lock;

/* "seat " / "seats" so columns line up like the expected output */
static const char *seat_word(int n) { return n == 1 ? "seat " : "seats"; }

void *book_ticket(void *arg)
{
    BookingRequest *req = (BookingRequest *)arg;

    /* pthread_t is opaque; cast to unsigned long for printing (Linux) */
    printf("[Agent %d | TID %lu] Booking %d %s for %s...\n",
           req->agent_id, (unsigned long)pthread_self(),
           req->seats_wanted, seat_word(req->seats_wanted), req->customer);

    sleep(1);                               /* force real concurrency */

    /* ---------- critical section: check + deduct are ATOMIC ---------- */
    pthread_mutex_lock(&seat_lock);
    if (seats_available >= req->seats_wanted) {
        seats_available -= req->seats_wanted;
        printf("[Agent %d] CONFIRMED: %d %s for %s. Remaining: %d\n",
               req->agent_id, req->seats_wanted, seat_word(req->seats_wanted),
               req->customer, seats_available);
    } else {
        failed_bookings++;
        printf("[Agent %d] SOLD OUT:  needs %d %s, only %d left — booking failed.\n",
               req->agent_id, req->seats_wanted,
               req->seats_wanted == 1 ? "seat" : "seats", seats_available);
    }
    pthread_mutex_unlock(&seat_lock);
    /* ------------------------------------------------------------------ */

    return NULL;
}

int main(void)
{
    BookingRequest requests[NUM_AGENTS] = {
        {1, "Nguyen Van An",  2},
        {2, "Tran Thi Bich",  1},
        {3, "Le Van Cuong",   3},
        {4, "Pham Thi Dung",  1},
        {5, "Hoang Van Em",   2}
    };
    pthread_t tids[NUM_AGENTS];
    int created[NUM_AGENTS] = {0};

    printf("==============================================\n");
    printf("   TICKET BOOKING SYSTEM (%d agents, %d seats)\n", NUM_AGENTS, TOTAL_SEATS);
    printf("==============================================\n");

    if (pthread_mutex_init(&seat_lock, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < NUM_AGENTS; i++) {
        int rc = pthread_create(&tids[i], NULL, book_ticket, &requests[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create(agent %d): %s\n",
                    requests[i].agent_id, strerror(rc));
            failed_bookings++;              /* no other thread touches it yet for this request */
            continue;
        }
        created[i] = 1;
    }

    for (int i = 0; i < NUM_AGENTS; i++) {
        if (created[i]) pthread_join(tids[i], NULL);
    }

    printf("================ SUMMARY ================\n");
    printf("  Total seats     : %d\n", TOTAL_SEATS);
    printf("  Seats sold      : %d\n", TOTAL_SEATS - seats_available);
    printf("  Seats remaining : %d\n", seats_available);
    printf("  Failed bookings : %d\n", failed_bookings);
    printf("=========================================\n");

    pthread_mutex_destroy(&seat_lock);
    return EXIT_SUCCESS;
}
