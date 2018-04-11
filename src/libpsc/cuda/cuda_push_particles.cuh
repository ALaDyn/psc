
#pragma once

// ----------------------------------------------------------------------
// cuda_push_mprts

template<typename BS>
struct cuda_mparticles;

template<typename BS>
struct CudaPushParticles_
{
  static void push_mprts_yz(cuda_mparticles<BS>* cmprts, struct cuda_mfields *cmflds,
			    bool ip_ec, bool deposit_vb_3d, bool currmem_global);

  static void push_mprts_xyz(cuda_mparticles<BS144>*cmprts, struct cuda_mfields *cmflds);
};
