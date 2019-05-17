
#pragma once

#include "vec3.hxx"

namespace kg
{
namespace io
{

class Engine;

template <typename T, typename Enable = void>
class Descr;

// ======================================================================
// Descr<T>
//
// single value
// (FIXME, should be adios2 T only)

template <typename T, typename Enable>
class Descr
{
public:
  void put(Engine& writer, const T& value, Mode launch = Mode::Deferred);
  void get(Engine& reader, T& value, Mode launch = Mode::Deferred);
};

// ======================================================================
// Descr<T[N]>

template <typename T, size_t N>
class Descr<T[N]> // typename
                  // std::enable_if<std::is_adios2_type<T>::value>::type>
{
public:
  using value_type = T[N];

  void put(Engine& writer, const value_type& arr, Mode launch = Mode::Deferred);
  void get(Engine& reader, value_type& arr, Mode launch = Mode::Deferred);
};

// ======================================================================
// Descr<std::vector>

template <class T>
class Descr<std::vector<T>>
{
public:
  using value_type = std::vector<T>;

  void put(Engine& writer, const value_type& vec, Mode launch = Mode::Deferred);
  void get(Engine& reader, value_type& vec, Mode launch = Mode::Deferred);
};

// ======================================================================
// Descr<Vec3>

template <typename T>
class Descr<Vec3<T>>
{
public:
  using value_type = Vec3<T>;

  void put(Engine& writer, const value_type& vec, Mode launch = Mode::Deferred);
  void get(Engine& reader, value_type& data, Mode launch = Mode::Deferred);
};

} // namespace io
} // namespace kg

#include "Descr.inl"
