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
// this examples uses asserts so they need to be activated
#undef NDEBUG
#include <assert.h>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

const size_t PROC_SIZE = 1 << 30;

std::mutex output_mutex;

bool file_transfer_loop(int fs, ssize_t soff, int fd, ssize_t doff, ssize_t remaining) {
    const size_t MAX_BUFF_SIZE = 1 << 24;
    bool success = true;
    char *buff = new char[MAX_BUFF_SIZE];
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
    srand48(112244L);
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
    bool aggregated = false;
	po::options_description desc("options");
	desc.add_options()
		("aggregated,a", po::value<bool>(&aggregated), "set aggregation flag");
	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);
	if(vm.count("aggregated"))
		aggregated = true;


    double dec_tol = (double)tol/100; //dec representation 
    const size_t MAX_BUFF_SIZE = 1 << 26;
    ssize_t rank_size[t_count] = {0}; 
    std::cout << "starting test with tol = " << dec_tol << ", and " << t_count << " threads..." << std::endl;
    #pragma omp parallel for num_threads(t_count)
    for (int i = 0; i < t_count; i++) {
        generate_sizes(t_count, dec_tol, rank_size);
        char *buff = new char[rank_size[omp_get_thread_num()]];
	char *buff_ptr = buff;
        assert(buff != NULL);
        memset(buff, (char)omp_get_thread_num(), rank_size[omp_get_thread_num()]);
        std::string fs = "/dev/shm/veloc/"+filename+std::to_string(omp_get_thread_num())+".dat";
        int fi = open(fs.c_str(), O_CREAT | O_WRONLY, 0644);
        if (fi == -1) {
	    close(fi);
	    exit(-1);
        }
        ssize_t remaining = rank_size[omp_get_thread_num()];
        ssize_t off = 0;
        bool success = true;
        while(remaining > 0){
            size_t transfer = std::min(MAX_BUFF_SIZE, (size_t)remaining);
            if(pwrite(fi, buff+off, transfer, (off_t)off) != transfer) {
                success = false;                    
                break;
            }
            remaining -= (ssize_t)transfer;
            off += (ssize_t)transfer;
        }
        close(fi);
        //delete[] buff_ptr;
	if(!success)
	    exit(-1);
    }

    	double agg_throughput = 0;
	#pragma omp parallel for num_threads(t_count) reduction(+:agg_throughput)
    for (int i = 0; i < t_count; i++) {
	int t_id = omp_get_thread_num();
        std::string fs = "/dev/shm/veloc/"+filename+std::to_string(t_id)+".dat";
        int fi = open(fs.c_str(), O_CREAT | O_RDONLY, 0644);
        if (fi == -1) {
	        close(fi);
	        exit(-1);
        }
	std::string fd = "/scratch/mikailg/persis/"+filename+std::to_string(t_id)+".dat";
	ssize_t fo_off = 0;
	if(aggregated) {
        	fd = "/scratch/mikailg/persis/"+filename+"-aggregated.dat";
		fo_off = get_offset(t_id, rank_size);
	}
        int fo = open(fd.c_str(), O_CREAT | O_WRONLY, 0644);
        if (fo == -1) {
	        close(fo);
	        exit(-1);
        }
        ssize_t thr_size = rank_size[t_id];
        double start = omp_get_wtime();
        int ret = file_transfer_loop(fi, 0, fo, fo_off, thr_size);
        double timer = omp_get_wtime() - start;
		double thruput = (double)(thr_size/(1e9)/timer);
	std::unique_lock<std::mutex> cout_lck(output_mutex);
        std::cout << "openMP thread " << t_id  << " wrote " << thr_size << " bytes at offset " << fo_off << " in " << timer << " [s], " << thruput << " GB/s" << std::endl;
        agg_throughput += thruput;
	close(fi);
        close(fo);
    }

	std::cout << "Aggregated Throughput = " << agg_throughput << std::endl;
    return 0;
}

