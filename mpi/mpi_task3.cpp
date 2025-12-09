#include <fstream>
#include "mpi.h"
#include <cmath>
#include <iostream> 

using namespace std;

int f(int* data, int i, int j, int n)
{
	int state = data[i*(n+2)+j];
	int s = -state;
	for( int ii = i - 1; ii <= i + 1; ii ++ )
		for( int jj = j - 1; jj <= j + 1; jj ++ )
			s += data[ii*(n+2)+jj];
	if( state==0 && s==3 )
		return 1;
	if( state==1 && (s<2 || s>3) ) 
		return 0;
	return state;
}

void update_data(int n, int* data, int* temp)
{
	for( int i=1; i<=n; i++ )
		for( int j=1; j<=n; j++ )
			temp[i*(n+2)+j] = f(data, i, j, n);
}

void init(int n, int* data, int* temp)
{
	for( int i=0; i<(n+2)*(n+2); i++ )
		data[i] = temp[i] = 0;
	int n0 = 1+n/2;
	int m0 = 1+n/2;
	data[(n0-1)*(n+2)+m0] = 1;
	data[n0*(n+2)+m0+1] = 1;
	for( int i=0; i<3; i++ )
		data[(n0+1)*(n+2)+m0+i-1] = 1;
}

void setup_boundaries(int n, int* data, int p)
{

	for( int i=0; i<n+2; i++ )
	{
		data[i*(n+2)+0] = data[i*(n+2)+n];
		data[i*(n+2)+n+1] = data[i*(n+2)+1];
	}
	for( int j=0; j<n+2; j++ )
	{
		data[0*(n+2)+j] = data[n*(n+2)+j];
		data[(n+1)*(n+2)+j] = data[1*(n+2)+j];
	}
}

void DistributeData(int* data, int n, int p, int rank, int size) {
    int rows_per_proc = (n - 2) / (size - 1);  
    int extra_rows = (n - 2) % (size - 1);    

    int num_rows = rows_per_proc + (rank - 1 < extra_rows ? 1 : 0);

    if (rank == 0) { 
        for (int proc = 1; proc < size; proc++) {
            int start = (proc - 1) * rows_per_proc + 1;
            if (proc != 1) {
                if (extra_rows >= proc) {
                    start += (proc - 1);
                }
                else {
                    start += extra_rows;
                }
            }
            int count = rows_per_proc + (proc - 1 < extra_rows ? 1 : 0);

            for (int i = 0; i < count; i++) {
                MPI_Send(&data[(start + i) * n + 1], n - 2, MPI_INT, proc, 0, MPI_COMM_WORLD);
            }
        }
    } else { 
        int* recv_data = new int[num_rows * (n - 2)];
        for (int i = 0; i < num_rows; i++) {
            MPI_Recv(&data[i * (n - 2)], n - 2, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        for (int i = 0; i < num_rows; i++) {
            for (int j = 0; j < n - 2; j++) {
                cout << data[i * (n - 2) + j] << " ";
            }
            cout <<"   " << rank << "\n";
        }
    }
}

void run_life(int n, int T, int rank, int size)
{   
    int p = sqrt(size - 1);
    int local_n = n / p;
    int* data, *temp;

    if (rank == 0) {
	    data = new int[(n+2)*(n+2)];
	    temp = new int[(n+2)*(n+2)];
        init(n, data, temp);
    } else {
        data = new int[(local_n)*(local_n)];
	    temp = new int[(local_n)*(local_n)];
    }
    DistributeData(data, n + 2, p, rank, size);

	for( int t = 0; t < T; t++ )
	{
	//	setup_boundaries(local_n, data, p);
	//	update_data(n, data, temp);
	//	swap(data, temp);
	}
    if (rank == 0){
	ofstream f("output.dat");
	for( int i=1; i<=n; i++ )
	{
		for( int j=1; j<=n; j++ )
			f << data[i*(n+2)+j];
		f << endl;
	}
	f.close();
    }
	delete[] data;
	delete[] temp;
}

int main(int argc, char** argv)
{
    MPI_Init(&argc, &argv);
    int n = atoi(argv[1]);
	int T = atoi(argv[2]);
    int rank, size;
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
	run_life(n, T, rank, size);

    MPI_Finalize();
	return 0;
}
