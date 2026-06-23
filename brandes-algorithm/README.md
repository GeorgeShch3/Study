# Distributed Betweenness Centrality (Brandes' Algorithm, MPI)

A parallel implementation of **betweenness centrality** for undirected,
unweighted graphs, using Brandes' algorithm distributed across processes with MPI.

## What it computes

Betweenness centrality measures how often a vertex sits on the shortest paths
between other vertices. A high score means the vertex acts as an important
"bridge" in the graph.

For each vertex `u`:

    BC(u) = sum over all pairs (s, t) of  ( shortest paths s->t through u
                                            / total shortest paths s->t )

## How it works

A direct calculation would be very slow (O(n^3)). Brandes' algorithm is much
faster: it runs a BFS from every vertex, counts the shortest paths along the
way, and then walks back through the BFS levels to accumulate each vertex's
contribution.

This version is distributed:

1. **Forward pass (BFS by levels).** Vertices are split across processes. Each
   process handles its own vertices; when an edge leads to a vertex owned by
   another process, a message is sent to that owner. The end of each level is
   detected with a global reduction.
2. **Backward pass.** Levels are processed in reverse to accumulate dependency
   values, again exchanging messages between processes where needed.
3. Each result is divided by 2, since every pair is counted twice in an
   undirected graph.

The graph is stored in CSR format. Helper functions in `defs.h`
(`VERTEX_OWNER`, `VERTEX_LOCAL`, `VERTEX_TO_GLOBAL`) decide which process owns a
vertex and map between global and local indices.

## Files

| File | Purpose |
|------|---------|
| `defs.h` | Shared types, the `graph_t` struct, vertex-distribution helpers |
| `solution_mpi.cpp` | The distributed Brandes' algorithm (`run()`) |
| `main_mpi.cpp` | MPI entry point: argument parsing, timing, output |
| `graph_tools.cpp` | Reading and writing graphs (CSR format) |
| `gen_RMAT.cpp` / `gen_RMAT_mpi.cpp` | RMAT graph generators |
| `gen_random.cpp` / `gen_random_mpi.cpp` | Random graph generators |
| `gen_valid_info.cpp` | Computes the reference answer for validation |
| `reference_bfs.cpp` | Reference BFS implementation (graphs up to 4096 vertices) |
| `validation.cpp` | Compares a result against the reference answer |

## Build

Requires an MPI compiler (`mpicxx`) with OpenMP support.

    make

This builds `gen_RMAT`, `gen_random`, `gen_valid_info`, `validation`, and
`solution_mpi`.

## Usage

1. Generate a graph (`scale` = log2 of the number of vertices):

       ./gen_RMAT -s 16 -out rmat-16

2. Run the computation on N processes:

       mpirun -np 4 ./solution_mpi -in rmat-16 -out rmat-16.res

   Useful flags: `-nIters <K>` (number of timed runs),
   `--generate RMAT|random -s <scale>` (generate a graph on the fly).

3. Create a reference answer (small graphs only, up to 4096 vertices):

       ./gen_valid_info -in rmat-12 -out rmat-12.ans

4. Check correctness:

       ./validation -ans rmat-12.ans -res rmat-12.res

   Output is either `Accepted` (green) or `Wrong answer` (red).

## File formats

- **Graph (binary):** `n` (uint32), `m` (uint64), `align` (uint8),
  `rowsIndices[n+1]` (uint64), `endV[m]` (uint32)
- **Answer (`.ans` / `.res`):** an array of `n` doubles, one BC value per vertex

## Notes

- The reference generator only works for graphs up to 4096 vertices.
- Graph generators expect the number of processes to be a power of two.
- Graphs are undirected and unweighted; each edge is stored twice.
