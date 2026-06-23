MPICXX=mpicxx
SPEEDFLAGS=-O3
CXXFLAGS=-Wall $(SPEEDFLAGS) -fopenmp -std=c++11
CXXFLAGS_MPI=-Wall $(SPEEDFLAGS) -std=c++11 -fopenmp
LDFLAGS=$(SPEEDFLAGS) -fopenmp
LDFLAGS_MPI=$(SPEEDFLAGS) -fopenmp
TARGET = gen_valid_info validation gen_RMAT gen_random solution_mpi
all: $(TARGET)
solution_mpi: main_mpi.o solution_mpi.o graph_tools.o gen_RMAT_mpi.o gen_random_mpi.o
	$(MPICXX) $^ -o $@ $(LDFLAGS_MPI)
gen_RMAT: gen_RMAT.o graph_tools.o
	$(MPICXX) $^ -o $@ $(LDFLAGS)
gen_random: gen_random.o graph_tools.o
	$(MPICXX) $^ -o $@ $(LDFLAGS)
gen_valid_info: gen_valid_info.o reference_bfs.o graph_tools.o
	$(MPICXX) $^ -o $@ $(LDFLAGS)
validation: validation.o
	$(MPICXX) $^ -o $@ $(LDFLAGS)
solution_mpi.o: solution_mpi.cpp
	$(MPICXX) $(CXXFLAGS_MPI) -o $@ -c $<
main_mpi.o: main_mpi.cpp
	$(MPICXX) $(CXXFLAGS_MPI) -o $@ -c $<
gen_RMAT_mpi.o: gen_RMAT_mpi.cpp
	$(MPICXX) $(CXXFLAGS_MPI) -o $@ -c $<
gen_random_mpi.o: gen_random_mpi.cpp
	$(MPICXX) $(CXXFLAGS_MPI) -o $@ -c $<
.cpp.o:
	$(MPICXX) $(CXXFLAGS) -o $@ -c $<
clean:
	rm -rf *.o $(TARGET)