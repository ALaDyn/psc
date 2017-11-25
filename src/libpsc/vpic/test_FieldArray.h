
template<class FieldArray>
void test_FieldArray_methods(FieldArray& fa)
{
  double en[6];
  fa.energy_f(en);

  fa.advance_b(1.);
  fa.advance_e(1.);

  fa.clear_jf();
  fa.synchronize_jf();
  fa.clear_rhof();
  fa.synchronize_rho();

  fa.compute_rhob();
  fa.compute_curl_b();
}

