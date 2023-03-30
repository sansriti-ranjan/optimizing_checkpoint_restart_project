#!/bin/bash
#PBS -N veloc_and_vast_testing 
#PBS -l select=8:ncpus=20:mpiprocs=20:mem=125gb:phase=11a,walltime=03:00:00,place=scatter

cd /home/mikailg/ECE8730
export LD_LIBRARY_PATH=/home/mikailg/VELOC/install/lib64:$LD_LIBRARY_PATH
export PATH=/home/mikailg/VELOC/install/bin:$PATH

#set bash variables
PPN=8
test_identifier="original_comp_testing"
WORKING_DIR="/home/mikailg/ECE8730/work_dir/"
PERSIS_DIR="/scratch/mikailg/persis"
LOG_PATH="/home/mikailg/ECE8730/logs/$test_identifier-$PBS_JOBID-$NCPUS-$PPN-"`date '+%d-%m'`
stats_file="$test_identifier-$PBS_JOBID-$NCPUS-$PPN-"`date '+%d-%m'`".txt"

sed -i "s+scratch = .*+scratch = $TMPDIR+" veloc.cfg

mkdir -p $LOG_PATH
mkdir -p $TMPDIR/veloc
mkdir -p $TMPDIR/logs
declare -a NUM_NODES=(8)


strat=ORIGINAL
for it in {0..2}
do
    for tol in {0..20..10}
    do
        for nodes in ${NUM_NODES[@]}
        do
            procs=$((nodes*PPN))
	    export NUM_FILES=$procs
            rm -f $PERSIS_DIR/*
            rm -rf $WORKING_DIR/*
	    rm -rf $TMPDIR/veloc/*

            echo "starting aggregated test with $nodes nodes ($procs procs), $tol % tolerance, NUM_FILES=$NUM_FILES" >> /dev/stderr
            mpirun -n ${procs} -npernode ${PPN} -x MPICH_MAX_THREAD_SAFETY=multiple -x LD_PRELOAD=/lib64/libSegFault.so ./benchmark_og veloc.cfg $stats_file $strat $tol $procs
            du -h $PERSIS_DIR >> /dev/stderr

            echo "done....zipping and moving files..." >> /dev/stderr
            mv $TMPDIR/logs/* $WORKING_DIR
	    date=`date '+%d-%m_%H-%M'`
            log_filename="$test_identifier-$strat-$tol-$NUM_FILES-$nodes-$PPN-$procs-$date.tgz"
            tar -czf $log_filename $WORKING_DIR
            mv $log_filename $LOG_PATH
        done
    done
done

mv $stats_file $LOG_PATH
rm -f $TMPDIR/veloc/*
rm -f $TMPDIR/logs/*
rm -d $TMPDIR/veloc
rm -d $TMPDIR/logs

