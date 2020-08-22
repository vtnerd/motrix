// Copyright (c) 2020, Lee Clagett
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef WIRE_JSON_READ_HPP
#define WIRE_JSON_READ_HPP

#include <cstdint>
#include <rapidjson/reader.h>
#include <string>
#include <type_traits>

#include "byte_slice.hpp"
#include "span.hpp"
#include "wire/error.hpp"
#include "wire/field.hpp"
#include "wire/json/base.hpp"
#include "wire/traits.hpp"

namespace wire
{
  //! Reads JSON tokens one-at-a-time for DOMless parsing
  class json_reader
  {
    struct rapidjson_sax;

    byte_slice source_;
    span<const std::uint8_t> current_;
    std::string temp_str_;
    std::size_t depth_; //!< Tracks number of recursive objects and arrays
    rapidjson::Reader reader_;

    //! \throw std::system_error if max depth is reached
    void increment_depth();
    void decrement_depth() noexcept { --depth_; }

    void read_next_value(rapidjson_sax&);
    char get_next_token();
    span<const char> get_next_string();

    //! Skips next value. \throw wire::read_error if invalid JSON syntax.
    void skip_value();

  public:
    struct key_map
    {
      const char* name;
    };
    explicit json_reader(byte_slice source);

    json_reader(const json_reader&) = delete;
    json_reader& operator=(const json_reader&) = delete;

    //! \return Number of recursive objects and arrays
    std::size_t depth() const noexcept { return depth_; }

    //! \throw std::system_error if JSON parsing is incomplete.
    void check_complete() const;

    //! \throw std:system_error if next token not a boolean.
    bool boolean();
    //! \throw std::system_error if next token not an integer.
    std::intmax_t integer();
    //! \throw std::system_error if next token not an unsigned integer.
    std::uintmax_t unsigned_integer();
    //! \throw std::system_error if next token is not a number
    double real();

    //! \throw std::system error if next token not a string. \return Next string token.
    std::string string();
    //! \throw std::system_error if next token cannot be read as hex into `dest`.
    void binary(span<std::uint8_t> dest);
    //! \throw std::system_error if next token is not a string in `enums`. \return Index with `enums` of match.
    std::size_t enumeration(span<char const* const> enums);


    //! \throw std::system_error if next token not `[`.
    void start_array();

    //! Skips whitespace to next token. \return True if next token is eof or ']'.
    bool is_array_end(std::size_t count);

    //! \throw std::system_error if next token not `]`.
    void end_array() noexcept { decrement_depth(); }


    //! \throw std::system_error if next token not `{`.
    void start_object();

    /*! \throw std::system_error if next token not `,` or `}`.
      \return True if another value to read. */
    bool key(span<const key_map> map, std::size_t count, std::size_t& next);

    void end_object() noexcept { decrement_depth(); }
  };
} // wire

namespace read_json
{
  /*! Don't add a function called `read_bytes` to this namespace, it will prevent
      ADL lookup. ADL lookup delays the function searching until the template
      is used instead of when its defined. This allows the unqualified calls to
      `read_bytes` in this namespace to "find" user functions that are declared
      after these functions. */

  [[noreturn]] void throw_exception(const wire::error::schema code, const char* display, span<char const* const> names);

  //! \throw std::system_error if conversion from `source` to `T` fails.
  template<typename T>
  inline T to(byte_slice source)
  {
    T dest{};
    {
      wire::json_reader reader{std::move(source)};
      read_bytes(reader, dest);
      reader.check_complete();
    }
    return dest;
  }


  template<typename T>
  inline void array(wire::json_reader& source, T& dest)
  {
    source.start_array();

    dest.clear();
    for (std::size_t count = 0; !source.is_array_end(count); ++count)
    {
      dest.emplace_back();
      read_bytes(source, dest.back());
    }

    return source.end_array();
  }


  //! Tracks read status of every object field instance.
  template<typename T>
  class tracker
  {
    T field_;
    std::size_t our_index_;
    bool read_;

  public:
    static constexpr bool is_required() noexcept { return true; }
    static constexpr std::size_t count() noexcept { return 1; }

    explicit tracker(T field)
      : field_(std::move(field)), our_index_(0), read_(false)
    {}

    //! \return Field name if required and not read, otherwise `nullptr`.
    const char* name_if_missing() const noexcept
    {
      return (is_required() && !read_) ? field_.name : nullptr;
    }

    //! Set all entries in `map` related to this field (expand variant types!).
    template<std::size_t N>
    std::size_t set_mapping(std::size_t index, wire::json_reader::key_map (&map)[N])
    {
      our_index_ = index;
      map[index].name = field_.name;
      return index + count();
    }

    //! Try to read next value if `index` matches `this`. \return 0 if no match, 1 if optional field read, and 2 if required field read
    template<typename R>
    std::size_t try_read(R& source, const std::size_t index)
    {
      if (our_index_ != index)
        return 0;
      if (read_)
        throw_exception(wire::error::schema::invalid_key, "duplicate", {std::addressof(field_.name), 1});

      read_bytes(source, field_.get_value());
      read_ = true;
      return 1 + is_required();
    }
  };

  // `expand_tracker_map` writes all `tracker` types to a table

  template<std::size_t N>
  inline constexpr std::size_t expand_tracker_map(std::size_t index, const wire::json_reader::key_map (&)[N])
  {
    return index;
  }

  template<std::size_t N, typename T, typename... U>
  inline void expand_tracker_map(std::size_t index, wire::json_reader::key_map (&map)[N], tracker<T>& head, tracker<U>&... tail)
  {
    expand_tracker_map(head.set_mapping(index, map), map, tail...);
  }

  template<typename... T>
  inline void object(wire::json_reader& source, tracker<T>... fields)
  {
    static constexpr const std::size_t total_subfields = wire::sum(fields.count()...);
    static_assert(total_subfields < 100, "algorithm uses too much stack space and linear searching");

    source.start_object();
    std::size_t required = wire::sum(std::size_t(fields.is_required())...);

    wire::json_reader::key_map map[total_subfields] = {};
    expand_tracker_map(0, map, fields...);

    std::size_t next = 0;
    for (std::size_t count = 0; source.key(map, count, next); ++count)
    {
      switch (wire::sum(fields.try_read(source, next)...))
      {
      default:
      case 0:
        throw_exception(wire::error::schema::invalid_key, "bad map setup", nullptr);
        break;
      case 2:
        --required; /* fallthrough */
      case 1:
        break;
      }
    }

    if (required)
    {
      const char* missing[] = {fields.name_if_missing()...};
      throw_exception(wire::error::schema::missing_key, "", missing);
    }

    source.end_object();
  }
} // read_json

namespace wire
{
  // Don't call `read` directly in this namespace, do it from `read_json`.

  namespace integer
  {
    [[noreturn]] void throw_exception(std::intmax_t source, std::intmax_t min);
    [[noreturn]] void throw_exception(std::uintmax_t source, std::uintmax_t max);

    template<typename Target, typename U>
    inline Target convert_to(const U source)
    {
      using common = typename std::common_type<Target, U>::type;
      static constexpr const Target target_min = std::numeric_limits<Target>::min();
      static constexpr const Target target_max = std::numeric_limits<Target>::max();

      /* After optimizations, this is:
           * 1 check for unsigned -> unsigned (uint, uint)
           * 2 checks for signed -> signed (int, int)
           * 2 checks for signed -> unsigned-- (
           * 1 check for unsigned -> signed (uint, uint)

         Put `WIRE_DLOG_THROW` in cpp to reduce code/ASM duplication. Do not
         remove first check, signed values can be implicitly converted to
         unsigned in some checks. */
      if (!std::numeric_limits<Target>::is_signed && source < 0)
        throw_exception(std::intmax_t(source), std::intmax_t(0));
      else if (common(source) < common(target_min))
        throw_exception(std::intmax_t(source), std::intmax_t(target_min));
      else if (common(target_max) < common(source))
        throw_exception(std::uintmax_t(source), std::uintmax_t(target_max));

      return Target(source);
    }
  }

  template<typename T>
  inline T json::from_bytes(byte_slice source)
  {
    return read_json::to<T>(std::move(source));
  }


  inline void read_bytes(json_reader& source, bool& dest)
  {
    dest = source.boolean();
  }
  inline void read_bytes(json_reader& source, unsigned& dest)
  {
    dest = integer::convert_to<unsigned>(source.unsigned_integer());
  }
  inline void read_bytes(json_reader& source, unsigned long& dest)
  {
    dest = integer::convert_to<unsigned long>(source.unsigned_integer());
  }
  inline void read_bytes(json_reader& source, unsigned long long& dest)
  {
    dest = integer::convert_to<unsigned long long>(source.unsigned_integer());
  }

  template<typename T>
  inline typename std::enable_if<is_array<T>::value>::type read_bytes(json_reader& source, T& dest)
  {
    read_json::array(source, dest);
  }

  template<typename T>
  inline typename std::enable_if<is_blob<T>::value>::type read_bytes(json_reader& source, T& dest)
  {
    source.binary(as_mut_byte_span(dest));
  }

  template<typename... T>
  inline void object(json_reader& source, field_<T>... fields)
  {
    read_json::object(source, read_json::tracker<field_<T>>{std::move(fields)}...);
  }
} // wire

#endif // WIRE_JSON_READ_HPP
