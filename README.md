# drawdag

A lightweight, free terminal DAG (Directed Acyclic Graph) visualizer built for SSH-friendly environments and Unix pipelines.

This project started as a Python prototype during an internship at [EOST](https://eost.unistra.fr/) (École et Observatoire des Sciences de la Terre, Strasbourg) and has since been rewritten in C for performance and portability.

```
      A
      │
  ┌───┼───┐
  B   C   │
  │   │   │
  └┬──┘  ┌┘
   D     E
   │     │
   └──┬──┘
      F
```

## Overview

- **Sugiyama hierarchical layout** - automatic cycle breaking, level assignment, dummy node insertion, and crossing minimisation
- **Interactive ncurses mode** - click a node to highlight its connected edges and neighbors, scroll with keyboard or mouse wheel
- **Batch mode** (`--print`) - plain text output for piping into other tools
- **Minimal dependencies** - only requires ncurses and a C compiler
- **SSH-compatible** - uses Unicode box-drawing characters, works over any terminal

## Building

Requires `gcc` and `libncursesw` (wide-character ncurses).

```sh
# Debian/Ubuntu
sudo apt install libncursesw5-dev

# Build
make
```

## Usage

```sh
# Interactive mode with built-in demo graph
./drawdag

# Interactive mode with a custom edge list
./drawdag edges.txt

# Read from stdin
cat edges.txt | ./drawdag -

# Batch print (no ncurses, plain text output)
./drawdag --print edges.txt
```

### Edge file format

One edge per line: `FROM TO` (whitespace-separated). Lines starting with `#` are comments. See [edges.txt](edges.txt) for a full example.

### Interactive controls

| Key              | Action                  |
|------------------|-------------------------|
| Click on a node  | Select / deselect       |
| Space            | Deselect                |
| Arrows / `azsd`  | Scroll                  |
| Mouse wheel      | Scroll vertically       |
| `q`              | Quit                    |

## Limitations

- **Edge overlaps on dense graphs.** Some edges may merge visually due to the finite resolution of the terminal character grid.
- **Fixed limits.** The maximum number of nodes (512), edges (4096), and adjacency per node (64) are compile-time constants.

## How it works

The layout is based on the Sugiyama framework for hierarchical graph drawing, as described in:

> K. Sugiyama, S. Tagawa, and M. Toda,
> "[Methods for Visual Understanding of Hierarchical System Structures](https://doi.org/10.1109/TSMC.1981.4308636),"
> *IEEE Transactions on Systems, Man, and Cybernetics*, vol. 11, no. 2,
> pp. 109-125, 1981.

The implementation follows these phases:

1. **Cycle breaking** - greedy ordering of nodes (sources first, sinks last), then reverse any edge that violates this order to make the graph acyclic
2. **Level assignment** - iteratively peel sink nodes to assign each node to a horizontal layer
3. **Dummy node insertion** - edges spanning multiple levels are split into single-level segments with invisible intermediate nodes
4. **Crossing minimisation** - bottom-to-top sweep that reorders nodes within each level to reduce edge crossings, using a merge-sort on a pairwise crossing cost matrix

## Project structure

```
src/
  drawdag.h    - shared types, constants, public API
  graph.c      - graph data structure and operations
  sugiyama.c   - Sugiyama layout algorithm
  canvas.c     - canvas construction and glyph rendering
  render.c     - ncurses interactive display
  parse.c      - edge list parser
  main.c       - entry point
```

## License

This project is free software, licensed under the
[GNU General Public License v3.0](LICENSE).
