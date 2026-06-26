// ParThread.c - pthread version of file compression from question 5

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define THREADS 4
#define MIN_RUN 16

typedef struct { char c; long n; } Run;
typedef struct { char *data; long start, end; Run *runs; int used, cap; } Job;

static int bit(char c) { return c == '0' || c == '1'; }

static void add(Job *j, char c, long n) {
    if (n <= 0) return;
    if (j->used && j->runs[j->used - 1].c == c) {
        j->runs[j->used - 1].n += n;
        return;
    }
    if (j->used == j->cap) {
        j->cap = j->cap ? j->cap * 2 : 32;
        j->runs = realloc(j->runs, j->cap * sizeof(Run));
        if (!j->runs) { perror("realloc"); exit(1); }
    }
    j->runs[j->used].c = c;
    j->runs[j->used++].n = n;
}

static void *work(void *arg) {
    Job *j = arg;
    char cur = 0;
    long count = 0;
    for (long i = j->start; i < j->end; i++) {
        char c = j->data[i];
        if (count == 0) { cur = c; count = 1; }
        else if (c == cur) count++;
        else { add(j, cur, count); cur = c; count = 1; }
    }
    add(j, cur, count);
    return NULL;
}

static char *read_file(char *name, long *size) {
    FILE *f = fopen(name, "rb");
    if (!f) { perror("open input"); exit(1); }
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    rewind(f);
    char *data = malloc(*size ? *size : 1);
    if (!data) { perror("malloc"); exit(1); }
    if (fread(data, 1, *size, f) != (size_t)*size) { perror("read"); exit(1); }
    fclose(f);
    return data;
}

static void write_run(FILE *out, Run r) {
    if (bit(r.c) && r.n >= MIN_RUN)
        fprintf(out, "%c%ld%c", r.c == '1' ? '+' : '-', r.n, r.c == '1' ? '+' : '-');
    else
        for (long i = 0; i < r.n; i++) fputc(r.c, out);
}

int main(int argc, char *argv[]) {
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: %s <source> <dest> [threads]\n", argv[0]);
        return 1;
    }
    int n = argc == 4 ? atoi(argv[3]) : THREADS;
    if (n < 1) n = THREADS;

    long size;
    char *data = read_file(argv[1], &size);
    if (size < n) n = size ? (int)size : 1;
    pthread_t tid[n];
    Job jobs[n];
    long chunk = (size + n - 1) / n;

    for (int i = 0; i < n; i++) {
        jobs[i] = (Job){ data, i * chunk, (i + 1) * chunk, NULL, 0, 0 };
        if (jobs[i].end > size) jobs[i].end = size;
        if (pthread_create(&tid[i], NULL, work, &jobs[i])) { perror("pthread_create"); return 1; }
    }
    for (int i = 0; i < n; i++) pthread_join(tid[i], NULL);

    FILE *out = fopen(argv[2], "wb");
    if (!out) { perror("open output"); return 1; }
    Job all = {0};
    for (int i = 0; i < n; i++)
        for (int k = 0; k < jobs[i].used; k++) add(&all, jobs[i].runs[k].c, jobs[i].runs[k].n);
    for (int i = 0; i < all.used; i++) write_run(out, all.runs[i]);

    fclose(out);
    for (int i = 0; i < n; i++) free(jobs[i].runs);
    free(all.runs); free(data);
    return 0;
}
