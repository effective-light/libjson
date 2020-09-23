#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "libjson.h"

static void print_diff(char *msg, struct timespec start, struct timespec end) {
    printf("%s: %Lf ms\n", msg, ((end.tv_sec - start.tv_sec) * (long) 1e9
                + (end.tv_nsec - start.tv_nsec)) * (long double) 1e-6);
}

int main() {
    size_t size = 0;
    size_t capacity = 1024;
    ssize_t ret;

    char *json = safe_malloc(capacity * sizeof(char));
    while ((ret = read(STDIN_FILENO, (json + size), capacity - size)) != 0) {
        if (ret == -1) {
            free(json);
            perror("read");
            return 1;
        }

        size += ret;
        if (capacity == size) {
            json = realloc(json, (capacity *= 2) * sizeof(char));
        }
    }
    json[size] = '\0';

    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    json_entry_t *ent = json_parse(json);
    clock_gettime(CLOCK_MONOTONIC, &end);

    if (ent) {
        print_diff("parse", start, end);
        size_t n;
        clock_gettime(CLOCK_MONOTONIC, &start);
        char *json_out = json_stringify(ent, &n);
        clock_gettime(CLOCK_MONOTONIC, &end);
        print_diff("stringify", start, end);
        printf("%s\n", json_out);

        clock_gettime(CLOCK_MONOTONIC, &start);
        json_destroy(ent);
        clock_gettime(CLOCK_MONOTONIC, &end);
        print_diff("destroy", start, end);
        free(json_out);
    }

    free(json);

    return 0;
}
