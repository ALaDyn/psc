#!/bin/sh 

openmpirun -n 1 ./mhd_kh \
    --mrc_crds_lx -0.5 --mrc_crds_ly -0.5 \
    --mrc_crds_hx 0.5 --mrc_crds_hy 0.5 \
    --mrc_crds_lz -0.01 --mrc_crds_hz 0.01 \
    --pert 1e-1 \
    --ggcm_mhd_diag_fields rr1:uu1:rv1:j:b1:divb:rr:pp:v:b \
    --ggcm_mhd_diag_run kh \
    --mx 64 --my 64 --mz 2 --npx 1 --npy 1 \
    --mrc_ts_output_every_time 0.01 --gamma 1.4 \
    --mrc_ts_max_time 5.0 \
    --mrc_ts_type step \
    --ggcm_mhd_step_type vl \
    --magdiffu const \
    --do_nwst \
    2>&1 | tee log
