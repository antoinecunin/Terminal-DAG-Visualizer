#include "drawdag.h"

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

/* ---- helpers ---- */

static void mark_edge_path(bool *highlight, const Canvas *cv,
                           int src, int dst) {
    for (int e = 0; e < cv->ep_count; e++) {
        if (cv->ep_src[e] == src && cv->ep_dst[e] == dst) {
            for (int p = 0; p < cv->ep_len[e]; p++) {
                int row = cv->pr[cv->ep_off[e] + p];
                int col = cv->pc[cv->ep_off[e] + p];
                highlight[row * cv->width + col] = true;
            }
            break;
        }
    }
}

static void compute_highlight(bool *highlight, const Canvas *cv,
                              const Graph *g, int selected) {
    memset(highlight, 0, cv->width * cv->height * sizeof *highlight);
    if (selected < 0) return;

    bool connected[MAX_NODES] = {0};
    bool visited[MAX_NODES];
    int stack[MAX_NODES];
    int stack_top;

    /* forward traversal */
    stack_top = 0; memset(visited, 0, sizeof visited);
    stack[stack_top++] = selected;
    while (stack_top > 0) {
        int node = stack[--stack_top];
        if (visited[node]) continue;
        visited[node] = true;
        for (int i = 0; i < g->nodes[node].out_count; i++) {
            int neighbor = g->nodes[node].adj_out[i];
            mark_edge_path(highlight, cv, node, neighbor);
            if (g->nodes[neighbor].is_dummy) stack[stack_top++] = neighbor;
            else                             connected[neighbor] = true;
        }
    }

    /* backward traversal */
    stack_top = 0; memset(visited, 0, sizeof visited);
    stack[stack_top++] = selected;
    while (stack_top > 0) {
        int node = stack[--stack_top];
        if (visited[node]) continue;
        visited[node] = true;
        for (int i = 0; i < g->nodes[node].in_count; i++) {
            int neighbor = g->nodes[node].adj_in[i];
            mark_edge_path(highlight, cv, neighbor, node);
            if (g->nodes[neighbor].is_dummy) stack[stack_top++] = neighbor;
            else                             connected[neighbor] = true;
        }
    }

    /* highlight connected node labels */
    for (int i = 0; i < g->count; i++) {
        if (!connected[i] || !cv->has_bnd[i]) continue;
        int row = cv->bnd_y[i];
        for (int x = cv->bnd_xs[i]; x <= cv->bnd_xe[i]; x++)
            if (x >= 0 && x < cv->width)
                highlight[row * cv->width + x] = true;
    }
}

static void render(WINDOW *win, const Canvas *cv, const bool *highlight,
                   int selected, int scroll_x, int scroll_y) {
    int max_row, max_col;
    getmaxyx(win, max_row, max_col);
    int draw_width = max_col - DRAW_MARGIN;
    cchar_t cch;
    wchar_t wstr[2] = {0, 0};

    for (int screen_row = 0; screen_row < max_row; screen_row++) {
        int canvas_row = scroll_y + screen_row;
        if (canvas_row >= cv->height) break;
        for (int screen_col = 0; screen_col < draw_width; screen_col++) {
            int canvas_col = scroll_x + screen_col;
            if (canvas_col >= cv->width) break;
            wchar_t ch = cv->cells[canvas_row * cv->width + canvas_col];
            attr_t attr;
            short pair;
            if (highlight[canvas_row * cv->width + canvas_col])
                                     { attr = A_BOLD;   pair = 2; }
            else if (ch != L' ')     { attr = A_NORMAL; pair = 1; }
            else                     { attr = A_NORMAL; pair = 0; }
            wstr[0] = ch;
            setcchar(&cch, wstr, attr, pair, NULL);
            mvadd_wch(screen_row, screen_col, &cch);
        }
    }

    /* highlight selected node label */
    if (selected >= 0 && cv->has_bnd[selected]) {
        int row = cv->bnd_y[selected];
        int screen_y = row - scroll_y;
        if (screen_y >= 0 && screen_y < max_row) {
            for (int x = cv->bnd_xs[selected]; x <= cv->bnd_xe[selected]; x++) {
                int screen_x = x - scroll_x;
                if (screen_x >= 0 && screen_x < draw_width) {
                    wstr[0] = cv->cells[row * cv->width + x];
                    setcchar(&cch, wstr, A_REVERSE, 2, NULL);
                    mvadd_wch(screen_y, screen_x, &cch);
                }
            }
        }
    }
}

static int find_clicked(const Canvas *cv, int node_count,
                        int abs_x, int abs_y) {
    for (int i = 0; i < node_count; i++)
        if (cv->has_bnd[i] && abs_y == cv->bnd_y[i] &&
            abs_x >= cv->bnd_xs[i] && abs_x <= cv->bnd_xe[i])
            return i;
    return -1;
}

/* ---- setup ---- */

static void render_setup(mmask_t *scroll_up, mmask_t *scroll_down) {
    curs_set(0);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    start_color();
    use_default_colors();
    init_pair(1, COLOR_WHITE, -1);
    init_pair(2, COLOR_YELLOW, -1);

    *scroll_up = 0;
    *scroll_down = 0;
#ifdef BUTTON4_PRESSED
    *scroll_up = BUTTON4_PRESSED;
#endif
#ifdef BUTTON5_PRESSED
    *scroll_down = BUTTON5_PRESSED;
#endif
    if (*scroll_up && !*scroll_down)
        *scroll_down = *scroll_up << 5;
    if (*scroll_down && !*scroll_up)
        *scroll_up = *scroll_down >> 5;
}

/* ---- public API ---- */

void event_loop(const Graph *g, const Canvas *cv) {
    mmask_t scroll_up_mask, scroll_down_mask;
    render_setup(&scroll_up_mask, &scroll_down_mask);

    int scroll_x = 0, scroll_y = 0, selected = -1;
    bool *highlight = calloc(cv->width * cv->height, sizeof *highlight);
    if (!highlight) return;

    for (;;) {
        int term_rows, term_cols;
        getmaxyx(stdscr, term_rows, term_cols);
        int max_scroll_x = cv->width > term_cols ?
                           cv->width - term_cols : 0;
        int max_scroll_y = cv->height > (term_rows - DRAW_MARGIN) ?
                           cv->height - (term_rows - DRAW_MARGIN) : 0;
        if (scroll_x < 0) scroll_x = 0;
        if (scroll_x > max_scroll_x) scroll_x = max_scroll_x;
        if (scroll_y < 0) scroll_y = 0;
        if (scroll_y > max_scroll_y) scroll_y = max_scroll_y;

        compute_highlight(highlight, cv, g, selected);
        erase();
        render(stdscr, cv, highlight, selected, scroll_x, scroll_y);
        refresh();

        int key = getch();
        if (key == 'q' || key == 'Q') break;
        else if (key == ' ')                            selected = -1;
        else if (key == KEY_LEFT  || key == 'a')        scroll_x -= SCROLL_STEP;
        else if (key == KEY_RIGHT || key == 'd')        scroll_x += SCROLL_STEP;
        else if (key == KEY_UP    || key == 'z')        scroll_y -= SCROLL_STEP;
        else if (key == KEY_DOWN  || key == 's')        scroll_y += SCROLL_STEP;
        else if (key == KEY_MOUSE) {
            MEVENT mouse;
            if (getmouse(&mouse) == OK) {
                if (scroll_up_mask && (mouse.bstate & scroll_up_mask))
                    scroll_y -= SCROLL_STEP;
                else if (scroll_down_mask && (mouse.bstate & scroll_down_mask))
                    scroll_y += SCROLL_STEP;
                else if (mouse.bstate & BUTTON1_CLICKED) {
                    int clicked = find_clicked(cv, g->count,
                                              mouse.x + scroll_x,
                                              mouse.y + scroll_y);
                    selected = (clicked == selected) ? -1 : clicked;
                }
            }
        }
    }
    free(highlight);
}
