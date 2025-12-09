#include <mpi.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
#include <random> 

using namespace std;

double gen(const double start, const double end) {
    static random_device rd;         
    static mt19937 gen(rd());        
    uniform_real_distribution<> dis(start, end);  
	return dis(gen);
}

double frand() 
{
	static random_device rd;
  	static mt19937 gen(rd());
  	static uniform_real_distribution<> dis(0.0, 1.0);
	return dis(gen);
}

double sphere_function(double* a, int n) {
  	double sum = 0;
  	for (int i = 0; i < n; i++) 
    	sum += a[i] * a[i];
  	return sum;
}

double rosenbrock_function(double* a, int n) {
  	double sum = 0;
  	for (int i = 0; i < n - 1; i++) 
    	sum += 100 * (a[i + 1] - a[i] * a[i]) * (a[i + 1] - a[i] * a[i]) +  (1 - a[i]) * (1 - a[i]);
  	return sum;
}

double rastrigin_function(double* a, int n) {
  	double sum = 10 * n;
  	for (int i = 0; i < n; i++) 
    	sum += a[i] * a[i] - 10 * (cos(2 * M_PI * a[i]));
  	return sum;
}

double eval(double* a, int n, int function)
{
	double sum = 0;
  	if(function == 0) {
    	sum = sphere_function(a, n);
  	}
  	else if (function == 1) {
    	sum = rosenbrock_function(a, n);
  	} 
  	else 
    	sum = rastrigin_function(a, n);
	return sum;
}

void init(double* P, int m, int n) {
	for (int k = 0; k < m; k++) 
		for (int i = 0; i < n; i++) 
    		P[k * n + i] = gen(-100, 100);
}

void shuffle(double* P, int m, int n) {
  	for (int k = 0; k < m; k++) {
    	int l = rand() % m;
    	for (int i = 0; i < n; i++)
      		swap(P[k * n + i], P[l * n + i]);
  	}
}

void select(double* P, int m, int n) {
  	double pwin = 0.75;
  	shuffle(P, m, n);
 	for (int k = 0; k < m / 2; k++) {
    	int a = 2 * k;
    	int b = 2 * k + 1;
    	double fa = eval(P + a * n, n, 2);
    	double fb = eval(P + b * n, n, 2);

    	double p = frand();
    	if ((fa < fb && p < pwin) || (fa > fb && p > pwin))
      		for (int i = 0; i < n; i++)
        		P[b * n + i] = P[a * n + i];
    	else
      	for (int i = 0; i < n; i++)
        	P[a * n + i] = P[b * n + i];
  	}
}

void crossover(double* P, int m, int n) {
	shuffle(P, m, n);
  	for (int k = 0; k < m / 2; k++) {
    	int a = 2 * k;
    	int b = 2 * k + 1;
    	int j = rand() % n;
    	for (int i = j; i < n; i++)
      		swap(P[a * n + i], P[b * n + i]);
  	}
}

void mutate(double* P, int m, int n) {
  	double pmut = 0.1;
  	double mutation_parametr = 5;
  	for (int k = 0; k < m; k++) 
    	for (int i = 0; i < n; i++) 
      		if (frand() < pmut) {
        		P[k * n + i] += ((frand() * 2.0 ) - 1) * mutation_parametr;
        		P[k * n + i] = max(-100.0, min(100.0, P[k * n + i]));
      		}
}

double printthebest(double* P, int m, int n) {
	int k0 = -1;
  	double f0 = 1e12;
  	for (int k = 0; k < m; k++) {
    	double f = eval(P + k * n, n, 2);
    	if (f < f0) {
      		f0 = f;
      		k0 = k;
    	}
  	}
  	return f0;
}

void migrate(double* P, int m, int n, int rank, int size) 
{
	MPI_Status status;
	shuffle(P, m, n);
  	int next = (rank + 1) % size;
  	int prev = (rank - 1 + size) % size;
  	int mig = 25;

  	MPI_Sendrecv(P, mig * n, MPI_DOUBLE, next, 0, P, mig * n, MPI_DOUBLE, prev, 0, MPI_COMM_WORLD, &status);
 
  	MPI_Barrier(MPI_COMM_WORLD);

}

void runGA(int n, int m, int T, int dt, int rank, int size)
{
  	int local_m = m / size;
	double* P = new double [n*local_m];
	double a;

	init(P, local_m, n);
	
	double* all_messages = nullptr;
	if (rank == 0) 
      	all_messages = new double [(size - 1) * 2]; 

	for( int t=0; t<T; t++ )
	{   
    	if (t % dt == 0) 
      		migrate(P, local_m, n, rank, size); 

		select(P, local_m, n);
		crossover(P, local_m, n);
		mutate(P, local_m, n);
	    
    	a = printthebest(P, local_m, n);
      
    	MPI_Gather(&a, 1, MPI_DOUBLE, all_messages, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
		MPI_Barrier(MPI_COMM_WORLD);
    	if (rank == 0) {
			double best = all_messages[0];
			double sum = best;

      		for (int i = 1; i < size; i++) {
				if (all_messages[i] < best )
					best = all_messages[i];
				sum += all_messages[i];
			}
			cout << best << "             " << sum / size  <<endl;
      	}
      
	}
	delete[] all_messages;
	delete[] P;
}

int main(int argc, char** argv)
{
  MPI_Init(&argc, &argv);

  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  int n = 1000;
  int m = 1200;
  int T = 1100;
  int dt = 20;

  if (m % size != 0) {
    if (rank == 0) 
      cerr << "Не верное количество процессов " << endl;
    
    MPI_Finalize();
    return 1;
  }

  runGA(n, m, T, dt, rank, size);

  MPI_Finalize();
	return 0;
}
