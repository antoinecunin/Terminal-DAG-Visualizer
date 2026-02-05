#include "drawdag.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- Connector lookup table ---- */

const wchar_t CONNECTOR[16] = {
    L' ',  L'\u2565', L'\u2567', L'\u2502',
    L'\u2576', L'\u2514', L'\u250c', L'\u251c',
    L'\u2574', L'\u2518', L'\u2510', L'\u2524',
    L'\u2500', L'\u2534', L'\u252c', L'\u253c',
};

/* ---- helpers ---- */

static void path_push(Canvas *cv, int row, int col) {
    if (cv->ptotal >= cv->pcap) {
        int new_cap = cv->pcap ? cv->pcap * 2 : 8192;
        int *new_pr = realloc(cv->pr, new_cap * sizeof *cv->pr);
        if (!new_pr) return;
        cv->pr = new_pr;
        int *new_pc = realloc(cv->pc, new_cap * sizeof *cv->pc);
        if (!new_pc) return;
        cv->pc = new_pc;
        cv->pcap = new_cap;
    }
    cv->pr[cv->ptotal] = row;
    cv->pc[cv->ptotal] = col;
    cv->ptotal++;
}

static void add_dir(Canvas *cv, int x, int y, uint8_t d) {
    if (y >= 0 && y < cv->height && x >= 0 && x < cv->width)
        cv->dirs[y * cv->width + x] |= d;
}

static void draw_vline(Canvas *cv, int x, int y0, int y1) {
    int s = (y1 > y0) ? 1 : -1;
    int y = y0;
    while (y != y1) {
        add_dir(cv, x, y, s > 0 ? DIR_S : DIR_N);
        path_push(cv, y, x);
        y += s;
        add_dir(cv, x, y, s > 0 ? DIR_N : DIR_S);
    }
    path_push(cv, y1, x);
}

static void draw_hline(Canvas *cv, int y, int x0, int x1) {
    int s = (x1 > x0) ? 1 : -1;
    int x = x0;
    while (x != x1) {
        add_dir(cv, x, y, s > 0 ? DIR_E : DIR_W);
        path_push(cv, y, x);
        x += s;
        add_dir(cv, x, y, s > 0 ? DIR_W : DIR_E);
    }
    path_push(cv, y, x1);
}

/* ---- internal steps ---- */

static void canvas_place_nodes(Canvas *cv, const NodeList *levels,
                               int level_count) {
    for (int lvl = 0; lvl < level_count; lvl++) {
        int nodes_in_level = levels[lvl].count;
        if (nodes_in_level == 0) nodes_in_level = 1;
        for (int ni = 0; ni < levels[lvl].count; ni++) {
            int node = levels[lvl].items[ni];
            cv->node_col[node] = (int)round(
                (ni + 0.5) / (double)nodes_in_level * (cv->width - 1));
            cv->node_row[node] = VERT_SPACING * lvl;
        }
    }
}

static void canvas_route_edges(Canvas *cv, const Graph *g) {
    cv->pr = NULL; cv->pc = NULL;
    cv->ptotal = cv->pcap = 0;
    cv->ep_count = 0;

    for (int i = 0; i < g->count; i++) {
        if (!g->nodes[i].active) continue;
        int src_col = cv->node_col[i], src_row = cv->node_row[i];
        int edge_row = src_row + EDGE_V_OFFSET;
        for (int j = 0; j < g->nodes[i].out_count; j++) {
            int dst = g->nodes[i].adj_out[j];
            int dst_col = cv->node_col[dst], dst_row = cv->node_row[dst];
            int path_offset = cv->ptotal;
            draw_vline(cv, src_col, src_row, edge_row);
            draw_hline(cv, edge_row, src_col, dst_col);
            draw_vline(cv, dst_col, edge_row, dst_row);
            if (cv->ep_count < MAX_EDGES) {
                int edge_idx = cv->ep_count++;
                cv->ep_src[edge_idx] = i;
                cv->ep_dst[edge_idx] = dst;
                cv->ep_off[edge_idx] = path_offset;
                cv->ep_len[edge_idx] = cv->ptotal - path_offset;
            }
        }
    }
}

static void canvas_stamp_glyphs(Canvas *cv, const Graph *g) {
    for (int i = 0; i < cv->height * cv->width; i++)
        cv->cells[i] = CONNECTOR[cv->dirs[i]];

    memset(cv->has_bnd, 0, sizeof cv->has_bnd);
    for (int i = 0; i < g->count; i++) {
        if (!g->nodes[i].active || g->nodes[i].is_dummy) continue;
        int col = cv->node_col[i], row = cv->node_row[i];
        int label_len = (int)strlen(g->nodes[i].name);
        int label_start = col - label_len / 2;
        for (int c = 0; c < label_len; c++) {
            int x = label_start + c;
            if (x >= 0 && x < cv->width)
                cv->cells[row * cv->width + x] =
                    (wchar_t)g->nodes[i].name[c];
        }
        cv->bnd_xs[i] = label_start;
        cv->bnd_xe[i] = label_start + label_len - 1;
        cv->bnd_y[i]  = row;
        cv->has_bnd[i] = true;
    }
}

/* ---- public API ---- */

int canvas_compute_width(const Graph *g, const NodeList *levels,
                         int level_count) {
    int max_label = 1;
    for (int i = 0; i < g->count; i++)
        if (g->nodes[i].active && !g->nodes[i].is_dummy) {
            int len = (int)strlen(g->nodes[i].name);
            if (len > max_label) max_label = len;
        }
    int cols_per_node = max_label + 2;
    if (cols_per_node < MIN_COLS_NODE) cols_per_node = MIN_COLS_NODE;
    int max_level_size = 1;
    for (int i = 0; i < level_count; i++)
        if (levels[i].count > max_level_size)
            max_level_size = levels[i].count;
    return cols_per_node * max_level_size + CANVAS_MARGIN;
}

void build_canvas(Canvas *cv, const Graph *g,
                  const NodeList *levels, int level_count, int canvas_width) {
    cv->width = canvas_width;
    cv->height = VERT_SPACING * level_count + CANVAS_MARGIN;
    cv->cells = calloc(cv->height * cv->width, sizeof *cv->cells);
    cv->dirs  = calloc(cv->height * cv->width, sizeof *cv->dirs);
    if (!cv->cells || !cv->dirs) return;
    for (int i = 0; i < cv->height * cv->width; i++) cv->cells[i] = L' ';

    canvas_place_nodes(cv, levels, level_count);
    canvas_route_edges(cv, g);
    canvas_stamp_glyphs(cv, g);
}

void canvas_free(Canvas *cv) {
    free(cv->cells); free(cv->dirs);
    free(cv->pr);    free(cv->pc);
}
