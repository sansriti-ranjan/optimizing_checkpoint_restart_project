#!/bin/bash
cd $HOME/ECE8650/optimizing_checkpoint_restart_project/palmetto_test_scripts

echo $HOME
#SET TEST ID VARS
TEST_ID=$1
WORKING_DIR="$HOME/ECE8650/compress_dir/"
PERSIS_DIR="/scratch/$USER/persis"
LOG_PATH="$HOME/ECE8650/optimizing_checkpoint_restart_project/logs/$TEST_ID-$PBS_JOBID-$NCPUS-$PPN-"`date '+%d-%m'`
STATS="$TEST_ID-$PBS_JOBID-$NCPUS-$PPN-"`date '+%d-%m'`".txt"
mkdir -p $WORKING_DIR
mkdir -p $PERSIS_DIR
mkdir -p $LOG_PATH
mkdir -p $TMPDIR/veloc
mkdir -p /dev/shm/veloc
mkdir -p /dev/shm/logs

#SET CONTROL VARIABLES
declare -a NUM_NODES=(1 8 16 32)
declare -a CKPT_FREQS=(1 6 12 60)
declare -a STORAGE_DEV=("/dev/shm" "$TMPDIR")

strat=FILE_PER_PROC
for it in {0..0}
do
    for ckpt_freq in ${CKPT_FREQS[@]}
    do
        for device in ${STORAGE_DEV[@]}
        do
            sed -i "s+scratch = .*+scratch = $device/veloc+" veloc.cfg
            sed -i "s+persistent = .*+persistent= $PERSIS_DIR+" veloc.cfg
            export LOCAL_STORAGE_DEV=$device
            for nodes in ${NUM_NODES[@]}
            do
                #GET FILE SYSTEM STATUS
                #whatsfree >> whatsfree.txt
                export PPN=8
                export NODES_TOTAL=$(awk '/TOTAL NODES/ {print $3}' whatsfree.txt)
                export NODES_FREE=$(awk '/TOTAL NODES/ {print $9}' whatsfree.txt)
                export NODES_OFFLINE=$(awk '/TOTAL NODES/ {print $12}' whatsfree.txt)
                procs=$((nodes*PPN))
                export NUM_FILES=$procs

                #CLEAR
                rm -f $PERSIS_DIR/*
                rm -rf $WORKING_DIR/*
                rm -f $device/veloc/heatdis*
                rm -f $device/logs/*
                rm -f $TMPDIR/veloc/*
                rm /dev/shm/veloc-*

                echo "starting heat distribution test with $nodes nodes ($procs procs), file = $STATS, strat = $strat, feq = $ckpt_freq, PPN = $PPN" >> /dev/stderr
                mpirun -n ${procs} -npernode ${PPN} -x MPICH_MAX_THREAD_SAFETY=multiple ./heatdis_og 100 veloc.cfg $STATS $strat $ckpt_freq
                du -h $PERSIS_DIR >> /dev/stderr

                echo "done....zipping and moving log files..." >> /dev/stderr
                mv /dev/shm/logs/* $WORKING_DIR
                date=`date '+%d-%m_%H-%M'`
                log_filename="$TEST_ID-$strat-$NUM_FILES-$nodes-$PPN-$procs-$date.tgz"
                tar -czf $log_filename $WORKING_DIR
                mv $log_filename $LOG_PATH
            done
        done
    done
done

mv $stats_file $LOG_PATH
rm -f $TMPDIR/veloc/*
rm -f $TMPDIR/logs/*
rm -f dev/shm/veloc/*
rm -f dev/shm/logs/*
rm -d $TMPDIR/veloc
rm -d /dev/shm/veloc
rm -d /dev/shm/logs

