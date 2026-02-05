#include "drawdag.h"

#include <locale.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

static void print_canvas(const Canvas *cv) {
    char buf[MAX_NAME * MAX_NODES];
    for (int row = 0; row < cv->height; row++) {
        int len = 0;
        for (int col = 0; col < cv->width; col++)
            len += wctomb(buf + len, cv->cells[row * cv->width + col]);
        /* trim trailing spaces */
        while (len > 0 && buf[len - 1] == ' ') len--;
        buf[len] = '\0';
        puts(buf);
    }
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");

    bool batch = false;
    RawEdge edges[MAX_EDGES];
    int edge_count = 0;
    const char *file_arg = NULL;

    /* parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--print") == 0)
            batch = true;
        else
            file_arg = argv[i];
    }

    if (file_arg) {
        FILE *fp;
        if (strcmp(file_arg, "-") == 0) {
            fp = stdin;
        } else {
            fp = fopen(file_arg, "r");
            if (!fp) { perror(file_arg); return 1; }
        }
        edge_count = read_edges(fp, edges, MAX_EDGES);
        if (fp != stdin) fclose(fp);
        if (fp == stdin && !batch && !freopen("/dev/tty", "r", stdin)) {
            fprintf(stderr, "Cannot open /dev/tty\n");
            return 1;
        }
    } else {
        edge_count = default_edges(edges);
    }

    if (edge_count == 0) { fprintf(stderr, "No edges\n"); return 1; }

    /* build graph */
    Graph orig;
    graph_init(&orig);
    for (int i = 0; i < edge_count; i++) {
        int src = graph_find_or_add(&orig, edges[i].src);
        int dst = graph_find_or_add(&orig, edges[i].dst);
        graph_add_edge(&orig, src, dst);
    }

    /* layout */
    Graph layout;
    NodeList levels[MAX_LEVELS];
    int level_count;
    sugiyama(&orig, &layout, levels, &level_count);

    int canvas_width = canvas_compute_width(&layout, levels, level_count);

    Canvas cv = {0};
    build_canvas(&cv, &layout, levels, level_count, canvas_width);

    if (batch) {
        print_canvas(&cv);
    } else {
        initscr();
        noecho();
        keypad(stdscr, TRUE);
        event_loop(&layout, &cv);
        endwin();
    }

    canvas_free(&cv);
    return 0;
}
