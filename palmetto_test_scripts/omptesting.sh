for it in {0..2}
do
	for tol in {0..20..10}
	do
		rm /dev/shm/veloc/*
		rm /lus/grand/projects/VeloC/mikailg/persis/*
		./omp_test tolerance-$tol $tol 8
	done
done
