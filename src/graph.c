#include "drawdag.h"

#include <string.h>

/* ---- helpers ---- */

static bool has_adj(const int *arr, int count, int val) {
    for (int i = 0; i < count; i++) if (arr[i] == val) return true;
    return false;
}

static void rm_adj(int *arr, int *count, int val) {
    for (int i = 0; i < *count; i++)
        if (arr[i] == val) { arr[i] = arr[--(*count)]; return; }
}

/* ---- public API ---- */

void graph_init(Graph *g) { memset(g, 0, sizeof *g); }

int graph_find(const Graph *g, const char *name) {
    for (int i = 0; i < g->count; i++)
        if (g->nodes[i].active && strcmp(g->nodes[i].name, name) == 0)
            return i;
    return -1;
}

int graph_add(Graph *g, const char *name) {
    if (g->count >= MAX_NODES) return -1;
    int idx = g->count++;
    Node *node = &g->nodes[idx];
    memset(node, 0, sizeof *node);
    snprintf(node->name, MAX_NAME, "%s", name);
    node->active = true;
    return idx;
}

int graph_find_or_add(Graph *g, const char *name) {
    int idx = graph_find(g, name);
    return idx >= 0 ? idx : graph_add(g, name);
}

void graph_add_edge(Graph *g, int src, int dst) {
    Node *s = &g->nodes[src], *d = &g->nodes[dst];
    if (!has_adj(s->adj_out, s->out_count, dst) && s->out_count < MAX_ADJ)
        s->adj_out[s->out_count++] = dst;
    if (!has_adj(d->adj_in, d->in_count, src) && d->in_count < MAX_ADJ)
        d->adj_in[d->in_count++] = src;
}

void graph_remove_edge(Graph *g, int src, int dst) {
    rm_adj(g->nodes[src].adj_out, &g->nodes[src].out_count, dst);
    rm_adj(g->nodes[dst].adj_in,  &g->nodes[dst].in_count,  src);
}

void graph_remove_node(Graph *g, int idx) {
    Node *node = &g->nodes[idx];
    if (!node->active) return;
    for (int i = 0; i < node->in_count; i++)
        rm_adj(g->nodes[node->adj_in[i]].adj_out,
               &g->nodes[node->adj_in[i]].out_count, idx);
    for (int i = 0; i < node->out_count; i++)
        rm_adj(g->nodes[node->adj_out[i]].adj_in,
               &g->nodes[node->adj_out[i]].in_count, idx);
    node->active = false;
    node->in_count = node->out_count = 0;
}

void graph_twist(Graph *g, int (*edges)[2], int count) {
    for (int i = 0; i < count; i++) graph_remove_edge(g, edges[i][0], edges[i][1]);
    for (int i = 0; i < count; i++) graph_add_edge(g, edges[i][1], edges[i][0]);
}
