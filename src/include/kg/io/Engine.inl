
#include "Attribute.h"
#include "Variable.h"

namespace kg
{
namespace io
{

Engine::Engine(adios2::Engine engine, adios2::IO& io, MPI_Comm comm)
  : engine_{engine}, io_{io}
{
  MPI_Comm_rank(comm, &mpi_rank_);
  MPI_Comm_size(comm, &mpi_size_);
}

template <typename T>
detail::Variable<T> Engine::_defineVariable(const std::string& name,
                                            const Dims& shape,
                                            const Dims& start,
                                            const Dims& count,
                                            const bool constantDims)
{
  auto var = io_.InquireVariable<T>(name);
  if (var) {
    return var;
  } else {
    return io_.DefineVariable<T>(name, shape, start, count, constantDims);
  }
}

template <typename T>
Variable<T> Engine::defineVariable(const std::string& name)
{
  return {name, *this};
}

// ----------------------------------------------------------------------
// put

template <class T, class... Args>
void Engine::put(T& variable, Args&&... args)
{
  mprintf("put name %s\n", "?"); // variable.name().c_str());
  variable.put(*this, std::forward<Args>(args)...);
}

template <class T, class... Args>
void Engine::put1(const std::string& pfx, const T& datum, Args&&... args)
{
  put<Attribute>(pfx, datum, std::forward<Args>(args)...);
}

template <class T, class... Args>
void Engine::putLocal(const std::string& pfx, const T& datum, Args&&... args)
{
  put<VariableLocalSingleValue>(pfx, datum, std::forward<Args>(args)...);
}

template <class T, class... Args>
void Engine::putVar(const std::string& pfx, const T& datum, Args&&... args)
{
  put<Variable>(pfx, datum, std::forward<Args>(args)...);
}

template <template <typename...> class Var, class T, class... Args>
void Engine::put(const std::string& pfx, const T& datum, Args&&... args)
{
  prefixes_.push_back(pfx);
  mprintf("put<Var> pfx %s -- %s\n", pfx.c_str(), prefix().c_str());
  Var<T> var{prefix(), *this};
  var.put(*this, datum, std::forward<Args>(args)...);
  prefixes_.pop_back();
}

// ----------------------------------------------------------------------
// get

template <class T, class... Args>
void Engine::get(T& variable, Args&&... args)
{
  variable.get(*this, std::forward<Args>(args)...);
}

template <class T, class... Args>
void Engine::get1(const std::string& pfx, T& datum, Args&&... args)
{
  get<Attribute>(pfx, datum, std::forward<Args>(args)...);
}

template <class T, class... Args>
void Engine::getLocal(const std::string& pfx, T& datum, Args&&... args)
{
  get<VariableLocalSingleValue>(pfx, datum, std::forward<Args>(args)...);
}

template <class T, class... Args>
void Engine::getVar(const std::string& pfx, T& datum, Args&&... args)
{
  get<Variable>(pfx, datum, std::forward<Args>(args)...);
}

template <template <typename...> class Var, class T, class... Args>
void Engine::get(const std::string& pfx, T& datum, Args&&... args)
{
  prefixes_.push_back(pfx);
  mprintf("get<Var> pfx %s -- %s\n", pfx.c_str(), prefix().c_str());
  Var<T> var{prefix(), *this};
  var.get(*this, datum, std::forward<Args>(args)...);
  prefixes_.pop_back();
}

// ----------------------------------------------------------------------
// performPuts

void Engine::performPuts()
{
  engine_.PerformPuts();
}

// ----------------------------------------------------------------------
// performGets

void Engine::performGets()
{
  engine_.PerformGets();
}

// ----------------------------------------------------------------------
// close

void Engine::close()
{
  engine_.Close();
}

int Engine::mpiRank() const
{
  return mpi_rank_;
}
int Engine::mpiSize() const
{
  return mpi_size_;
}

} // namespace io
} // namespace kg
