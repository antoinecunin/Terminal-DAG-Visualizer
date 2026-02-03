#include "drawdag.h"

#include <stdlib.h>
#include <string.h>

/* ---- Phase 1: topological ordering for cycle breaking ---- */

static void cycle_analysis(const Graph *g, NodeList *order) {
    Graph tmp;
    memcpy(&tmp, g, sizeof tmp);

    NodeList left = {.count = 0}, right = {.count = 0};

    for (;;) {
        bool any_active = false;
        for (int i = 0; i < tmp.count; i++)
            if (tmp.nodes[i].active) { any_active = true; break; }
        if (!any_active) break;

        /* sources (no incoming edges) */
        NodeList batch = {.count = 0};
        for (int i = 0; i < tmp.count; i++)
            if (tmp.nodes[i].active && tmp.nodes[i].in_count == 0)
                batch.items[batch.count++] = i;

        if (batch.count) {
            for (int i = 0; i < batch.count; i++)
                left.items[left.count++] = batch.items[i];
            for (int i = 0; i < batch.count; i++)
                graph_remove_node(&tmp, batch.items[i]);
            continue;
        }

        /* sinks (no outgoing edges) */
        batch.count = 0;
        for (int i = 0; i < tmp.count; i++)
            if (tmp.nodes[i].active && tmp.nodes[i].out_count == 0)
                batch.items[batch.count++] = i;

        if (batch.count) {
            for (int i = 0; i < batch.count; i++)
                right.items[right.count++] = batch.items[i];
            for (int i = 0; i < batch.count; i++)
                graph_remove_node(&tmp, batch.items[i]);
            continue;
        }

        /* max out-rank node (most outgoing minus incoming) */
        int best = -1, best_rank = -999999;
        for (int i = 0; i < tmp.count; i++) {
            if (!tmp.nodes[i].active) continue;
            int rank = tmp.nodes[i].out_count - tmp.nodes[i].in_count;
            if (rank > best_rank) { best_rank = rank; best = i; }
        }
        left.items[left.count++] = best;
        graph_remove_node(&tmp, best);
    }

    order->count = 0;
    for (int i = 0; i < left.count; i++)
        order->items[order->count++] = left.items[i];
    for (int i = 0; i < right.count; i++)
        order->items[order->count++] = right.items[i];
}

/* ---- Phase 1b: reverse backedges ---- */

static void invert_back_edges(const Graph *orig, const NodeList *order,
                              Graph *out) {
    memcpy(out, orig, sizeof *out);

    int position[MAX_NODES];
    memset(position, 0xff, sizeof position);
    for (int i = 0; i < order->count; i++) position[order->items[i]] = i;

    int back_edges[MAX_EDGES][2];
    int back_count = 0;
    for (int i = 0; i < order->count; i++) {
        int node = order->items[i];
        for (int j = 0; j < out->nodes[node].out_count; j++) {
            int child = out->nodes[node].adj_out[j];
            if (position[child] < position[node] && back_count < MAX_EDGES) {
                back_edges[back_count][0] = node;
                back_edges[back_count][1] = child;
                back_count++;
            }
        }
    }
    graph_twist(out, back_edges, back_count);
}

/* ---- Phase 2: assign nodes to levels ---- */

static void level_assignment(const Graph *g, NodeList *levels,
                             int *level_count) {
    Graph tmp;
    memcpy(&tmp, g, sizeof tmp);
    *level_count = 0;

    for (;;) {
        bool any_active = false;
        for (int i = 0; i < tmp.count; i++)
            if (tmp.nodes[i].active) { any_active = true; break; }
        if (!any_active) break;

        NodeList *level = &levels[*level_count];
        level->count = 0;
        for (int i = 0; i < tmp.count; i++)
            if (tmp.nodes[i].active && tmp.nodes[i].out_count == 0)
                level->items[level->count++] = i;
        (*level_count)++;
        for (int i = 0; i < level->count; i++)
            graph_remove_node(&tmp, level->items[i]);
    }

    /* reverse (built bottom-up) */
    for (int i = 0; i < *level_count / 2; i++) {
        NodeList swap = levels[i];
        levels[i] = levels[*level_count - 1 - i];
        levels[*level_count - 1 - i] = swap;
    }
}

/* ---- Phase 2b: insert dummy nodes on multi-level edges ---- */

static void solve_mid_transition(Graph *g, int src, int dst,
                                 NodeList *levels, int *dummy_id) {
    int level_from = g->nodes[src].level;
    int level_to   = g->nodes[dst].level;
    int step = (level_to > level_from) ? 1 : -1;
    graph_remove_edge(g, src, dst);

    int prev = src;
    for (int lvl = level_from + step; lvl != level_to; lvl += step) {
        if (g->count >= MAX_NODES) break;
        int dummy = g->count++;
        Node *dn = &g->nodes[dummy];
        memset(dn, 0, sizeof *dn);
        snprintf(dn->name, MAX_NAME, "_d%d", (*dummy_id)++);
        dn->active = true;
        dn->is_dummy = true;
        dn->level = lvl;
        g->nodes[prev].adj_out[g->nodes[prev].out_count++] = dummy;
        dn->adj_in[dn->in_count++] = prev;
        levels[lvl].items[levels[lvl].count++] = dummy;
        prev = dummy;
    }
    g->nodes[prev].adj_out[g->nodes[prev].out_count++] = dst;
    g->nodes[dst].adj_in[g->nodes[dst].in_count++] = prev;
}

static void get_in_between_nodes(const Graph *orig, Graph *out,
                                 NodeList *levels, int level_count) {
    memcpy(out, orig, sizeof *out);
    for (int i = 0; i < level_count; i++)
        for (int j = 0; j < levels[i].count; j++)
            out->nodes[levels[i].items[j]].level = i;

    int multi_edges[MAX_EDGES][2];
    int multi_count = 0;
    for (int lvl = 0; lvl < level_count; lvl++)
        for (int j = 0; j < levels[lvl].count; j++) {
            int node = levels[lvl].items[j];
            for (int k = 0; k < out->nodes[node].out_count; k++) {
                int child = out->nodes[node].adj_out[k];
                if (abs(out->nodes[child].level - lvl) > 1
                    && multi_count < MAX_EDGES) {
                    multi_edges[multi_count][0] = node;
                    multi_edges[multi_count][1] = child;
                    multi_count++;
                }
            }
        }

    int dummy_id = 0;
    for (int i = 0; i < multi_count; i++)
        solve_mid_transition(out, multi_edges[i][0], multi_edges[i][1],
                             levels, &dummy_id);
}

/* ---- Phase 3: crossing minimisation ---- */

static int neighbor_indices(const Graph *g, int node,
                            const NodeList *level, int *out) {
    int count = 0;
    for (int i = 0; i < g->nodes[node].out_count; i++) {
        int neighbor = g->nodes[node].adj_out[i];
        for (int j = 0; j < level->count; j++)
            if (level->items[j] == neighbor) { out[count++] = j; break; }
    }
    for (int i = 0; i < g->nodes[node].in_count; i++) {
        int neighbor = g->nodes[node].adj_in[i];
        for (int j = 0; j < level->count; j++)
            if (level->items[j] == neighbor) { out[count++] = j; break; }
    }
    return count;
}

static void cost_matrix(const Graph *g, const NodeList *upper,
                        const NodeList *lower, int *matrix) {
    int n = upper->count;
    memset(matrix, 0, MAX_NODES * MAX_NODES * sizeof(int));

    for (int ui = 0; ui < n; ui++)
        for (int vi = ui + 1; vi < n; vi++) {
            int idx_u[MAX_ADJ * 2], idx_v[MAX_ADJ * 2];
            int count_u = neighbor_indices(g, upper->items[ui], lower, idx_u);
            int count_v = neighbor_indices(g, upper->items[vi], lower, idx_v);
            for (int a = 0; a < count_u; a++)
                for (int b = 0; b < count_v; b++) {
                    if (idx_u[a] > idx_v[b]) matrix[ui * MAX_NODES + vi]++;
                    if (idx_u[a] < idx_v[b]) matrix[vi * MAX_NODES + ui]++;
                }
        }
}

static void cross_sort(const int *indices, int count, const int *matrix,
                       int *out, int *out_count) {
    if (count < 2) {
        memcpy(out, indices, count * sizeof *indices);
        *out_count = count;
        return;
    }
    int pivot = count / 2;
    int left[MAX_NODES], left_count;
    int right_half[MAX_NODES], right_count;
    cross_sort(indices, pivot, matrix, left, &left_count);
    cross_sort(indices + pivot, count - pivot, matrix,
               right_half, &right_count);

    int li = 0, ri = 0;
    *out_count = 0;
    while (li < left_count && ri < right_count) {
        if (matrix[left[li] * MAX_NODES + right_half[ri]] <=
            matrix[right_half[ri] * MAX_NODES + left[li]])
            out[(*out_count)++] = left[li++];
        else
            out[(*out_count)++] = right_half[ri++];
    }
    while (li < left_count) out[(*out_count)++] = left[li++];
    while (ri < right_count) out[(*out_count)++] = right_half[ri++];
}

static void two_level_cross_min(const Graph *g, NodeList *levels,
                                int level_count) {
    if (level_count < 2) return;

    int *matrix = malloc(MAX_NODES * MAX_NODES * sizeof(int));
    if (!matrix) return;

    NodeList result[MAX_LEVELS];
    int result_count = 0;
    result[result_count++] = levels[level_count - 1];

    for (int i = level_count - 2; i >= 0; i--) {
        cost_matrix(g, &levels[i], &result[result_count - 1], matrix);
        int indices[MAX_NODES], sorted[MAX_NODES], sorted_count;
        for (int j = 0; j < levels[i].count; j++) indices[j] = j;
        cross_sort(indices, levels[i].count, matrix, sorted, &sorted_count);

        NodeList reordered = {.count = 0};
        for (int j = 0; j < sorted_count; j++)
            reordered.items[reordered.count++] = levels[i].items[sorted[j]];
        result[result_count++] = reordered;
    }
    for (int i = 0; i < result_count; i++)
        levels[i] = result[result_count - 1 - i];

    free(matrix);
}

/* ---- Main entry point ---- */

void sugiyama(const Graph *orig, Graph *out,
              NodeList *levels, int *level_count) {
    NodeList order;
    cycle_analysis(orig, &order);

    Graph acyclic;
    invert_back_edges(orig, &order, &acyclic);
    level_assignment(&acyclic, levels, level_count);
    get_in_between_nodes(orig, out, levels, *level_count);
    two_level_cross_min(out, levels, *level_count);
}
