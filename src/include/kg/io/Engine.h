
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
} // namespace detail

template <typename T>
class Descr;

// ======================================================================
// Variable

template <typename T>
class Variable;

// ======================================================================
// FileAdios

class FileAdios
{
public:
  FileAdios(adios2::Engine engine, adios2::IO io);

  void close();
  void performPuts();
  void performGets();

  template <typename T>
  void put(detail::Variable<T>& var, const T* data,
           const Mode launch = Mode::Deferred);
  template <typename T>
  void get(detail::Variable<T>& var, T* data,
           const Mode launch = Mode::Deferred);

  template <typename T>
  Dims getShape(detail::Variable<T>& var) const;

private:
  template <typename T>
  adios2::Variable<T> makeAdiosVariable(const detail::Variable<T>& var) const;

private:
  adios2::Engine engine_;

public: // FIXME
  adios2::IO io_;
};

// ======================================================================
// Engine

class Engine
{
public:
  Engine(adios2::Engine engine, adios2::IO io, MPI_Comm comm);

  template <typename T>
  detail::Variable<T> makeVariable();

  // ----------------------------------------------------------------------
  // put

  template <typename T>
  void put(detail::Variable<T>& var, const T* data,
           const Mode launch = Mode::Deferred);

  template <class T, class... Args>
  void put(const std::string& pfx, const T& datum, Args&&... args);

  template <class T, class... Args>
  void putAttribute(const std::string& pfx, const T& datum, Args&&... args);

  template <class T>
  void putLocal(const std::string& pfx, const T& datum,
                Mode launch = Mode::Deferred);

  template <template <typename...> class Var, class T, class... Args>
  void put(const std::string& pfx, const T& datum, Args&&... args);

  // ----------------------------------------------------------------------
  // get

  template <typename T>
  void get(detail::Variable<T>& var, T* data,
           const Mode launch = Mode::Deferred);

  template <class T, class... Args>
  void get(const std::string& pfx, T& datum, Args&&... args);

  template <class T, class... Args>
  void getAttribute(const std::string& pfx, T& datum, Args&&... args);

  template <class T, class... Args>
  void getLocal(const std::string& pfx, T& datum, Args&&... args);

  template <template <typename...> class Var, class T, class... Args>
  void get(const std::string& pfx, T& datum, Args&&... args);

  // ----------------------------------------------------------------------
  // performPuts

  void performPuts();

  // ----------------------------------------------------------------------
  // performGets

  void performGets();

  // ----------------------------------------------------------------------
  // getShape

  template <typename T>
  Dims getShape(detail::Variable<T>& var);

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
  FileAdios file_;
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
