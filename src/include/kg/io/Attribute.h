
#pragma once

namespace kg
{
namespace detail
{

// ======================================================================
// detail::Attribute
//
// Handles T being one of the base adios2 types (or arrays thereof)

template <typename T>
struct Attribute
{
  Attribute(const std::string& name, IO& io) : name_{name} {}

  void put(Engine& writer, const T* data, size_t size)
  {
    writer.putAttribute(name_, data, size);
  }

  void put(Engine& writer, const T& value)
  {
    writer.putAttribute(name_, value);
  }

  void get(Engine& reader, std::vector<T>& data)
  {
    reader.getAttribute(name_, data);
  }

  void get(Engine& reader, T& value)
  {
    std::vector<T> vals;
    reader.getAttribute(name_, vals);
    assert(vals.size() == 1);
    value = vals[0];
  }

protected:
  const std::string name_;
};

}; // namespace detail

// ======================================================================
// Attribute<T>
//
// single value

template <typename T, typename Enable>
class Attribute
{
  using DataType = T;

public:
  Attribute(const std::string& name, IO& io) : attr_{name, io} {}

  void put(Engine& writer, const T& value, const Mode launch = Mode::Deferred)
  {
    attr_.put(writer, value);
  }

  void get(Engine& reader, T& value, const Mode launch = Mode::Deferred)
  {
    attr_.get(reader, value);
  }

private:
  detail::Attribute<DataType> attr_;
};

// ======================================================================
// Attribute<std::vector>

namespace detail
{

template <typename T>
struct is_vector : std::false_type
{};

template <typename T>
struct is_vector<std::vector<T>> : std::true_type
{};

}; // namespace detail

template <class T>
class Attribute<T, typename std::enable_if<detail::is_vector<T>::value>::type>
{
  using DataType = typename T::value_type;

public:
  using value_type = T;

  Attribute(const std::string& name, IO& io) : attr_{name, io} {}

  void put(Engine& writer, const T& vec, const Mode launch = Mode::Deferred)
  {
    attr_.put(writer, vec.data(), vec.size());
  }

  void get(Engine& reader, T& vec, const Mode launch = Mode::Deferred)
  {
    attr_.get(reader, vec);
  }

private:
  detail::Attribute<DataType> attr_;
};

// ======================================================================
// Attribute<Vec3>

namespace detail
{

template <typename T>
struct is_Vec3 : std::false_type
{};

template <typename T>
struct is_Vec3<Vec3<T>> : std::true_type
{};

}; // namespace detail

template <class T>
class Attribute<T, typename std::enable_if<detail::is_Vec3<T>::value>::type>
{
  using DataType = typename T::value_type;

public:
  using value_type = T;

  Attribute(const std::string& name, IO& io) : attr_{name, io} {}

  void put(Engine& writer, const T& vec, const Mode launch = Mode::Deferred)
  {
    attr_.put(writer, vec.data(), 3);
  }

  void get(Engine& reader, T& data, const Mode launch = Mode::Deferred)
  {
    std::vector<DataType> vals;
    attr_.get(reader, vals);
    data = {vals[0], vals[1], vals[2]};
  }

private:
  detail::Attribute<DataType> attr_;
};

}; // namespace kg
