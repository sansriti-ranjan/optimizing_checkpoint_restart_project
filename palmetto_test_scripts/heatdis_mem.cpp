#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <string>
#include "/home/sansrir/VELOC/veloc_install/include/veloc.h"
#include <mpi.h>
#include <fstream>
#include <iostream>
#include "heatdis.h"
#include <assert.h>
#include <unistd.h>

/*
    This sample application is based on the heat distribution code
    originally developed within the FTI project: github.com/leobago/fti
*/

static const ssize_t PROC_SIZE = (1 << 30);

void initData(int nbLines, int M, int rank, double *h) {
    int i, j;
    for (i = 0; i < nbLines; i++) {
        for (j = 0; j < M; j++) {
            h[(i*M)+j] = 0;
        }
    }
    if (rank == 0) {
        for (j = (M*0.1); j < (M*0.9); j++) {
            h[j] = 100;
        }
    }
}

double doWork(int numprocs, int rank, int M, int nbLines, double *g, double *h) {
    int i,j;
    MPI_Request req1[2], req2[2];
    MPI_Status status1[2], status2[2];
    double localerror;
    localerror = 0;
    for(i = 0; i < nbLines; i++) {
        for(j = 0; j < M; j++) {
            h[(i*M)+j] = g[(i*M)+j];
        }
    }
    if (rank > 0) {
        MPI_Isend(g+M, M, MPI_DOUBLE, rank-1, WORKTAG, MPI_COMM_WORLD, &req1[0]);
        MPI_Irecv(h,   M, MPI_DOUBLE, rank-1, WORKTAG, MPI_COMM_WORLD, &req1[1]);
    }
    if (rank < numprocs - 1) {
        MPI_Isend(g+((nbLines-2)*M), M, MPI_DOUBLE, rank+1, WORKTAG, MPI_COMM_WORLD, &req2[0]);
        MPI_Irecv(h+((nbLines-1)*M), M, MPI_DOUBLE, rank+1, WORKTAG, MPI_COMM_WORLD, &req2[1]);
    }
    if (rank > 0) {
        MPI_Waitall(2,req1,status1);
    }
    if (rank < numprocs - 1) {
        MPI_Waitall(2,req2,status2);
    }
    for(i = 1; i < (nbLines-1); i++) {
        for(j = 0; j < M; j++) {
            g[(i*M)+j] = 0.25*(h[((i-1)*M)+j]+h[((i+1)*M)+j]+h[(i*M)+j-1]+h[(i*M)+j+1]);
            if(localerror < fabs(g[(i*M)+j] - h[(i*M)+j])) {
                localerror = fabs(g[(i*M)+j] - h[(i*M)+j]);
            }
        }
    }
    if (rank == (numprocs-1)) {
        for(j = 0; j < M; j++) {
            g[((nbLines-1)*M)+j] = g[((nbLines-2)*M)+j];
        }
    }
    return localerror;
}

int main(int argc, char *argv[]) {
    std::string filename = argv[3];
    std::string strategy = argv[4];
    int FREQ_MOD = std::stoi(argv[5]);

    static const unsigned int CKPT_INT = ITER_TIMES / FREQ_MOD;
    static const int CKPT_FREQ = ITER_TIMES / CKPT_INT;
    int rank, N, nbLines, i, M, arg;
    double wtime, *h, *g, memSize, localerror, globalerror = 1;

    if (argc < 3) {
	printf("Usage: %s <mem_in_mb> <cfg_file>\n", argv[0]);
	exit(1);
    }

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &N);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (sscanf(argv[1], "%d", &arg) != 1) {
        printf("Wrong memory size! See usage\n");
	exit(3);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    if (VELOC_Init(MPI_COMM_WORLD, argv[2]) != VELOC_SUCCESS) {
	printf("Error initializing VELOC! Aborting...\n");
	exit(2);
    }

    M = (int)sqrt((double)(arg * 1024.0 * 1024.0 * N) / (2 * sizeof(double))); // two matrices needed
    nbLines = (M / N) + 3;
    h = (double *) malloc(sizeof(double *) * M * nbLines);
    g = (double *) malloc(sizeof(double *) * M * nbLines);
    initData(nbLines, M, rank, g);
    memSize = M * nbLines * 2 * sizeof(double) / (1024 * 1024);

    if (rank == 0)
	printf("Local data size is %d x %d = %f MB (%d).\n", M, nbLines, memSize, arg);
    if (rank == 0)
	printf("Target precision : %f \n", PRECISION);
    if (rank == 0)
	printf("Maximum number of iterations : %d \n", ITER_TIMES);

    VELOC_Mem_protect(0, &i, 1, sizeof(int));
    VELOC_Mem_protect(1, h, M * nbLines, sizeof(double));
    VELOC_Mem_protect(2, g, M * nbLines, sizeof(double));

    i = 0;
    double ckpt_timer = 0;
    double work_timer = 0;
    double interval = 0;
    double last_ckpt_end = 0;
    int ckpt_num = 0;
    double start = MPI_Wtime();
    while(i < ITER_TIMES) {
        double work_start = MPI_Wtime();
        localerror = doWork(N, rank, M, nbLines, g, h);
        work_timer += (MPI_Wtime() - work_start);
        if (((i % ITER_OUT) == 0) && (rank == 0))
            printf("Step : %d, error = %f\n", i, globalerror);
        if ((i % REDUCE) == 0)
            MPI_Allreduce(&localerror, &globalerror, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        if (globalerror < PRECISION)
            break;
        i++;
        if (i % CKPT_FREQ == 0){
            printf("starting checkpoint i = %d\n",i);
            double ckpt_start = MPI_Wtime();
            if (VELOC_Checkpoint("heatdis", i) != VELOC_SUCCESS) {
                printf("Error checkpointing! Aborting...\n");
                exit(2);
            }
            ckpt_timer += (MPI_Wtime() - ckpt_start);
            interval += (ckpt_start - last_ckpt_end);
            ckpt_num++;
            last_ckpt_end = MPI_Wtime();
        }
    }
    assert(VELOC_Checkpoint_wait() == VELOC_SUCCESS);
    interval /= ckpt_num;
    double ckpt_total = MPI_Wtime() - start;
    double avgTime = 0, maxTime = 0;
    double avgWork = 0, maxWork = 0;
    double AVG_INT = 0;
    MPI_Reduce(&ckpt_timer, &avgTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&ckpt_timer, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&interval, &AVG_INT, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&work_timer, &avgWork, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&work_timer, &maxWork, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if (rank == 0){
	    std::ofstream outputfile;
        outputfile.open(filename.c_str(), std::ofstream::out | std::ofstream::app);
        int NODES_TOTAL = std::stoi(std::getenv("NODES_TOTAL"));
        int NODES_FREE = std::stoi(std::getenv("NODES_FREE"));
        int NODES_OFFLINE = std::stoi(std::getenv("NODES_OFFLINE"));
        std::string local_storage_dev = std::getenv("LOCAL_STORAGE_DEV");
        int PROCS_PER_NODE = std::stoi(std::getenv("PPN"));
        int NUM_FILES = std::stoi(std::getenv("NUM_FILES"));
        AVG_INT /= ckpt_num;

        outputfile.seekp(0, std::ios::end);  
        if (outputfile.tellp() == 0) {    
            outputfile << "STRATEGY,STORAGE DEV,N PROCS,PROCS PER NODE,N NODES,AGG SIZE,LOCAL SIZE,AVG CKPT TIME,MAX CKPT TIME," <<
            "AVG CKPT THROUGHPUT,MAX CKPT THROUGHPUT,AVG WORK,MAX WORK,NUM ITERATIONS,CKPT FREQ,AVG CKPT INTERVAL,SYSTEM NODES," <<
            "NODES IN USE,NODES OFFLINE" << std::endl;

        }
        outputfile << strategy << "," << local_storage_dev << "," << N << "," << PROCS_PER_NODE << "," << N/PROCS_PER_NODE << "," << NUM_FILES << "," <<
        arg * N  << "," << memSize << "," << avgTime/N << "," << maxTime << "," << ((double)arg / (double)PROC_SIZE) / (avgTime/N) << "," << 
        ((double)arg / (double)PROC_SIZE) / maxTime << "," << avgWork/N << "," << maxWork << "," << 
        ITER_TIMES << "," << CKPT_FREQ << "," << CKPT_INT << "," << AVG_INT << ","  << 
        NODES_TOTAL << "," << NODES_TOTAL - NODES_FREE - NODES_OFFLINE << "," << NODES_OFFLINE << "\n";
    
        
        outputfile.close();
        outputfile.close();
    }

    free(h);
    free(g);
    VELOC_Finalize(1); // wait for checkpoints to finish
    MPI_Finalize();
    return 0;
}
