
#pragma once

#include "FileAdios2.h"

namespace kg
{
namespace io
{

// ======================================================================
// File

class File
{
public:
  File(adios2::Engine engine, adios2::IO io);

  void close();
  void performPuts();
  void performGets();

  template <typename T>
  void putVariable(const std::string& name, const T* data, Mode launch,
                   const Dims& shape, const Box<Dims>& selection,
                   const Box<Dims>& memory_selection);

  template <typename T>
  void getVariable(const std::string& name, T* data, Mode launch,
                   const Box<Dims>& selection,
                   const Box<Dims>& memory_selection);

  Dims shapeVariable(const std::string& name) const;

  template <typename T>
  void getAttribute(const std::string& name, T* data);

  template <typename T>
  void putAttribute(const std::string& name, const T* data, size_t size);

  size_t sizeAttribute(const std::string& name) const;

private:
  std::unique_ptr<FileBase> impl_;
};

} // namespace io
} // namespace kg

#include "File.inl"

