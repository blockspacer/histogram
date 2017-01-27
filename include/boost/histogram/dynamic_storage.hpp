// Copyright 2015-2016 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef _BOOST_HISTOGRAM_DYNAMIC_STORAGE_HPP_
#define _BOOST_HISTOGRAM_DYNAMIC_STORAGE_HPP_

#include <boost/histogram/detail/weight.hpp>
#include <boost/histogram/detail/mpl.hpp>
#include <boost/assert.hpp>
#include <boost/mpl/map.hpp>
#include <boost/mpl/pair.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/at.hpp>
#include <boost/cstdint.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <type_traits>
#include <algorithm>
#include <limits>
#include <cstddef>
#include <memory>

namespace boost {
namespace histogram {

namespace detail {

  static_assert(std::is_pod<weight_t>::value, "weight_t must be POD");

  using mp_int = multiprecision::cpp_int;

  using type_to_int = mpl::map<
    mpl::pair<weight_t, mpl::int_<-1>>,

    mpl::pair<int8_t, mpl::int_<1>>,
    mpl::pair<int16_t, mpl::int_<2>>,
    mpl::pair<int32_t, mpl::int_<3>>,
    mpl::pair<int64_t, mpl::int_<4>>,

    mpl::pair<uint8_t, mpl::int_<1>>,
    mpl::pair<uint16_t, mpl::int_<2>>,
    mpl::pair<uint32_t, mpl::int_<3>>,
    mpl::pair<uint64_t, mpl::int_<4>>,

    mpl::pair<mp_int, mpl::int_<5>>
  >;

  using int_to_type = mpl::map<
    mpl::pair<mpl::int_<-1>, weight_t>,
    mpl::pair<mpl::int_<1>, uint8_t>,
    mpl::pair<mpl::int_<2>, uint16_t>,
    mpl::pair<mpl::int_<3>, uint32_t>,
    mpl::pair<mpl::int_<4>, uint64_t>,
    mpl::pair<mpl::int_<5>, mp_int>
  >;

  template <typename T>
  using storage_type =
    typename mpl::at<
      detail::int_to_type,
      typename mpl::at<detail::type_to_int, T>::type
    >::type;

  template <typename T>
  using next_storage_type =
    typename mpl::at<int_to_type,
      typename mpl::next<
        typename mpl::at<type_to_int, T>::type
      >::type
    >::type;

  template <typename Buffer, typename T, typename TO>
  struct add_one_impl {
    static void apply(Buffer& b, const std::size_t i, const TO& o) {
      auto& bi = b.template at<T>(i);
      if (static_cast<T>(std::numeric_limits<T>::max() - bi) >= o)
        bi += static_cast<T>(o);
      else {
        b.template grow<T>();
        add_one_impl<Buffer, next_storage_type<T>, TO>::apply(b, i, o);
      }
    }
  };

  template <typename Buffer, typename TO>
  struct add_one_impl<Buffer, mp_int, TO> {
    static void apply(Buffer& b, const std::size_t i, const TO& o) {
      b.template at<mp_int>(i) += o;
    }
  };

  template <typename Buffer, typename TO>
  struct add_impl {
    static void apply(Buffer& b, const Buffer& o) {
      for (decltype(b.size_) i = 0; i < b.size_; ++i) {
        const auto oi = o.template at<TO>(i);
        switch (b.type_.id_) {
          case -1: b.template at<weight_t>(i) += oi; break;
          case 0: b.template initialize<uint8_t>(); // fall through
          case 1: add_one_impl<Buffer, uint8_t, TO>::apply(b, i, oi); break;
          case 2: add_one_impl<Buffer, uint16_t, TO>::apply(b, i, oi); break;
          case 3: add_one_impl<Buffer, uint32_t, TO>::apply(b, i, oi); break;
          case 4: add_one_impl<Buffer, uint64_t, TO>::apply(b, i, oi); break;
          case 5: add_one_impl<Buffer, mp_int, TO>::apply(b, i, oi); break;
        }
      }
    }
  };

  template <typename Buffer>
  struct add_impl<Buffer, weight_t> {
    static void apply(Buffer& b, const Buffer& o) {
      b.wconvert();
      for (decltype(b.size_) i = 0; i < b.size_; ++i)
        b.template at<weight_t>(i) += o.template at<weight_t>(i);
    }
  };

  struct buffer
  {
    explicit buffer(std::size_t s = 0) :
      size_(s),
      ptr_(nullptr)
    {}

    buffer(const buffer& o) :
      size_(o.size_)
    {
      switch (o.type_.id_) {
        case -1: create<weight_t>(); copy_from<weight_t>(o.ptr_); break;
        case 0: ptr_ = nullptr; break;
        case 1: create<uint8_t>(); copy_from<uint8_t>(o.ptr_); break;
        case 2: create<uint16_t>(); copy_from<uint16_t>(o.ptr_); break;
        case 3: create<uint32_t>(); copy_from<uint32_t>(o.ptr_); break;
        case 4: create<uint64_t>(); copy_from<uint64_t>(o.ptr_); break;
        case 5: create<mp_int>(); copy_from<mp_int>(o.ptr_); break;
      }
    }

    buffer& operator=(const buffer& o)
    {
      if (this != &o) {
        if (size_ != o.size_ || type_.id_ != o.type_.id_) {
          destroy_any();
          size_ = o.size_;
          switch (o.type_.id_) {
            case -1: create<weight_t>(); copy_from<weight_t>(o.ptr_); break;
            case 0: ptr_ = nullptr; break;
            case 1: create<uint8_t>(); copy_from<uint8_t>(o.ptr_); break;
            case 2: create<uint16_t>(); copy_from<uint16_t>(o.ptr_); break;
            case 3: create<uint32_t>(); copy_from<uint32_t>(o.ptr_); break;
            case 4: create<uint64_t>(); copy_from<uint64_t>(o.ptr_); break;
            case 5: create<mp_int>(); copy_from<mp_int>(o.ptr_); break;
          }
        } else {
          switch (o.type_.id_) {
            case -1: copy_from<weight_t>(o.ptr_); break;
            case 0: ptr_ = nullptr; break;
            case 1: copy_from<uint8_t>(o.ptr_); break;
            case 2: copy_from<uint16_t>(o.ptr_); break;
            case 3: copy_from<uint32_t>(o.ptr_); break;
            case 4: copy_from<uint64_t>(o.ptr_); break;
            case 5: copy_from<mp_int>(o.ptr_); break;
          }
        }
      }
      return *this;
    }

    buffer(buffer&& o) :
      type_(o.type_),
      size_(o.size_),
      ptr_(o.ptr_)
    {
      o.size_ = 0;
      o.type_ = type();
      o.ptr_ = nullptr;
    }

    buffer& operator=(buffer&& o)
    {
      if (this != &o) {
        destroy_any();
        type_ = o.type_;
        size_ = o.size_;
        ptr_ = o.ptr_;
        o.type_ = type();
        o.size_ = 0;
        o.ptr_ = nullptr;
      }
      return *this;
    }

    ~buffer() { destroy_any(); }

    template <typename T>
    void create() {
      std::allocator<T> a;
      ptr_ = a.allocate(size_);
      new (ptr_) T[size_];
      type_.set<T>();
    }

    template <typename T>
    void destroy() {
      for (T* iter = &at<T>(0); iter != &at<T>(size_); ++iter)
        iter->~T();
      std::allocator<T> a;
      a.deallocate(static_cast<T*>(ptr_), size_);
      ptr_ = nullptr;
    }

    void destroy_any() {
      switch (type_.id_) {
        case -1: destroy<weight_t>(); break;
        case 0: ptr_ = nullptr; break;
        case 1: destroy<uint8_t>(); break;
        case 2: destroy<uint16_t>(); break;
        case 3: destroy<uint32_t>(); break;
        case 4: destroy<uint64_t>(); break;
        case 5: destroy<mp_int>(); break;
      }
    }

    template <typename T>
    void copy_from(const void* p) {
      std::copy(static_cast<const T*>(p), static_cast<const T*>(p) + size_,
                static_cast<T*>(ptr_));
    }

    template <typename T,
              typename U = next_storage_type<T>>
    void grow() {
      std::allocator<U> a;
      U* u = a.allocate(size_);
      new (u) U[size_];
      std::copy(&at<T>(0), &at<T>(size_), u);
      destroy<T>();
      ptr_ = u;
      type_.set<U>();
    }

    void wconvert()
    {
      switch (type_.id_) {
        case -1: break;
        case 0: initialize<weight_t>(); break;
        case 1: grow<uint8_t, weight_t> (); break;
        case 2: grow<uint16_t, weight_t>(); break;
        case 3: grow<uint32_t, weight_t>(); break;
        case 4: grow<uint64_t, weight_t>(); break;
        case 5: grow<mp_int, weight_t>(); break;
      }
    }

    template <typename T>
    void initialize() {
      create<T>();
      std::fill(&at<T>(0), &at<T>(size_), T(0));
    }

    template <typename T>
    T& at(std::size_t i) { return static_cast<T*>(ptr_)[i]; }

    template <typename T>
    const T& at(std::size_t i) const { return static_cast<const T*>(ptr_)[i]; }

    template <typename T>
    void increase(const std::size_t i) {
      auto& bi = at<T>(i);
      if (bi < std::numeric_limits<T>::max())
        ++bi;
      else {
        grow<T>();
        ++at<next_storage_type<T>>(i);
      }
    }

    struct type {
      char id_ = 0, depth_ = 0;
      template <typename T>
      void set() {
        id_ = mpl::at<type_to_int, T>::type::value;
        depth_ = sizeof(T);
      }
    } type_;
    std::size_t size_;
    void* ptr_;
  };

} // NS detail

class dynamic_storage
{
public:
  using value_t = double;
  using variance_t = double;

  explicit
  dynamic_storage(std::size_t s) :
    buffer_(s)
  {}

  dynamic_storage() = default;
  dynamic_storage(const dynamic_storage&) = default;
  dynamic_storage& operator=(const dynamic_storage&) = default;
  dynamic_storage(dynamic_storage&&) = default;
  dynamic_storage& operator=(dynamic_storage&&) = default;

  template <typename T,
            template <typename> class Storage,
            typename = detail::is_standard_integral<T>>
  dynamic_storage(const Storage<T>& o) :
    buffer_(o.size())
  {
    using U = detail::storage_type<T>;
    buffer_.create<U>();
    std::copy(o.data(), o.data() + o.size(), &buffer_.at<U>(0));
  }

  template <typename T,
            template <typename> class Storage,
            typename = detail::is_standard_integral<T>>
  dynamic_storage& operator=(const Storage<T>& o)
  {
    buffer_.destroy_any();
    buffer_.size_ = o.size();
    using U = detail::storage_type<T>;
    buffer_.create<U>();
    std::copy(o.data(), o.data() + o.size(), &buffer_.at<U>(0));
    return *this;
  }

  std::size_t size() const { return buffer_.size_; }
  unsigned depth() const { return buffer_.type_.depth_; }
  const void* data() const { return buffer_.ptr_; }
  void increase(std::size_t i);
  void increase(std::size_t i, double w);
  value_t value(std::size_t i) const;
  variance_t variance(std::size_t i) const;
  dynamic_storage& operator+=(const dynamic_storage&);

private:
  detail::buffer buffer_;

  friend struct python_access;

  template <class Archive>
  friend void serialize(Archive&, dynamic_storage&, unsigned);
};

inline
void dynamic_storage::increase(std::size_t i)
{
  switch (buffer_.type_.id_) {
    case -1: ++(buffer_.at<detail::weight_t>(i)); break;
    case 0: buffer_.initialize<uint8_t>(); // and fall through
    case 1: buffer_.increase<uint8_t>(i); break;
    case 2: buffer_.increase<uint16_t>(i); break;
    case 3: buffer_.increase<uint32_t>(i); break;
    case 4: buffer_.increase<uint64_t>(i); break;
    case 5: ++(buffer_.at<detail::mp_int>(i)); break;
  }
}

inline
void dynamic_storage::increase(std::size_t i, double w)
{
  buffer_.wconvert();
  buffer_.at<detail::weight_t>(i).add_weight(w);
}

inline
dynamic_storage& dynamic_storage::operator+=(const dynamic_storage& o)
{
  switch (o.buffer_.type_.id_) {
    case -1: detail::add_impl<decltype(buffer_), detail::weight_t>::apply(buffer_, o.buffer_); break;
    case 0: break;
    case 1: detail::add_impl<decltype(buffer_), uint8_t>::apply(buffer_, o.buffer_); break;
    case 2: detail::add_impl<decltype(buffer_), uint16_t>::apply(buffer_, o.buffer_); break;
    case 3: detail::add_impl<decltype(buffer_), uint32_t>::apply(buffer_, o.buffer_); break;
    case 4: detail::add_impl<decltype(buffer_), uint64_t>::apply(buffer_, o.buffer_); break;
    case 5: detail::add_impl<decltype(buffer_), detail::mp_int>::apply(buffer_, o.buffer_); break;
  }
  return *this;
}

inline
dynamic_storage::value_t dynamic_storage::value(std::size_t i) const
{
  switch (buffer_.type_.id_) {
    case -1: return buffer_.at<detail::weight_t>(i).w;
    case 0: break;
    case 1: return buffer_.at<uint8_t> (i);
    case 2: return buffer_.at<uint16_t>(i);
    case 3: return buffer_.at<uint32_t>(i);
    case 4: return buffer_.at<uint64_t>(i);
    case 5: return static_cast<double>(buffer_.at<detail::mp_int>(i));
  }
  return 0.0;
}

inline
dynamic_storage::variance_t dynamic_storage::variance(std::size_t i) const
{
  switch (buffer_.type_.id_) {
    case -1: return buffer_.at<detail::weight_t>(i).w2;
    case 0: break;
    case 1: return buffer_.at<uint8_t> (i);
    case 2: return buffer_.at<uint16_t>(i);
    case 3: return buffer_.at<uint32_t>(i);
    case 4: return buffer_.at<uint64_t>(i);
    case 5: return static_cast<double>(buffer_.at<detail::mp_int>(i));
  }
  return 0.0;
}

}
}

#endif
