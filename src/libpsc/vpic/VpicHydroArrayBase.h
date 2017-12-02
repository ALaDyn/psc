
#ifndef VPIC_HYDRO_ARRAY_BASE_H
#define VPIC_HYDRO_ARRAY_BASE_H

// ======================================================================
// VpicHydroArrayBase

struct VpicHydroArrayBase : hydro_array_t
{
  typedef hydro_t Element;
  
  VpicHydroArrayBase(Grid* g);
  ~VpicHydroArrayBase();

  float* getData(int* ib, int* im);

  Element  operator[](int idx) const { return h[idx]; }
  Element& operator[](int idx)       { return h[idx]; }

  Element *data()
  {
    return h;
  }

  Grid* getGrid()
  {
    return static_cast<Grid*>(g);
  }
};

// ----------------------------------------------------------------------
// copied from hydro_array.c, converted from new/delete -> ctor

inline void
hydro_array_ctor(hydro_array_t * ha, grid_t * g ) {
  if( !g ) ERROR(( "NULL grid" ));
  MALLOC_ALIGNED( ha->h, g->nv, 128 );
  ha->g = g;
  /* clear_hydro_array( ha ); */ // now done in C++ constructor
}

inline void
hydro_array_dtor( hydro_array_t * ha ) {
  if( !ha ) return;
  FREE_ALIGNED( ha->h );
}

// ----------------------------------------------------------------------
// VpicHydroArrayBase implementation

inline VpicHydroArrayBase::VpicHydroArrayBase(Grid* grid)
{
  hydro_array_ctor(this, grid);
  /*clear();*/ // can't do it here, only in derived class (case for CRTP)?
}

inline VpicHydroArrayBase::~VpicHydroArrayBase()
{
  hydro_array_dtor(this);
}

inline float* VpicHydroArrayBase::getData(int* ib, int* im)
{
  const int B = 1; // VPIC always uses one ghost cell (on c.c. grid)

  im[0] = g->nx + 2*B;
  im[1] = g->ny + 2*B;
  im[2] = g->nz + 2*B;
  ib[0] = -B;
  ib[1] = -B;
  ib[2] = -B;
  return &h[0].jx;
}


#endif

