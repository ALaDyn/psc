
#pragma once

#include "Engine.h"

namespace kg
{
namespace io
{

namespace detail
{

// ----------------------------------------------------------------------
// detail::Variable

template <typename T>
Variable<T>::Variable(adios2::Variable<T> var) : var_{var}
{}

template <typename T>
void Variable<T>::put(Engine& writer, const T& datum, const Mode launch)
{
  writer.engine_.Put(var_, datum, launch);
}

template <typename T>
void Variable<T>::put(Engine& writer, const T* data, const Mode launch)
{
  writer.engine_.Put(var_, data, launch);
}

template <typename T>
void Variable<T>::get(Engine& reader, T& datum, const Mode launch)
{
  reader.engine_.Get(var_, datum, launch);
}

template <typename T>
void Variable<T>::get(Engine& reader, T* data, const Mode launch)
{
  reader.engine_.Get(var_, data, launch);
}

template <typename T>
void Variable<T>::setSelection(const Box<Dims>& selection)
{
  var_.SetSelection(selection);
}

template <typename T>
void Variable<T>::setMemorySelection(const Box<Dims>& selection)
{
  var_.SetMemorySelection(selection);
}

template <typename T>
void Variable<T>::setShape(const Dims& shape)
{
  var_.SetShape(shape);
}

template <typename T>
Dims Variable<T>::shape() const
{
  return var_.Shape();
}

} // namespace detail

} // namespace io
} // namespace kg
