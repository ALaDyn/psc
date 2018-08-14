
#ifndef PSC_FIELDS_VPIC_H
#define PSC_FIELDS_VPIC_H

#include "fields3d.hxx"

// ======================================================================
// MfieldsStatePsc

template<typename FieldArray>
struct MfieldsStatePsc
{
  using real_t = float;
  using Grid = typename FieldArray::Grid;
  using MaterialList = typename FieldArray::MaterialList;

  using Element = typename FieldArray::Element;

  enum {
    EX = 0, EY = 1, EZ = 2, DIV_E_ERR = 3,
    BX = 4, BY = 5, BZ = 6, DIV_B_ERR = 7,
    TCAX = 8, TCAY = 9, TCAZ = 10, RHOB = 11,
    JFX = 12, JFY = 13, JFZ = 14, RHOF = 15,
    N_COMP = 20,
  };

  struct Patch
  {
    using Element = Element;
    
    Element* data() { return fa->data(); }
    Grid* grid() { return fa->grid(); }
    Element  operator[](int idx) const { return (*fa)[idx]; }
    Element& operator[](int idx)       { return (*fa)[idx]; }

    FieldArray* fa;
  };
  
  using fields_t = fields3d<float, LayoutAOS>;

  MfieldsStatePsc(const Grid_t& grid, Grid* vgrid, const MaterialList& material_list, double damp = 0.)
    : grid_{grid}
  {
    assert(grid.n_patches() == 1);

    patch_.fa = FieldArray::create(vgrid, material_list, damp);
  }

  const Grid_t& grid() const { return grid_; }
  int n_patches() const { return grid_.n_patches(); }
  int n_comps() const { return N_COMP; }
  Int3 ibn() const { return {1,1,1}; }
  
  fields_t operator[](int p)
  {
    assert(p == 0);
    int ib[3], im[3];
    float* data = patch_.fa->getData(ib, im);
    return {ib, im, N_COMP, data};
  }

  Patch& getPatch(int p) { return patch_; }

  FieldArray& vmflds() { return *patch_.fa; }

  Grid* vgrid() { return patch_.fa->grid(); }

  // static const Convert convert_to_, convert_from_;
  // const Convert& convert_to() override { return convert_to_; }
  // const Convert& convert_from() override { return convert_from_; }

private:
  const Grid_t& grid_;
  Patch patch_;
};


#endif
