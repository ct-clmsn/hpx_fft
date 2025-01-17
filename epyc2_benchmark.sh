#!/bin/bash
################################################################################
# Benchmark script for ipvs-epyc2 (dual-socket AMD EPYC 7742)
################################################################################
if [[ "$1" == "strong" ]]
then
    # strong
    mkdir result
    mkdir plans
    BASE_SIZE=16384
    LOOP=5
    FFTW_PLAN=measure
    # Compute benchmark script from 2^start to 2^stop
    POW_START=1
    POW_STOP=7
    ###############################
    # HPX loop
    # shared
    ./build/fft_hpx_loop_shared --hpx:threads=1 --nx=$BASE_SIZE --ny=$BASE_SIZE --run=par --header=true --plan=$FFTW_PLAN
    for (( j=1; j<$LOOP; j=j+1 ))
    do
        ./build/fft_hpx_loop_shared --hpx:threads=1 --nx=$BASE_SIZE --ny=$BASE_SIZE --run=par --plan=$FFTW_PLAN
    done
    for (( i=2**$POW_START; i<=2**$POW_STOP; i=i*2 ))
    do
        for (( j=0; j<$LOOP; j=j+1 ))
        do
            ./build/fft_hpx_loop_shared --hpx:threads=$i --nx=$BASE_SIZE --ny=$BASE_SIZE --run=par --plan=$FFTW_PLAN
        done
    done
    # scatter
    ./build/fft_hpx_loop --hpx:threads=1 --nx=$BASE_SIZE --ny=$BASE_SIZE --run=scatter --header=true --plan=$FFTW_PLAN
    for (( j=1; j<$LOOP; j=j+1 ))
    do
        ./build/fft_hpx_loop --hpx:threads=1 --nx=$BASE_SIZE --ny=$BASE_SIZE --run=scatter --plan=$FFTW_PLAN
    done
    for (( i=2**$POW_START; i<=2**$POW_STOP; i=i*2 ))
    do
        for (( j=0; j<$LOOP; j=j+1 ))
        do
            ./build/fft_hpx_loop --hpx:threads=$i --nx=$BASE_SIZE --ny=$BASE_SIZE --run=scatter --plan=$FFTW_PLAN
        done
    done
    ##############################
    HPX future
    # shared sync
    ./build/fft_hpx_task_sync_shared --hpx:threads=1 --nx=$BASE_SIZE --ny=$BASE_SIZE --header=true --plan=$FFTW_PLAN
    for (( j=1; j<$LOOP; j=j+1 ))
    do
        ./build/fft_hpx_task_sync_shared --hpx:threads=1 --nx=$BASE_SIZE --ny=$BASE_SIZE --plan=$FFTW_PLAN
    done
    for (( i=2**$POW_START; i<=2**$POW_STOP; i=i*2 ))
    do
        for (( j=0; j<$LOOP; j=j+1 ))
        do
            ./build/fft_hpx_task_sync_shared --hpx:threads=$i --nx=$BASE_SIZE --ny=$BASE_SIZE --plan=$FFTW_PLAN
        done
    done
    # shared optimized version
    ./build/fft_hpx_task_shared --hpx:threads=1 --nx=$BASE_SIZE --ny=$BASE_SIZE --header=true --plan=$FFTW_PLAN
    for (( j=1; j<$LOOP; j=j+1 ))
    do
        ./build/fft_hpx_task_shared --hpx:threads=1 --nx=$BASE_SIZE --ny=$BASE_SIZE --plan=$FFTW_PLAN
    done
    for (( i=2**$POW_START; i<=2**$POW_STOP; i=i*2 ))
    do
        for (( j=0; j<$LOOP; j=j+1 ))
        do
            ./build/fft_hpx_task_shared --hpx:threads=$i --nx=$BASE_SIZE --ny=$BASE_SIZE --plan=$FFTW_PLAN
        done
    done
    # shared naive version
    ./build/fft_hpx_task_naive_shared --hpx:threads=1 --nx=$BASE_SIZE --ny=$BASE_SIZE --header=true --plan=$FFTW_PLAN
    for (( j=1; j<$LOOP; j=j+1 ))
    do
        ./build/fft_hpx_task_naive_shared --hpx:threads=1 --nx=$BASE_SIZE --ny=$BASE_SIZE --plan=$FFTW_PLAN
    done
    for (( i=2**$POW_START; i<=2**$POW_STOP; i=i*2 ))
    do
        for (( j=0; j<$LOOP; j=j+1 ))
        do
            ./build/fft_hpx_task_naive_shared --hpx:threads=$i --nx=$BASE_SIZE --ny=$BASE_SIZE --plan=$FFTW_PLAN
        done
    done
    # shared agas
    ./build/fft_hpx_task_agas_shared --hpx:threads=1 --nx=$BASE_SIZE --ny=$BASE_SIZE --header=true --plan=$FFTW_PLAN
    for (( j=1; j<$LOOP; j=j+1 ))
    do
        ./build/fft_hpx_task_agas_shared --hpx:threads=1 --nx=$BASE_SIZE --ny=$BASE_SIZE --plan=$FFTW_PLAN
    done
    for (( i=2**$POW_START; i<=2**$POW_STOP; i=i*2 ))
    do
        for (( j=0; j<$LOOP; j=j+1 ))
        do
            ./build/fft_hpx_task_agas_shared --hpx:threads=$i --nx=$BASE_SIZE --ny=$BASE_SIZE --plan=$FFTW_PLAN
        done
    done
    # scatter
    ./build/fft_hpx_task_agas --hpx:threads=1 --nx=$BASE_SIZE --ny=$BASE_SIZE --run=scatter --header=true --plan=$FFTW_PLAN
    for (( j=1; j<$LOOP; j=j+1 ))
    do
        ./build/fft_hpx_task_agas --hpx:threads=1 --nx=$BASE_SIZE --ny=$BASE_SIZE --run=scatter --plan=$FFTW_PLAN
    done
    for (( i=2**$POW_START; i<=2**$POW_STOP; i=i*2 ))
    do
        for (( j=0; j<$LOOP; j=j+1 ))
        do
            ./build/fft_hpx_task_agas --hpx:threads=$i --nx=$BASE_SIZE --ny=$BASE_SIZE --run=scatter --plan=$FFTW_PLAN
        done
    done
    ##############################
    # FFTW
    # Threads
    ./build/fftw_mpi_threads 1 $BASE_SIZE $BASE_SIZE $FFTW_PLAN 1
    for (( j=1; j<$LOOP; j=j+1 ))
    do
        ./build/fftw_mpi_threads 1 $BASE_SIZE $BASE_SIZE $FFTW_PLAN 0
    done
    for (( i=2**$POW_START; i<=2**$POW_STOP; i=i*2 ))
    do
        for (( j=0; j<$LOOP; j=j+1 ))
        do
            ./build/fftw_mpi_threads $i  $BASE_SIZE $BASE_SIZE $FFTW_PLAN 0 
        done
    done
    # OpenMP
    ./build/fftw_mpi_omp 1 $BASE_SIZE $BASE_SIZE $FFTW_PLAN 1
    for (( j=1; j<$LOOP; j=j+1 ))
    do
        ./build/fftw_mpi_omp 1 $BASE_SIZE $BASE_SIZE $FFTW_PLAN 0
    done
    for (( i=2**$POW_START; i<=2**$POW_STOP; i=i*2 ))
    do
        for (( j=0; j<$LOOP; j=j+1 ))
        do
            ./build/fftw_mpi_omp $i  $BASE_SIZE $BASE_SIZE $FFTW_PLAN 0 
        done
    done
    # MPI
    mpirun -n 1 ./build/fftw_mpi_omp 1 $BASE_SIZE $BASE_SIZE $FFTW_PLAN 1
    for (( j=1; j<$LOOP; j=j+1 ))
    do
        mpirun -n 1 ./build/fftw_mpi_omp 1 $BASE_SIZE $BASE_SIZE $FFTW_PLAN 0
    done
    for (( i=2**$POW_START; i<=2**$POW_STOP; i=i*2 ))
    do
        for (( j=0; j<$LOOP; j=j+1 ))
        do
            mpirun -n $i ./build/fftw_mpi_omp 1 $BASE_SIZE $BASE_SIZE $FFTW_PLAN 0 
        done
    done
    # HPX
    FFTW3_HPX_NTHREADS=1 ./build/fftw_mpi_hpx 1 $BASE_SIZE $BASE_SIZE $FFTW_PLAN 1
    for (( j=1; j<$LOOP; j=j+1 ))
    do
        FFTW3_HPX_NTHREADS=1./build/fftw_mpi_hpx 1 $BASE_SIZE $BASE_SIZE $FFTW_PLAN 0
    done
    for (( i=2**$POW_START; i<=2**$POW_STOP; i=i*2 ))
    do
        for (( j=0; j<$LOOP; j=j+1 ))
        do
            FFTW3_HPX_NTHREADS=$i ./build/fftw_mpi_hpx $i  $BASE_SIZE $BASE_SIZE $FFTW_PLAN 0 
        done
    done
elif [[ "$1" == "weak" ]]
then

else
  echo 'Please specify benchmark: "strong" or "weak"'
  exit 1
fi