
#pragma once

#include <kg/io.h>

#include "fields3d.inl"

#include <cstdio>

class WriterADIOS2
{
public:
  explicit operator bool() const { return pfx_.size() != 0; }

  void open(const std::string& pfx, const std::string& dir = ".")
  {
    assert(pfx_.size() == 0);
    pfx_ = pfx;
    dir_ = dir;
  }

  void close()
  {
    assert(pfx_.size() != 0);
    pfx_.clear();
    dir_.clear();
  }

  void begin_step(const Grid_t& grid)
  {
    begin_step(grid.timestep(), grid.timestep() * grid.dt);
  }

  void begin_step(int step, double time)
  {
    char filename[dir_.size() + pfx_.size() + 20];
    sprintf(filename, "%s/%s.%09d.bp", dir_.c_str(), pfx_.c_str(), step);
    file_ = io__.open(filename, kg::io::Mode::Write);
    file_.beginStep(kg::io::StepMode::Append);
    file_.put("step", step);
    file_.put("time", time);
    file_.performPuts();
  }

  void end_step() { file_.close(); }

  void set_subset(const Grid_t& grid, Int3 rn, Int3 rx) {}

  template <typename Mfields>
  void write(const Mfields& _mflds, const Grid_t& grid, const std::string& name,
             const std::vector<std::string>& comp_names)
  {
    auto&& mflds = evalMfields(_mflds);

    file_.put(name, mflds);
    file_.performPuts();
  }

private:
  kg::io::IOAdios2 io__;
  kg::io::Engine file_;
  std::string pfx_;
  std::string dir_;
};
