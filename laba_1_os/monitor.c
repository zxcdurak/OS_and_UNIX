#define _DEFAULT_SOURCE  
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

struct event {
    int id;
    char* text;
};

struct event* create_event(int id){
    struct event* e = malloc(sizeof *e);
    if (!e) return NULL;
    e->id = id;
    e->text = malloc(32);
    if (!e->text) { free(e); return NULL; }
    snprintf(e->text, 32, "payload #%d", id);
    return e;
}

struct monitor {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    struct event* data;
    int ready;
};

struct monitor mon = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .cond  = PTHREAD_COND_INITIALIZER,
    .ready = 0,
    .data  = NULL,
};

int monitor_provide(struct monitor* m, struct event* e) {
    pthread_mutex_lock(&m->mutex);
    if (m->ready != 0) {
        printf("событие не отправлено\n");
        pthread_mutex_unlock(&m->mutex);
        return 0;
    }
    m->data = e;
    m->ready = 1;
    printf("отправлено событие %d\n", e->id);
    fflush(stdout);
    pthread_cond_signal(&m->cond);
    pthread_mutex_unlock(&m->mutex);
    return 1;
}

struct event* monitor_consume(struct monitor* m){
    pthread_mutex_lock(&m->mutex);
    while(m->ready == 0){
        pthread_cond_wait(&m->cond, &m->mutex);
    }
    struct event* e = m->data;
    m->data = NULL;
    m->ready = 0;
    printf("получено событие %d %s\n", e->id, e->text);
    fflush(stdout);
    pthread_mutex_unlock(&m->mutex);
    return e;
}

void event_destroy(struct event *e) {
    if (!e) return;
    free(e->text);
    free(e);
}

void* consumer(void* arg){
    struct monitor* mon = arg;
    for(;;){
        struct event* e = monitor_consume(mon);
        printf("обработка события %d %s\n", e->id, e->text);
        event_destroy(e);
    }
    return NULL;
}

void* provider(void* arg){
    struct monitor* mon = arg;
    for(int i = 0;; i++){
        sleep(1);
        struct event* e = create_event(i);
        if (!e) { perror("malloc"); continue; }
        int rc = monitor_provide(mon, e);
        if (rc == 0) {
            event_destroy(e);
        }
    }
    return NULL;
}

int main(void) {
    pthread_t tid_prov, tid_cons;
    int rc = pthread_create(&tid_prov, NULL, provider, &mon);
    if (rc != 0) {fprintf(stderr, "pthread_create: %s\n", strerror(rc)); return 1;}
    rc = pthread_create(&tid_cons, NULL, consumer, &mon);
    if (rc != 0) {fprintf(stderr, "pthread_create: %s\n", strerror(rc)); return 1;}
    pthread_join(tid_prov, NULL);
    pthread_join(tid_cons, NULL);
    return 0;
}