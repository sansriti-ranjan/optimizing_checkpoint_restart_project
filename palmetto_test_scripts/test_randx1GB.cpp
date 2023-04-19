#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <string>
#include "/home/sansrir/VELOC/veloc_install/include/veloc.h"
#include <mpi.h>
#include <fstream>
#include <iostream>
// this examples uses asserts so they need to be activated
#undef NDEBUG
#include <assert.h>


const size_t PROC_SIZE = 1 << 30;

int main(int argc, char *argv[]) {    
	std::cout << "starting test.." << std::endl;
    std::string filename = argv[2];
    std::string strategy = argv[3];
    int tol = atoi(argv[4]);
    int num_threads = atoi(argv[5]);
    double dec_tol = (double)tol/100; //dec representation
    int NUM_FILES = atoi(getenv("NUM_FILES"));	
    int provided, N, rank;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    assert(provided == MPI_THREAD_MULTIPLE);
    MPI_Comm_size(MPI_COMM_WORLD, &N);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);    

    if(rank == 0){
        std::cout << "tolerance selected: " << tol << ", dec=" << dec_tol << std::endl;
        std::cout << "N = " << N << " PROC_SIZE = " << PROC_SIZE << std::endl;
    }
    ssize_t total = N * PROC_SIZE;
    ssize_t rank_size[N]; 
    srand48(112244L);
    for (int i = 0; i < N; i++) {
        rank_size[i] = (ssize_t)ceil(PROC_SIZE * (1 + (drand48() - 0.5) * 2 * dec_tol));
        total -= (ssize_t)rank_size[i];
    }
    
    for (int i = 0; i < N; i++) {
        rank_size[i] += (ssize_t)(total / N);
        assert(rank_size[i] > 0);
    }
    size_t total_size = 0;
    for (int i = 0; i < N; i++) 
        total_size += rank_size[i];
    
    if(rank == 0)
        std::cout << "total_size = " << (double)total_size / (1 << 30) << " GiB" << std::endl;
    
    
    char *ptr = (char *)malloc(rank_size[rank] * sizeof(char));
    assert(ptr != NULL);
    memset(ptr, (char)rank, rank_size[rank]);
    MPI_Barrier(MPI_COMM_WORLD);
	if(rank == 0)
		std::cout << "all ranks allocated mem, initializing VeloC.." << std::endl;
    if(VELOC_Init(MPI_COMM_WORLD, argv[1]) != VELOC_SUCCESS) {
	    printf("Error initializing VELOC! Aborting...\n");
		exit(2);
    }
	std::cout << "rank " << rank << " done, protecting mem" << std::endl;
    VELOC_Mem_protect(rank, ptr, rank_size[rank], sizeof(char));
    MPI_Barrier(MPI_COMM_WORLD);
	std::cout << "rank " << rank << " starting checkpoint" << std::endl;
    double start = MPI_Wtime();
    assert(VELOC_Checkpoint("veloc_test", N) == VELOC_SUCCESS);
    double local_end = MPI_Wtime();
    assert(VELOC_Checkpoint_wait() == VELOC_SUCCESS);
    double flush_end = MPI_Wtime() - local_end;
    local_end -= start;

    double avgLocalTime, maxLocalTime, avgFlushTime, maxFlushTime;
    MPI_Reduce(&local_end, &avgLocalTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&flush_end, &avgFlushTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_end, &maxLocalTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&flush_end, &maxFlushTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if(rank == 0)
    {
        std::ofstream outputfile;
        outputfile.open(filename.c_str(), std::ofstream::out | std::ofstream::app);
        
	outputfile << strategy << ",local," << tol << "," << NUM_FILES << "," << N/8 << "," << N << "," << num_threads << 
	"," << total_size << "," << avgLocalTime / N << "," << maxLocalTime << "," << (total_size / (1 << 30)) / (avgLocalTime / N) << 
	"," << (total_size / (1 << 30)) / maxLocalTime << "\n";
        
	outputfile << strategy << ",total," << tol << "," << NUM_FILES << "," << N/8 << "," << N << "," << num_threads << 
	"," << total_size << "," << avgFlushTime / N << "," << maxFlushTime << "," << (total_size / (1 << 30)) / (avgFlushTime / N) << 
	"," << (total_size / (1 << 30)) / maxFlushTime << "\n";
        
	outputfile.close();
    }    
    free(ptr);
    VELOC_Finalize(0); // no clean up
    MPI_Finalize();
    return 0;
}
