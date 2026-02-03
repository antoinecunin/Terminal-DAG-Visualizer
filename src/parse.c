#include "drawdag.h"

#include <string.h>

int read_edges(FILE *fp, RawEdge *edges, int max) {
    int n = 0;
    char line[256];
    while (fgets(line, sizeof line, fp) && n < max) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        if (sscanf(p, "%63s %63s", edges[n].src, edges[n].dst) == 2) n++;
    }
    return n;
}

int default_edges(RawEdge *e) {
    static const char *d[][2] = {
        {"init","parse"},     {"init","config"},
        {"fetch","transform"},{"parse","fetch"},
        {"parse","validate"}, {"parse","build"},
        {"config","lint"},    {"config","transform"},
        {"config","build"},   {"config","deploy"},
        {"transform","bundle"},{"validate","bundle"},
        {"validate","test"},  {"build","validate"},
        {"deploy","test"},    {"bundle","publish"},
        {"test","publish"},
    };
    int n = sizeof d / sizeof d[0];
    for (int i = 0; i < n; i++) {
        snprintf(e[i].src, MAX_NAME, "%s", d[i][0]);
        snprintf(e[i].dst, MAX_NAME, "%s", d[i][1]);
    }
    return n;
}
