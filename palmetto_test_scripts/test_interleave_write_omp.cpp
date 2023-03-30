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

// this examples uses asserts so they need to be activated
#undef NDEBUG
#include <assert.h>

const size_t PROC_SIZE = 1 << 30;

bool file_transfer_loop(void *buff, int fd, ssize_t doff, ssize_t remaining) {
    const size_t MAX_BUFF_SIZE = 1 << 26;
    bool success = true;
    while (remaining > 0) {
        ssize_t transferred = pwrite(fd, buff, remaining, doff);
	if(transferred != remaining) {
            success = false;
            break;
        }
        remaining -= transferred;
	doff += transferred;
    }
    return success;
}

int main(int argc, char *argv[]) {
    std::string filename = argv[1];
    int t_count = atoi(argv[2]);
    const size_t MAX_BUFF_SIZE = 1 << 26;
    int tol = atoi(argv[3]);
    std::cout << "starting interleaved write test with " << t_count << " threads, " << MAX_BUFF_SIZE << " size buffers" << std::endl;
    #pragma omp parallel for num_threads(t_count)
    for (int i = 0; i < t_count; i++) {
	std::string fd = "/lus/grand/projects/VeloC/mikailg/persis/"+filename+"-aggregated.dat";
        int fo = open(fd.c_str(), O_CREAT | O_WRONLY, 0644);
        if (fo == -1) {
                close(fo);
                exit(-1);
        }
        char *buff = new char[MAX_BUFF_SIZE];
        assert(buff != NULL);
        memset(buff, (char)omp_get_thread_num(), MAX_BUFF_SIZE);
        ssize_t remaining = PROC_SIZE;
	int num_passes = 0;
        bool success = true;
	double start = omp_get_wtime();
        while(remaining > 0 && success != false){
	    ssize_t off = MAX_BUFF_SIZE * (omp_get_thread_num() + (num_passes * t_count));
            size_t transfer = std::min(MAX_BUFF_SIZE, (size_t)remaining);
	    //if(omp_get_thread_num() == 0)
		//std::cout << "thread 0 writing " << transfer << " bytes at offset " << off << std::endl;
	    success = file_transfer_loop(buff, fo, off, transfer);
	    remaining -= transfer;
	    num_passes++;
        }
	double stop = omp_get_wtime() - start;
	std::cout << "thread " << omp_get_thread_num() << " wrote " << PROC_SIZE / (1 <<30) << " GB in " << stop <<" sec; BW = " << ((PROC_SIZE / (1 <<30))/stop) << " GB/s" << std::endl;
        close(fo);
        delete[] buff;
    }
	
    return 0;
}

