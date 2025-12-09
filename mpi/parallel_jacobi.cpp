#include <iostream>
#include <fstream>
#include <cmath>
#include <mpi.h>

MPI_Datatype lines;
MPI_Datatype colums;

void create_lines_type(int m) {
    MPI_Type_contiguous(m, MPI_DOUBLE, &lines); 
    MPI_Type_commit(&lines); 
}

void create_colums_type(int n, int local_n){
    MPI_Type_vector(n, local_n, n, MPI_DOUBLE, &colums); 
    MPI_Type_commit(&colums);
}

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

void init_lines(int rank, int size, int n, int local_n, double* A, double* b, double* x) {
    if (rank == 0) {
        for (int i = 1; i < size; ++i) {
            MPI_Send(&b[(i - 1) * local_n], local_n, MPI_DOUBLE, i, 0, MPI_COMM_WORLD); 
            MPI_Send(&x[0], n, MPI_DOUBLE, i, 0, MPI_COMM_WORLD); 
            MPI_Send(&A[(i - 1) * n * local_n], local_n * n, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
        }
    } else {

        MPI_Recv(b, local_n, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(x, n, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(A, local_n * n, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    MPI_Barrier(MPI_COMM_WORLD);
}

void init_colums(int rank, int size, int n, int local_n, double* A, double* b, double* x) {
    if (rank == 0) {
        for (int i = 1; i < size; ++i) {
            MPI_Send(&b[0], n, MPI_DOUBLE, i, 0, MPI_COMM_WORLD); 
            MPI_Send(&x[0], n, MPI_DOUBLE, i, 0, MPI_COMM_WORLD); 
            MPI_Send(&A[(i - 1) * local_n], 1, colums, i, 0, MPI_COMM_WORLD);
        }
    } else {

        MPI_Recv(b, n, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(x, n, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(A, n*local_n,MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    MPI_Barrier(MPI_COMM_WORLD);
}

void Jacobi_colums(double* A, double* b, double* x, int local_n, int n, double eps, int rank, int size) {
    double norm = 0;
    double* temp_x = new double [n];

    do {
        if (rank != 0) {
            for (int i = 0; i < n; ++i) {
                temp_x[i] = 0;
                for (int j = 0; j < local_n; ++j) 
                    if (i != j + (rank - 1) * local_n ) 
                        temp_x[i] -= A[i * local_n + j] * x[j];
            }
        } 
        else {
            for (int i = 0; i < n; ++i)
                temp_x[i] = x[i];
        }

        MPI_Reduce(temp_x, x, n, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        if (rank == 0) {
            for (int i = 0; i < n; ++i) {
                x[i] += b[i];
                x[i] /= A[i + i * n ];
            }

            norm = fabs(-x[0] + temp_x[0]);
            for (int i = 0; i < n; ++i) 
                if (fabs(-x[i] + temp_x[i]) > norm) 
                    norm = fabs(-x[i] + temp_x[i]);  
            std::cout << norm << std::endl;        
        }
        MPI_Bcast(x, n, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Bcast(&norm, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    } while (norm> eps);
    
    delete [] temp_x;
}

void Jacobi_lines(double* A, double* b, double* x, int local_n, int n, double eps, int rank, int size) {
    double norm = eps + 1;
    double* temp_x = new double [local_n];
    double max_norm;
    do {
        if (rank != 0) {
            for (int i = 0; i < local_n; ++i) {
                temp_x[i] = b[i];
                for (int j = 0; j < n; ++j) {
                    if (i != j - (rank - 1) * local_n ) 
                        temp_x[i] -= A[i * n + j] * x[j];
                }
                temp_x[i] /= A[i * n + local_n * (rank - 1) + i];
            }

            norm = fabs(x[0] - temp_x[0]);
            for (int i = 0; i < local_n; ++i) 
                if (fabs(x[i] - temp_x[i]) > norm) 
                    norm = fabs(x[i] - temp_x[i]);  
            if (rank == 1)
                MPI_Send(&norm, 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);   
            MPI_Send(temp_x, local_n, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
        } else {
            MPI_Recv(&norm, 1, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (int i = 1; i < size; ++i) {
                MPI_Recv(x + local_n * (i - 1), local_n, MPI_DOUBLE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        } 
        MPI_Bcast(x, n, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Reduce(&norm, &max_norm, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Bcast(&max_norm, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        if (rank == 1) 
        std::cout << max_norm << std::endl;
    } while (max_norm> eps);
    
    delete [] temp_x;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv); 

    int rank, size, n, local_n;
    double *x, *b, *A; 
    double eps = std::stod(argv[1]);

    int state = 12;

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
        if (state == 1) {
            b = new double [n];
        } else {
            b = new double [local_n];
        }
        x = new double [n];
        A = new double [n*local_n];
    }
    if (state == 1) {
        create_colums_type( n, local_n);
        init_colums(rank, size, n, local_n, A, b, x);
        Jacobi_colums(A, b, x, local_n, n, eps, rank, size);
    } else {
        create_lines_type(local_n);
        init_lines(rank, size, n, local_n, A, b, x);
        Jacobi_lines(A, b, x, local_n, n, eps, rank, size);
    }

    delete [] A;
    delete [] b;
    delete [] x;
    MPI_Finalize();
    return 0;
}
