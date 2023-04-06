CC = gcc
CXX = g++

MPICC = mpicc
MPICXX = mpicxx

MPI_CFLAGS := -g -O3 $(OPENMPFLAG) $(G99FLAG)
MPI_CFLAGS := -g -O3 $(OPENMPFLAG)
MPI_CPPFLAGS := $(BASE_CPPFLAGS)

VELOC_INC=$(HOME)/VELOC/install/include/
VELOC_LIB=$(HOME)/VELOC/install/lib64/

VELOC_AG_INC=$(HOME)/CKPT_AGGR/install/include
VELOC_AG_LIB=$(HOME)/CKPT_AGGR/install/lib64

all: benchmark_og heatdis_og benchmark_ag heatdis_ag omp_test omp_interleave

benchmark_og:
	mpic++ $(MPI_CFLAGS) -I$(VELOC_INC) -std=c++11 benchmark.cpp -o benchmark_og -L$(VELOC_LIB) -lveloc-client

heatdis_og:
	mpic++ $(MPI_CFLAGS) -I$(VELOC_INC) -std=c++11 heatdis_mem.cpp -o heatdis_og -L$(VELOC_LIB) -lveloc-client

benchmark_ag:
	mpic++ $(MPI_CFLAGS) -I$(VELOC_AG_INC) -std=c++11 test_randx1GB.cpp -o benchmark_ag -L$(VELOC_AG_LIB) -lveloc-client

heatdis_ag:
	mpic++ $(MPI_CFLAGS) -I$(VELOC_AG_INC) -std=c++11 heatdis_mem.cpp -o heatdis_ag -L$(VELOC_AG_LIB) -lveloc-client

omp_test:
	$(CXX) -g -O3 -fopenmp -std=c++11 test_write_omp.cpp -o omp_test -lboost_program_options

omp_interleave:
	$(CXX) -g -O3 -fopenmp -std=c++11 test_interleave_write_omp.cpp -o omp_interleave_test


clean:
	rm -f benchmark_og heatdis_og benchmark_ag heatdis_ag omp_test omp_interleave_test
