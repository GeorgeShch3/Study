#include <iostream>
#include <fstream>
#include <cmath>
#include <mpi.h>

int countDoublesInFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Ошибка открытия файла: " << filename << std::endl;
        return -1; 
    }

    std::string line;
    int count = 0;

    while (std::getline(file, line)) 
        if (!line.empty()) 
            count++;

    file.close(); 
    return count; 
}

void read_vector(const char *filename, double *b, int n) {
    std::ifstream file(filename);

    for (int i = 0; i < n ; i++) 
        file >> b[i];
    file.close();
}

void write_vector(const char *filename, double *x, int n) {
    std::ofstream file(filename);

    for (int i = 0; i < n; i++) 
        file << x[i] << std::endl;
    file.close();
}

void init(int rank, int size, int n, int local_n, double* A, double* b, double* x) {
    if (rank == 0) {
        for (int i = 1; i < size; ++i) {
            MPI_Send(&b[(i - 1) * local_n], local_n, MPI_DOUBLE, i, 0, MPI_COMM_WORLD); 
            MPI_Send(&x[(i - 1) * local_n], n, MPI_DOUBLE, i, 0, MPI_COMM_WORLD); 
            MPI_Send(&A[(i - 1) * n], local_n * n, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
        }
    } else {

        MPI_Recv(b, local_n, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(x, n, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(A, local_n * n, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
 
    MPI_Barrier(MPI_COMM_WORLD);
}

void collect(double* x, int local_n, int rank) {
    MPI_Gather(x, local_n, MPI_DOUBLE, x + local_n * (rank - 1), local_n, MPI_DOUBLE, 0, MPI_COMM_WORLD);
}

void work(double* A, double* b, double* x, int local_n, int n, double eps, int rank) {
    double norm = 0;
    double* temp_x = new double [local_n];

    if (rank != 0) {
        do {
            for (int i = 0; i < local_n; ++i) {
                temp_x[i] = b[i];
                for (int j = 0; j < n; ++j) {
                    if (i != j)
                        temp_x[i] -= A[i * local_n + j] * x[j]; 
                }
                temp_x[i] /= A[i * local_n + i];
            }

            MPI_Gather(temp_x + local_n * (rank - 1), local_n, MPI_DOUBLE, temp_x + local_n * (rank - 1), local_n, MPI_DOUBLE, 0, MPI_COMM_WORLD);
            norm = fabs(x[0] - temp_x[0]);
            for (int i = 0; i < local_n; ++i) {
                if (fabs(x[i] - temp_x[i]) > norm) 
                    norm = fabs(x[i] - temp_x[i]);
                x[i] = temp_x[i];
            }
            if (rank == 2) 
                std::cout << norm << std::endl; 
        } while (eps > eps);
    }
    delete [] temp_x;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv); 

    int rank, size, n, local_n;
    double *x, *b, *A; 
    double eps = std::stod(argv[1]);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        n = countDoublesInFile(argv[3]);
        local_n = n / (size - 1);

        A = new double [n * n];
        b = new double [n];
        x = new double [n];

        read_vector(argv[2], A, n * n);
        read_vector(argv[3], b, n);
        read_vector(argv[4], x, n);
    } 
    
    MPI_Bcast(&local_n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) {
        b = new double [local_n];
        x = new double [n];
        A = new double [local_n * n];
    }
    init(rank, size, n, local_n, A, b, x);


    work(A, b, x, local_n, n, eps, rank);
    collect(x, local_n, rank);
    MPI_Finalize();
    return 0;
}
