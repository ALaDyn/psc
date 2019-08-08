
#pragma once

#include <algorithm>
#include <cassert>
#include <memory>

#include "cuda_compat.h"
#include <kg/Macros.h>

namespace kg
{

// ======================================================================
// Vec

template <typename T, std::size_t N>
struct Vec
{
  using value_type = T;
  using size_t = std::size_t;

  T arr[N];

  KG_INLINE T operator[](size_t i) const { return arr[i]; }

  KG_INLINE T& operator[](size_t i) { return arr[i]; }

  KG_INLINE const T* data() const { return arr; }

  KG_INLINE T* data() { return arr; }

  // ----------------------------------------------------------------------
  // construct from pointer to values

  KG_INLINE static Vec fromPointer(const T* p) { return {p[0], p[1], p[2]}; }

  // ----------------------------------------------------------------------
  // converting to Vec of different type (e.g., float -> double)

  template <typename U>
  KG_INLINE explicit operator Vec<U, N>() const
  {
    return {U((*this)[0]), U((*this)[1]), U((*this)[2])};
  }

  // ----------------------------------------------------------------------
  // arithmetic

  KG_INLINE Vec operator-() const
  {
    Vec res;
    for (int i = 0; i < 3; i++) {
      res[i] = -(*this)[i];
    }
    return res;
  }

  KG_INLINE Vec& operator+=(const Vec& w)
  {
    for (int i = 0; i < 3; i++) {
      (*this)[i] += w[i];
    }
    return *this;
  }

  KG_INLINE Vec& operator-=(const Vec& w)
  {
    for (int i = 0; i < 3; i++) {
      (*this)[i] -= w[i];
    }
    return *this;
  }

  KG_INLINE Vec& operator*=(const Vec& w)
  {
    for (int i = 0; i < 3; i++) {
      (*this)[i] *= w[i];
    }
    return *this;
  }

  KG_INLINE Vec& operator*=(T s)
  {
    for (int i = 0; i < 3; i++) {
      (*this)[i] *= s;
    }
    return *this;
  }

  KG_INLINE Vec& operator/=(const Vec& w)
  {
    for (int i = 0; i < 3; i++) {
      (*this)[i] /= w[i];
    }
    return *this;
  }

  // conversion to pointer

  KG_INLINE operator const T*() const { return data(); }

  KG_INLINE operator T*() { return data(); }
};

template <typename T, std::size_t N>
bool operator==(const Vec<T, N>& x, const Vec<T, N>& y)
{
  return std::equal(x.arr, x.arr + N, y.arr);
}

template <typename T, std::size_t N>
bool operator!=(const Vec<T, N>& x, const Vec<T, N>& y)
{
  return !(x == y);
}

template <typename T, std::size_t N>
KG_INLINE Vec<T, N> operator+(const Vec<T, N>& v, const Vec<T, N>& w)
{
  Vec<T, N> res = v;
  res += w;
  return res;
}

template <typename T, std::size_t N>
KG_INLINE Vec<T, N> operator-(const Vec<T, N>& v, const Vec<T, N>& w)
{
  Vec<T, N> res = v;
  res -= w;
  return res;
}

template <typename T, std::size_t N>
KG_INLINE Vec<T, N> operator*(const Vec<T, N>& v, const Vec<T, N>& w)
{
  Vec<T, N> res = v;
  res *= w;
  return res;
}

template <typename T, std::size_t N>
KG_INLINE Vec<T, N> operator*(T s, const Vec<T, N>& v)
{
  Vec<T, N> res = v;
  res *= s;
  return res;
}

template <typename T, std::size_t N>
KG_INLINE Vec<T, N> operator/(const Vec<T, N>& v, const Vec<T, N>& w)
{
  Vec<T, N> res = v;
  res /= w;
  return res;
}

} // namespace kg

template <typename T>
using Vec3 = kg::Vec<T, 3>;

using Int3 = Vec3<int>;
using UInt3 = Vec3<unsigned int>;
using Float3 = Vec3<float>;
using Double3 = Vec3<double>;

