#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <string>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <omp.h>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <vector>
#include <new>
// this examples uses asserts so they need to be activated
#undef NDEBUG
#include <assert.h>

const size_t PROC_SIZE = 1 << 30;

std::mutex output_mutex, heap_mutex;

bool file_transfer_loop(int fs, ssize_t soff, int fd, ssize_t doff, ssize_t remaining) {
    const size_t MAX_BUFF_SIZE = 1 << 24;
    bool success = true;
    unsigned char *buff = new unsigned char[MAX_BUFF_SIZE];
    while (remaining > 0) {
	ssize_t buff_rem = (doff % MAX_BUFF_SIZE) != 0 ? MAX_BUFF_SIZE - (doff % MAX_BUFF_SIZE) : MAX_BUFF_SIZE;
	ssize_t transferred = pread(fs, buff, std::min((size_t)buff_rem, (size_t)remaining), soff);
        if (transferred == -1 || pwrite(fd, buff, transferred, doff) != transferred) {
            success = false;
            break;
        }
        remaining -= transferred;
	soff += transferred;
	doff += transferred;
    }
    delete []buff;
    return success;
}

void generate_sizes(int t_count, double tol, ssize_t *rank_size){
    ssize_t total = t_count * PROC_SIZE;
    srand48(0);
    for (int i = 0; i < t_count; i++) {
        rank_size[i] = (ssize_t)ceil(PROC_SIZE * (1 + (drand48() - 0.5) * 2 * tol));
	total -= (ssize_t)rank_size[i];
    }
    for (int i = 0; i < t_count; i++) {
        rank_size[i] += (ssize_t)(total / t_count);
        assert(rank_size[i] > 0);
    }
}

ssize_t get_offset(int t_id, ssize_t *rank_size){
    ssize_t off = 0;
    for(int i = 0; i < t_id; i++)
	off += rank_size[i];
    return off;
}

int main(int argc, char *argv[]) {
    std::string filename = argv[1];
    int tol = atoi(argv[2]);
    int t_count = atoi(argv[3]);
    double dec_tol = (double)tol/100; //dec representation 
    const size_t MAX_BUFF_SIZE = 1 << 26;
    ssize_t rank_size[t_count] = {0}; 
    generate_sizes(t_count, dec_tol, rank_size);
    std::cout << "starting test with tol = " << dec_tol << ", and " << t_count << " threads..." << std::endl;
    #pragma omp parallel for num_threads(t_count)
    for (int i = 0; i < t_count; i++) {
        // std::unique_lock<std::mutex> cout_lck(output_mutex);
        // std::cout << "thread = " << omp_get_thread_num() << " creating buff of size " << rank_size[omp_get_thread_num()] << std::endl;
        unsigned char *buff = new unsigned char[rank_size[omp_get_thread_num()] * sizeof(unsigned char)];
        assert(buff != NULL);
        // std::cout << "thread = " << omp_get_thread_num() << " initiating memset of size " << static_cast<size_t>(rank_size[omp_get_thread_num()]) << std::endl;
        memset(buff, 0, static_cast<size_t>(rank_size[omp_get_thread_num()]));
        // std::cout << "thread " << omp_get_thread_num() << " done" << std::endl;
        // cout_lck.unlock();
        std::string fs = "/dev/shm/veloc/"+filename+std::to_string(omp_get_thread_num())+".dat";
        int fi = open(fs.c_str(), O_CREAT | O_WRONLY, 0644);
        if (fi == -1) {
            close(fi);
            delete[] buff;
            exit(-1);
        }
        ssize_t remaining = rank_size[omp_get_thread_num()];
        ssize_t off = 0;
        bool success = true;
        while(remaining > 0){
            size_t transfer = std::min(MAX_BUFF_SIZE, (size_t)remaining);
            if(pwrite(fi, buff, transfer, (off_t)off) != (ssize_t)transfer) {
                success = false;                    
                break;
            }
            remaining -= (ssize_t)transfer;
            off += (ssize_t)transfer;
        }
        close(fi);
	    if(!success)
	    exit(-1);
    }

    #pragma omp barrier


    std::vector<double> thruputs(t_count);
    #pragma omp parallel for num_threads(t_count)
    for (int i = 0; i < t_count; i++) {
	int t_id = omp_get_thread_num();
        std::string fs = "/dev/shm/veloc/"+filename+std::to_string(t_id)+".dat";
        int fi = open(fs.c_str(), O_CREAT | O_RDONLY, 0644);
        if (fi == -1) {
	        close(fi);
	        exit(-1);
        }
        std::string fd = "/scratch/mikailg/persis/"+filename+"-aggregated.dat";
        int fo = open(fd.c_str(), O_CREAT | O_WRONLY, 0644);
        if (fo == -1) {
	        close(fo);
	        exit(-1);
        }
        ssize_t thr_size = rank_size[t_id];
        ssize_t fo_off = get_offset(t_id, rank_size);
        double start = omp_get_wtime();
        int ret = file_transfer_loop(fi, 0, fo, fo_off, thr_size);
        double timer = omp_get_wtime() - start;
        thruputs[t_id] = (double)(thr_size/(1e9)/timer);
	    std::unique_lock<std::mutex> cout_lck(output_mutex);
        std::cout << "openMP thread " << t_id  << " wrote " << thr_size << " bytes at offset " << fo_off << " in " << timer << " [s], " << thruputs[t_id] << " GB/s" << std::endl;
        close(fi);
        close(fo);
    }
    double agg_tp = 0;
    for (auto& t : thruputs)
        agg_tp += t;
    std::cout << "Aggregate throughput = " << (double)agg_tp << std::endl;
    return 0;
}

