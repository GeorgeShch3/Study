#include <iostream> 
#include <fstream> 
#include <cstdlib> 
#include <cmath> 
#include <ctime> 
#include <mpi.h> 
 
using namespace std; 
 
double frand(double a, double b) { 
    return a+(b-a)*(rand()/double(RAND_MAX)); 
} 
 
int do_walk(int a, int b, int x, double p, double& t) { 
        int step = 0; 
        while( x > a && x < b ) 
       { 
              if( frand(0,1) < p ) 
                    x += 1; 
              else 
                   x -= 1; 
              t += 1.0; 
              step += 1; 
       } 
       return x; 
} 
 
void run_mc(int a, int b, int x, double p, int N) { 
    double start = MPI_Wtime(); 
    double stop, all_time; 
    double t = 0.0;  
    double w = 0.0;  
    double t1, w1, max_time; 
    int size, rank; 
  
    MPI_Comm_size(MPI_COMM_WORLD, &size); 
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
     
    for( int i = rank * N / size; i < (rank + 1) * N / size; i++ ) { 
        int out = do_walk(a, b, x, p, t); 
        if( out == b ) 
            w += 1; 
    } 
 
    stop = MPI_Wtime(); 
    all_time = stop - start; 
    MPI_Reduce(&t, &t1, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD); 
    MPI_Reduce(&w, &w1, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD); 
    MPI_Reduce(&all_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, 
                MPI_COMM_WORLD); 
 
    if(rank == 0) { 
        ofstream f("output.dat"); 
        f << w/N << " " << t/N << endl; 
        f.close(); 
 
        ofstream s("stat.txt"); 
        s << "Time = " << max_time << endl; 
        s << "Num of Processes = " << size << endl; 
        s  << size  << a << " " << b << " " << x << " " << p << " " << N << endl; 
        s.close(); 
    } 
} 
 
int main(int argc, char** argv) { 
    int i = MPI_Init(&argc, &argv); 
    int a = atoi(argv[1]); 
    int b = atoi(argv[2]); 
    int x = atoi(argv[3]); 
    double p = atof(argv[4]); 
    int N = atoi(argv[5]); 
    run_mc(a, b, x, p, N); 
    i = MPI_Finalize(); 
    return 0; 
} 
