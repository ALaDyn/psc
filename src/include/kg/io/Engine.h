
#pragma once

#include "IO.h"

#include <mrc_common.h>

namespace kg
{

namespace detail
{
template <typename T>
class Variable;
}

// ======================================================================
// Engine

class Engine
{
public:
  Engine(adios2::Engine engine, IO& io, MPI_Comm comm);

  template <typename T>
  detail::Variable<T> _defineVariable(const std::string& name,
                                      const Dims& shape = Dims(),
                                      const Dims& start = Dims(),
                                      const Dims& count = Dims(),
                                      const bool constantDims = false);

  // ----------------------------------------------------------------------
  // put for adios2 variables

  template <typename T>
  void put(adios2::Variable<T> variable, const T* data,
           const Mode launch = Mode::Deferred);

  template <typename T>
  void put(adios2::Variable<T> variable, const T& datum,
           const Mode launch = Mode::Deferred);

  // ----------------------------------------------------------------------
  // put in general

  template <class T, class... Args>
  void put(T& variable, Args&&... args);

  // ----------------------------------------------------------------------
  // get for adios2 variables

  template <typename T>
  void get(adios2::Variable<T> variable, T& datum,
           const Mode launch = Mode::Deferred);

  template <typename T>
  void get(adios2::Variable<T> variable, T* data,
           const Mode launch = Mode::Deferred);

  template <typename T>
  void get(adios2::Variable<T> variable, std::vector<T>& data,
           const Mode launch = Mode::Deferred);

  // ----------------------------------------------------------------------
  // get in general

  template <class T, class... Args>
  void get(T& variable, Args&&... args);

  // ----------------------------------------------------------------------
  // performPuts

  void performPuts();

  // ----------------------------------------------------------------------
  // performGets

  void performGets();

  // ----------------------------------------------------------------------
  // close

  void close();

  template <typename T>
  void putAttribute(const std::string& name, const T* data, size_t size);

  template <typename T>
  void putAttribute(const std::string& name, const T& value);

  template <typename T>
  void getAttribute(const std::string& name, std::vector<T>& data);

  int mpiRank() const;
  int mpiSize() const;

private:
  adios2::Engine engine_;
  IO io_;
  int mpi_rank_;
  int mpi_size_;
};

} // namespace kg

#include "Engine.inl"
