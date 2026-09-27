#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define QUEUE_CAP     5
#define NUM_PRODUCERS 3
#define DOCS_PER_PROD 3

typedef struct {
    int  doc_id;
    char filename[60];
    int  pages;
} Document;

Document queue[QUEUE_CAP];
int head = 0, tail = 0, count = 0;
int all_sent = 0;           /* set to 1 by main after joining all producers */
pthread_mutex_t q_lock;
pthread_cond_t  not_full;   /* producers wait here when count == 5 */
pthread_cond_t  not_empty;  /* printer  waits here when count == 0 */

/* Statistics (protected by q_lock) */
int docs_submitted = 0;
int docs_printed   = 0;
int pages_printed  = 0;

/* Documents of each producer: 9 documents, 66 pages in total */
static const Document jobs[NUM_PRODUCERS][DOCS_PER_PROD] = {
    { {1, "report_Q1.pdf", 12}, {4, "slides.pdf",   20}, {7, "summary.pdf",  4} },
    { {2, "contract.pdf",   5}, {5, "memo.pdf",      2}, {8, "budget.pdf",   7} },
    { {3, "invoice.pdf",    3}, {6, "proposal.pdf",  8}, {9, "schedule.pdf", 5} },
};

/* Caller must hold q_lock and guarantee count < QUEUE_CAP */
static void enqueue(const Document *d)
{
    queue[tail] = *d;
    tail = (tail + 1) % QUEUE_CAP;
    count++;
}

/* Caller must hold q_lock and guarantee count > 0 */
static Document dequeue(void)
{
    Document d = queue[head];
    head = (head + 1) % QUEUE_CAP;
    count--;
    return d;
}

void *producer(void *arg)
{
    int id = *(int *)arg;                       /* 1..NUM_PRODUCERS */

    for (int i = 0; i < DOCS_PER_PROD; i++) {
        const Document *doc = &jobs[id - 1][i];

        pthread_mutex_lock(&q_lock);
        if (count == QUEUE_CAP) {
            printf("[Producer %d] Queue full — waiting...\n", id);
        }
        while (count == QUEUE_CAP) {            /* while, NOT if (see top) */
            pthread_cond_wait(&not_full, &q_lock);
        }
        enqueue(doc);
        docs_submitted++;
        printf("[Producer %d] Submitting: %-13s (%d pages)%s — queue: %d/%d\n",
               id, doc->filename, doc->pages, doc->pages < 10 ? " " : "",
               count, QUEUE_CAP);
        pthread_cond_signal(&not_empty);        /* wake the printer */
        pthread_mutex_unlock(&q_lock);

        usleep(100 * 1000);                     /* small gap between submissions */
    }
    return NULL;
}

void *printer(void *arg)
{
    (void)arg;

    for (;;) {
        pthread_mutex_lock(&q_lock);
        while (count == 0 && !all_sent) {       /* while, NOT if (see top) */
            pthread_cond_wait(&not_empty, &q_lock);
        }
        if (count == 0 && all_sent) {           /* nothing left and no more coming */
            pthread_mutex_unlock(&q_lock);
            break;
        }
        Document doc = dequeue();
        docs_printed++;
        pages_printed += doc.pages;
        printf("[Printer]    Printing:   %-13s (%d pages)%s — queue: %d/%d\n",
               doc.filename, doc.pages, doc.pages < 10 ? " " : "",
               count, QUEUE_CAP);
        pthread_cond_signal(&not_full);         /* wake a waiting producer */
        pthread_mutex_unlock(&q_lock);

        sleep(1);                               /* simulate printing (outside lock) */
    }

    printf("[Printer]    All documents printed. Exiting.\n");
    return NULL;
}

int main(void)
{
    pthread_t prod_tids[NUM_PRODUCERS], printer_tid;
    int prod_ids[NUM_PRODUCERS];

    printf("==============================================\n");
    printf("   OFFICE PRINT QUEUE (%d producers, 1 printer)\n", NUM_PRODUCERS);
    printf("   Queue capacity: %d documents\n", QUEUE_CAP);

    pthread_mutex_init(&q_lock, NULL);
    pthread_cond_init(&not_full, NULL);
    pthread_cond_init(&not_empty, NULL);

    if (pthread_create(&printer_tid, NULL, printer, NULL) != 0) {
        fprintf(stderr, "pthread_create(printer) failed\n");
        return EXIT_FAILURE;
    }
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        prod_ids[i] = i + 1;
        if (pthread_create(&prod_tids[i], NULL, producer, &prod_ids[i]) != 0) {
            fprintf(stderr, "pthread_create(producer %d) failed\n", i + 1);
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(prod_tids[i], NULL);
    }

    /* All producers finished: tell the printer no more documents will come */
    pthread_mutex_lock(&q_lock);
    all_sent = 1;
    pthread_cond_broadcast(&not_empty);
    pthread_mutex_unlock(&q_lock);

    pthread_join(printer_tid, NULL);

    printf("================ SUMMARY ================\n");
    printf("  Documents submitted : %d\n", docs_submitted);
    printf("  Documents printed   : %d\n", docs_printed);
    printf("  Total pages printed : %d\n", pages_printed);

    pthread_cond_destroy(&not_empty);
    pthread_cond_destroy(&not_full);
    pthread_mutex_destroy(&q_lock);
    return EXIT_SUCCESS;
}
