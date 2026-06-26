# Lidar Simulator

Casts virtual rays into a 3D scene and figures out where they hit — output is
a point cloud (CSV: x, y, z, distance) that you can view with `visualize.py`.

## 4 scenarios

- **corner_scan** — a room, 4 lidars in the corners, each with its own
  calibration error. Results get stitched by fitting planes to the walls
  and discarding outliers.
- **adaptive_scan** — instead of a uniform grid, points get placed denser
  where there are edges (corners, boundaries) and sparser on flat surfaces.
- **obj_scan** — loads your own `.obj`/`.stl` file and scans it from 6 sides.
- **robot_scan** — a robot drives a snake path through a room, accumulating
  odometry drift; compares the map before and after ICP correction.
  Needs Eigen3 + small_gicp.

The `*.hpp` files are shared geometry/math/noise code, used by all 4 programs.
Each `.cpp` only contains what's unique to that scenario.

## Setup

```bash
git clone <your-repo-url>
cd coursework
```

## Build

Needs g++ (C++17), CMake, OpenMP:
```bash
sudo apt install build-essential cmake libomp-dev
```

```bash
cmake -B build
cmake --build build -j$(nproc)
```

This builds `corner_scan`, `adaptive_scan`, `obj_scan` — no extra dependencies.

For `robot_scan` you also need Eigen3 and an ICP library:
```bash
sudo apt install libeigen3-dev
git clone https://github.com/koide3/small_gicp.git
cmake -B build -DSMALL_GICP_DIR=./small_gicp
cmake --build build -j$(nproc)
```

## Run

```bash
./build/corner_scan
./build/adaptive_scan
./build/obj_scan model.obj      # needs a real file, fails without one
./build/robot_scan
```

Each program writes CSV files next to itself and prints how many points it got.

## Visualize results

Needs Python with numpy/pandas/matplotlib:
```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

```bash
python visualize.py corner       # after corner_scan
python visualize.py adaptive     # after adaptive_scan
python visualize.py robot        # after robot_scan
python visualize.py obj points.csv   # any csv of your own
```

Opens a 3D point cloud, top/side projections, and for corner/robot — a table
in the console showing point spread per wall before/after correction (lower
= more accurate).

## Troubleshooting

- **`obj_scan`: "Cannot open file"** — no real `.obj` file was passed in.
- **`ModuleNotFoundError: numpy`** — wrong venv active. Check: `echo $VIRTUAL_ENV`.
- **`robot_scan` won't build** — missing `-DSMALL_GICP_DIR` or `libeigen3-dev`.
