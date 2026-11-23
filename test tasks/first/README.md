# Geo KNN Search (VP-Tree)

A Go program that finds the *k* nearest geographic points to a query coordinate
using a **Vantage-Point Tree** and **Haversine** distance.

## Usage

Run from `test tasks/first` in the cloned repository. Requires Go; no database or external packages are needed. A small sample file is included:

```bash
go run task2.go coordinates.example.csv
```

For your own data:

```bash
go run task2.go coordinates.csv
```

The CSV has no header and two columns: `latitude,longitude`. After loading,
enter coordinates as `latitude longitude` at the prompt to get the 5 nearest
points (default) with distances in km. Type `q` to quit.

## How it works

Coordinates are stored as `int32` (scaled by `1e6`). The tool builds a VP-Tree
over all points and answers each query with a bounded max-heap, pruning subtrees
via the triangle inequality.

*Note: program output and error messages are in Russian.*
