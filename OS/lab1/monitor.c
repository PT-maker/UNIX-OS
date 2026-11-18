// Сборка: gcc -std=c11 -Wall -Wextra -O2 -pthread monitor.c -o monitor

#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    int    id;
    double payload;
    char  *dynamic_text;
} Event;

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t  can_put;
    pthread_cond_t  can_get;
    Event          *slot;
    bool            closed;
} Monitor;

static void monitor_init(Monitor *m)
{
    pthread_mutex_init(&m->lock, NULL);
    pthread_cond_init(&m->can_put, NULL);
    pthread_cond_init(&m->can_get, NULL);
    m->slot   = NULL;
    m->closed = false;
}

static void monitor_destroy(Monitor *m)
{
    pthread_cond_destroy(&m->can_put);
    pthread_cond_destroy(&m->can_get);
    pthread_mutex_destroy(&m->lock);
}

static void monitor_put(Monitor *m, Event *ev)
{
    pthread_mutex_lock(&m->lock);
    while (m->slot != NULL)
        pthread_cond_wait(&m->can_put, &m->lock);

    m->slot = ev;
    printf("[Поставщик]  событие #%d отправлено\n", ev->id);
    fflush(stdout);

    pthread_cond_signal(&m->can_get);
    pthread_mutex_unlock(&m->lock);
}

static void monitor_close(Monitor *m)
{
    pthread_mutex_lock(&m->lock);
    m->closed = true;
    pthread_cond_broadcast(&m->can_get);
    pthread_mutex_unlock(&m->lock);
}

static Event *monitor_get(Monitor *m)
{
    pthread_mutex_lock(&m->lock);
    while (m->slot == NULL && !m->closed)
        pthread_cond_wait(&m->can_get, &m->lock);

    Event *ev = m->slot;
    if (ev != NULL) {
        m->slot = NULL;
        printf("[Потребитель] событие #%d получено\n", ev->id);
        fflush(stdout);
        pthread_cond_signal(&m->can_put);
    }
    pthread_mutex_unlock(&m->lock);
    return ev;
}

#define EVENT_COUNT 5

static void *producer(void *arg)
{
    Monitor *m = arg;
    for (int i = 1; i <= EVENT_COUNT; ++i) {
        struct timespec d = { .tv_sec = 1, .tv_nsec = 0 };
        while (nanosleep(&d, &d) == -1)
            ;

        Event *ev = malloc(sizeof *ev);
        if (!ev) { perror("malloc"); break; }
        ev->id = i;
        ev->payload = i * 1.5;
        ev->dynamic_text = strdup("несериализуемые данные");

        monitor_put(m, ev);
    }
    monitor_close(m);
    return NULL;
}

static void *consumer(void *arg)
{
    Monitor *m = arg;
    Event *ev;
    while ((ev = monitor_get(m)) != NULL) {
        (void)ev->payload;
        free(ev->dynamic_text);
        free(ev);
    }
    return NULL;
}

int main(void)
{
    Monitor m;
    monitor_init(&m);

    pthread_t prod, cons;
    pthread_create(&cons, NULL, consumer, &m);
    pthread_create(&prod, NULL, producer, &m);

    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    monitor_destroy(&m);
    return 0;
}