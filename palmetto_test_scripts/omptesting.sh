for it in {0..2}
do
	for tol in {0..20..10}
	do
		rm /dev/shm/veloc/*
		rm /scratch/mikailg/persis/*
		./omp_test tolerance-$tol $tol 8 -a true
	done
done
