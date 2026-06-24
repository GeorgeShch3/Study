# README

**mpi_task1** — Parallel MPI Monte-Carlo simulation of a 1D random walk (the gambler's ruin problem). A particle starts at `x` and moves right with probability `p`, otherwise left, until it reaches boundary `a` or `b`. The `N` walks are split across processes; estimates the probability of reaching the right boundary and the average number of steps.
Run: `mpirun -np <k> ./prog1 a b x p N`

**mpi_task2** — Parallel MPI genetic algorithm (island model) for minimizing test functions (Sphere, Rosenbrock, Rastrigin). Each process runs its own subpopulation with selection, crossover and mutation, and processes periodically exchange individuals via ring migration. Process 0 prints the best and average value per generation.
Run: `mpirun -np <k> ./prog2` (`m` must be divisible by the number of processes)

**mpi_task3** — Parallel MPI implementation of Conway's Game of Life with toroidal boundaries. The grid is distributed across processes row-wise and evolved for `T` steps; the result is written to `output.dat`.
Run: `mpirun -np <k> ./prog3 n T`

**parallel_jacobi** — Parallel MPI solver of a linear system `Ax = b` by the Jacobi method. The matrix is distributed across processes either by rows or by columns (chosen via `state`), iterating until the residual norm drops below `eps`. The matrix, vector and initial guess are read from files.
Run: `mpirun -np <k> ./prog4 eps matrixA.txt b.txt x0.txt`
