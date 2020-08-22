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

#ifndef WIRE_JSON_WRITE_HPP
#define WIRE_JSON_WRITE_HPP

#include <cstdint>
#include <rapidjson/writer.h>
#include <type_traits>

#include "byte_stream.hpp"
#include "hex.hpp"
#include "span.hpp"
#include "wire/field.hpp"
#include "wire/json/base.hpp"
#include "wire/traits.hpp"

namespace wire
{
  //! Writes JSON tokens one-at-a-time for DOMless output.
  class json_writer
  {
    byte_stream bytes_;
    rapidjson::Writer<byte_stream> writer_;

  public:
    json_writer()
      : bytes_(), writer_(bytes_)
    {}

    // These convenience functions exist so that the inlined rapidjson code only
    // gets generated once

    void integer(int);
    void integer(std::intmax_t);

    void unsigned_integer(unsigned);
    void unsigned_integer(std::uintmax_t);

    void string(span<const char>);

    void start_array();
    void end_array();

    void start_object();
    void key(const char*);
    void end_object();


    //! \return JSON document \post `writer.reset(...);`
    byte_slice take_json();
  };
}

namespace write_json
{
  /*! Don't add a function called `write_bytes` to this namespace, it will
      prevent ADL lookup. ADL lookup delays the function searching until the
      template is used instead of when its defined. This allows the unqualified
      calls to `write_bytes` in this namespace to "find" user functions that are
      declared after these functions. */

  template<typename T>
  inline byte_slice from(const T& value)
  {
    wire::json_writer dest{};
    write_bytes(dest, value);
    return dest.take_json();
  }

  template<typename T>
  inline void array(wire::json_writer& dest, const T& source)
  {
    dest.start_array();
    for (const auto& elem : source)
      write_bytes(dest, elem);
    dest.end_array();
  }

  template<typename T>
  inline bool field(wire::json_writer& dest, const wire::field_<T> elem)
  {
    dest.key(elem.name);
    write_bytes(dest, elem.get_value());
    return true;
  }
} // write_json

namespace wire
{
  template<typename T>
  byte_slice json::to_bytes(const T& source)
  {
    return write_json::from(source);
  }


  inline void write_bytes(json_writer& dest, unsigned source)
  {
    dest.unsigned_integer(source);
  }
  inline void write_bytes(json_writer& dest, std::uintmax_t source)
  {
    dest.unsigned_integer(source);
  }
  void write_bytes(json_writer& dest, const char* source);

  template<typename T>
  inline typename std::enable_if<is_blob<T>::value>::type write_bytes(json_writer& dest, const T& source)
  {
    const auto bytes = to_hex::array(source);
    dest.string({bytes.data(), bytes.size()});
  }

  template<typename T>
  inline typename std::enable_if<is_array<T>::value>::type write_bytes(json_writer& dest, const T& source)
  {
    return write_json::array(dest, source);
  }


  template<typename... T>
  inline void object(json_writer& dest, const field_<T>... fields)
  {
    dest.start_object();
    const bool dummy[] = {write_json::field(dest, fields)...};
    dest.end_object();
  }
}

#endif // WIRE_JSON_WRITE_HPP
