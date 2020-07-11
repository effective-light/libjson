#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "json.h"

void print_diff(char *msg, struct timespec start, struct timespec end) {
    printf("%s: %Lf ms\n", msg, ((end.tv_sec - start.tv_sec) * (long) 1e9
                + (end.tv_nsec - start.tv_nsec)) * (long double) 1e-6);
}

int main() {
    char c;
    size_t size = 0;
    size_t capacity = 1024;

    char *json = safe_malloc(capacity * sizeof(char));
    while ((c = getchar()) != EOF) {
        if (capacity < size + 1) {
            capacity *= 2;
            json = realloc(json, capacity * sizeof(char));
        }
        json[size] = c;
        size++;
    }
    json[size] = '\0';

    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    json_entry_t *ent = json_parse(json, size - 1);
    clock_gettime(CLOCK_MONOTONIC, &end);

    if (ent) {
        print_diff("parse", start, end);
        clock_gettime(CLOCK_MONOTONIC, &start);
        char *json_out = json_stringify(ent);
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
