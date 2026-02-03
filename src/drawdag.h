/*
 * drawdag.h - Terminal DAG visualizer
 *
 * Constants, data structures, and public API.
 */

#ifndef DRAWDAG_H
#define DRAWDAG_H

#define _XOPEN_SOURCE_EXTENDED 1

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

/* ---- Limits ---- */

#define MAX_NODES       512
#define MAX_ADJ          64
#define MAX_NAME         64
#define MAX_LEVELS      128
#define MAX_EDGES      4096

/* ---- Layout constants ---- */

#define VERT_SPACING     3
#define EDGE_V_OFFSET    2
#define MIN_COLS_NODE    4
#define CANVAS_MARGIN    1
#define DRAW_MARGIN      1
#define SCROLL_STEP      VERT_SPACING

/* ---- Direction bitmask for connectors ---- */

#define DIR_N  1
#define DIR_S  2
#define DIR_E  4
#define DIR_W  8

/* ---- Data structures ---- */

typedef struct {
    char name[MAX_NAME];
    int adj_in[MAX_ADJ],  in_count;
    int adj_out[MAX_ADJ], out_count;
    int level;
    bool is_dummy;
    bool active;
} Node;

typedef struct {
    Node nodes[MAX_NODES];
    int count;
} Graph;

typedef struct {
    int items[MAX_NODES];
    int count;
} NodeList;

typedef struct {
    wchar_t *cells;
    uint8_t *dirs;
    int width, height;

    int node_col[MAX_NODES];
    int node_row[MAX_NODES];
    int bnd_xs[MAX_NODES], bnd_xe[MAX_NODES], bnd_y[MAX_NODES];
    bool has_bnd[MAX_NODES];

    int *pr, *pc;           /* path row/col pools */
    int ptotal, pcap;
    int ep_src[MAX_EDGES], ep_dst[MAX_EDGES];
    int ep_off[MAX_EDGES],  ep_len[MAX_EDGES];
    int ep_count;
} Canvas;

typedef struct {
    char src[MAX_NAME], dst[MAX_NAME];
} RawEdge;

/* ---- Connector lookup table ---- */

extern const wchar_t CONNECTOR[16];

/* ---- Graph operations ---- */

void graph_init(Graph *g);
int  graph_find(const Graph *g, const char *name);
int  graph_add(Graph *g, const char *name);
int  graph_find_or_add(Graph *g, const char *name);
void graph_add_edge(Graph *g, int src, int dst);
void graph_remove_edge(Graph *g, int src, int dst);
void graph_remove_node(Graph *g, int idx);
void graph_twist(Graph *g, int (*edges)[2], int count);

/* ---- Sugiyama layout ---- */

void sugiyama(const Graph *orig, Graph *out, NodeList *levels, int *level_count);

/* ---- Canvas ---- */

void build_canvas(Canvas *cv, const Graph *g,
                  const NodeList *levels, int level_count, int canvas_width);
void canvas_free(Canvas *cv);

/* ---- Rendering ---- */

void event_loop(const Graph *g, const Canvas *cv);

/* ---- Input parsing ---- */

int read_edges(FILE *fp, RawEdge *edges, int max);
int default_edges(RawEdge *e);

#endif /* DRAWDAG_H */
