# README

**mpi_task1** — Parallel MPI Monte-Carlo simulation of a 1D random walk (gambler's ruin): a particle moves right with probability `p` until it hits a boundary. Estimates the probability of reaching the right boundary and the average number of steps.

**mpi_task2** — Parallel MPI genetic algorithm (island model) minimizing test functions (Sphere, Rosenbrock, Rastrigin). Each process evolves its own subpopulation via selection, crossover and mutation, with periodic ring migration between processes.

**mpi_task3** — Parallel MPI implementation of Conway's Game of Life with toroidal boundaries. The grid is distributed across processes row-wise and evolved for `T` steps.

**parallel_jacobi** — Parallel MPI solver of a linear system `Ax = b` by the Jacobi method, distributing the matrix by rows or columns and iterating until the residual norm drops below `eps`.
