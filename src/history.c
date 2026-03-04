#define _GNU_SOURCE

#include "../inc/history.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char **g_entries = NULL;  // массив строк истории
static int g_size = 0;   // число записей
static int g_cap = 0;   // размер
static char g_history_path[PATH_MAX]; 



// есть ли свободное место в массиве?
void history_ensure_capacity() {
    if (g_cap == 0) {
        g_cap = 64;
        g_entries = malloc(g_cap * sizeof(char *));
    } else if (g_size >= g_cap) {
        size_t new_cap = g_cap * 2;
        char **tmp = realloc(g_entries, new_cap * sizeof(char *));
        if (!tmp) return;
        g_cap = new_cap;
        g_entries = tmp;
    }
}

static void history_set_path() {
    const char *home = getenv("HOME");
    snprintf(g_history_path, sizeof(g_history_path), "%s/%s", home, ".my_shell_history");
   
}


// убираем стартовые и хвостовые пробелы
static char *trim_copy(const char *line) {
    if (!line) return NULL;

    size_t start = 0;
    size_t end = strlen(line);

    while (line[start] && isspace(line[start])) start++;
    while (end > start && isspace(line[end - 1])) end--;

    size_t len = end > start ? end - start : 0;
    char *out = malloc(len + 1);
    if (!out) return NULL;

    memcpy(out, line + start, len);
    out[len] = '\0';

    return out;
}

// преобразование в однострочную запись 
void sanitize_entry(char *entry) {
    if (!entry) return;
    for (int i = 0; entry[i]; ++i) {
        if (entry[i] == '\n' || entry[i] == '\r') {
            entry[i] = ' ';
        }
    }
}

void history_push(char *entry, int append_to_file) {
    if (!entry) return;

    history_ensure_capacity();
    if (!g_entries) {
        free(entry);
        return;
    }

    g_entries[g_size++] = entry;

    if (append_to_file && g_history_path[0]) {
        FILE *f = fopen(g_history_path, "a");
        if (f) {
            fprintf(f, "%s\n", entry);
            fclose(f);
        }
    }
}

void history_init(void) {
    history_set_path();

    FILE *f = fopen(g_history_path, "r");
    if (!f) return;

    char *line = NULL;
    size_t n = 0;
    while (getline(&line, &n, f) != -1) {
        char *trimmed = trim_copy(line);
        if (!trimmed || trimmed[0] == '\0') {
            free(trimmed);
            continue;
        }
        sanitize_entry(trimmed);
        history_push(trimmed, 0);
    }

    free(line);
    fclose(f);
}

void history_shutdown(void) {
    for (int i = 0; i < g_size; ++i) {
        free(g_entries[i]);
    }
    free(g_entries);
    g_entries = NULL;
    g_size = 0;
    g_cap = 0;
}

// обертка над пуш с преобразованием
void history_add(const char *line) {
    if (!line) return;

    char *trimmed = trim_copy(line);
    if (!trimmed || trimmed[0] == '\0') {
        free(trimmed);
        return;
    }

    sanitize_entry(trimmed);

    if (g_size > 0 && strcmp(g_entries[g_size - 1], trimmed) == 0) {
        free(trimmed);
        return;
    }

    history_push(trimmed, 1);
}

// получить запись по индексу
const char *history_get(int index) {
    if (index >= g_size) return NULL;
    return g_entries[index];
}

// получить запись с конца
const char *history_get_from_tail(int offset) {
    if (offset >= g_size) return NULL;
    return g_entries[g_size - 1 - offset];
}

int history_size() {
    return g_size;
}
