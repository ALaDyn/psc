#! /bin/sh

set -e

openmpirun -n 4 ./test_mrc_domain_multi --npx 3 --npy 2 \
    --mrc_io_type xdmf2 \
    --mrc_crds_sw 2 \
    --test_m3_sw 2 \

TEST=2
for a in reference_results/$TEST/*.xdmf; do 
    b=`basename $a`
    diff -u $a $b
done

for a in reference_results/$TEST/*.h5.dump; do 
    b=`basename $a .dump`
    h5dump $b > $b.dump
    diff -u $a $b.dump
    rm $b.dump
done

