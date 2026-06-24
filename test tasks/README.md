# Geo KNN Search (VP-Tree)

A Go programm that finds the *k* nearest geographic points to a query coordinate
using a **Vantage-Point Tree** and **Haversine** distance.

## Usage

```bash
go run task1.go coordinates.csv
```

The CSV has no header and two columns: `latitude,longitude`. After loading,
enter coordinates as `latitude longitude` at the prompt to get the 5 nearest
points (default) with distances in km. Type `q` to quit.

## How it works

Coordinates are stored as `int32` (scaled by `1e6`). The tool builds a VP-Tree
over all points and answers each query with a bounded max-heap, pruning subtrees
via the triangle inequality.

*Note: program output and error messages are in Russian.*
