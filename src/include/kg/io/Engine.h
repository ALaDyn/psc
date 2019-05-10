
#pragma once

#include <mrc_common.h>

#include <deque>
#include <iostream>

namespace kg
{
namespace io
{

namespace detail
{
template <typename T>
class Variable;

template <typename T>
class Attribute;
}

template <typename T>
class Variable;

// ======================================================================
// Engine

class Engine
{
public:
  Engine(adios2::Engine engine, adios2::IO& io, MPI_Comm comm);

  template <typename T>
  detail::Variable<T> _defineVariable(const std::string& name,
                                      const Dims& shape = Dims(),
                                      const Dims& start = Dims(),
                                      const Dims& count = Dims(),
                                      const bool constantDims = false);

  template <typename T>
  Variable<T> defineVariable(const std::string& name);

  // ----------------------------------------------------------------------
  // put

  template <class T, class... Args>
  void put(T& variable, Args&&... args);

  template <class T, class... Args>
  void put1(const std::string& pfx, const T& datum, Args&&... args);

  template <class T, class... Args>
  void putLocal(const std::string& pfx, const T& datum, Args&&... args);

  template <class T, class... Args>
  void putVar(const std::string& pfx, const T& datum, Args&&... args);

  template <template <typename> class Var, class T, class... Args>
  void put(const std::string& pfx, const T* data, Args&&... args);

  // ----------------------------------------------------------------------
  // get

  template <class T, class... Args>
  void get(T& variable, Args&&... args);

  template <class T, class... Args>
  void get1(const std::string& pfx, T& datum, Args&&... args);

  template <class T, class... Args>
  void getLocal(const std::string& pfx, T& datum, Args&&... args);

  template <class T, class... Args>
  void getVar(const std::string& pfx, T& datum, Args&&... args);

  template <template <typename> class Var, class T, class... Args>
  void get(const std::string& pfx, T* data, Args&&... args);

  // ----------------------------------------------------------------------
  // performPuts

  void performPuts();

  // ----------------------------------------------------------------------
  // performGets

  void performGets();

  // ----------------------------------------------------------------------
  // close

  void close();

  int mpiRank() const;
  int mpiSize() const;

  std::string prefix() const
  {
    std::string s;
    bool first = true;
    for (auto& pfx : prefixes_) {
      if (!first) {
        s += "::";
      }
      s += pfx;
      first = false;
    }
    return s;
  }

private:
  adios2::Engine engine_;
  adios2::IO io_;
  std::deque<std::string> prefixes_;
  int mpi_rank_;
  int mpi_size_;

  template <typename T>
  friend class detail::Attribute;
  template <typename T>
  friend class detail::Variable;
};

} // namespace io
} // namespace kg

#include "Engine.inl"
