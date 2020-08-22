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

#include "wire/json/read.hpp"

#include <algorithm>
#include <rapidjson/memorystream.h>
#include <stdexcept>

#include "expect.hpp"
#include "hex.hpp"
#include "wire/error.hpp"
#include "wire/json/error.hpp"

namespace
{
  //! Maximum number of bytes to display "near" JSON error.
  constexpr const std::size_t snippet_size = 30;

  //! Maximum depth for both objects and arrays before erroring
  constexpr const std::size_t max_json_read_depth = 100;

  struct json_default_reject : rapidjson::BaseReaderHandler<rapidjson::UTF8<>, json_default_reject>
  {
    bool Default() const noexcept { return false; }
  };

  //! \throw std::system_error by converting `code` into a std::error_code
  [[noreturn]] void throw_json_error(const rapidjson::Reader& reader, const wire::error::schema expected)
  {
    const rapidjson::ParseErrorCode parse_error = reader.GetParseErrorCode();
    switch (parse_error)
    {
    default:
      MOT_THROW(wire::error::rapidjson_e(parse_error), nullptr);
    case rapidjson::kParseErrorNone:
    case rapidjson::kParseErrorTermination: // the handler returned false
      break;
    }

    MOT_THROW(expected, nullptr);
  }
}

namespace wire
{
  struct json_reader::rapidjson_sax
  {
    struct string_contents
    {
       const char* ptr;
       std::size_t length;
    };

    std::string* temp_str;
    union
    {
      bool boolean;
      std::intmax_t integer;
      std::uintmax_t unsigned_integer;
      double number;
      string_contents string;
    } value;

    error::schema expected_;
    bool negative;

    rapidjson_sax(error::schema expected, std::string* temp_str = nullptr) noexcept
      : temp_str(temp_str), expected_(expected), negative(false)
    {}

    bool Null() const noexcept
    {
      return expected_ == error::schema::none;
    }

    bool Bool(bool i) noexcept
    {
      value.boolean = i;
      return expected_ == error::schema::boolean || expected_ == error::schema::none;
    }

    bool Int(int i) noexcept
    {
      return Int64(i);
    }
    bool Uint(unsigned i) noexcept
    {
      return Uint64(i);
    }
    bool Int64(std::int64_t i) noexcept
    {
      negative = true;
      switch(expected_)
      {
      default:
        return false;
      case error::schema::integer:
        value.integer = i;
        break;
      case error::schema::number:
        value.number = i;
        break;
      case error::schema::none:
        break;
      }
      return true;
    }
    bool Uint64(std::uint64_t i) noexcept
    {
      switch (expected_)
      {
      default:
        return false;
      case error::schema::integer:
        value.unsigned_integer = i;
        break;
      case error::schema::number:
        value.number = i;
        break;
      case error::schema::none:
        break;
      }
      return true;
    }

    bool Double(double i) noexcept
    {
      value.number = i;
      return expected_ == error::schema::number || expected_ == error::schema::none;
    }

    bool RawNumber(const char*, std::size_t, bool) const noexcept
    {
      return false;
    }

    bool String(const char* str, const std::size_t length, const bool copy) noexcept
    {
      if (expected_ == error::schema::string)
      {
	if (copy)
	{
	  if (!temp_str)
	    return false;
	  temp_str->assign(str, length);
	  str = temp_str->c_str();
	}
	value.string = {str, length};
	return true;
      }
      return expected_ == error::schema::none;
    }
    bool Key(const char* str, const std::size_t length, const bool copy)
    {
      return String(str, length, copy);
    }

    bool StartArray() const noexcept { return expected_ == error::schema::none; }
    bool EndArray(std::size_t) const noexcept { return expected_ == error::schema::none; }
    bool StartObject() const noexcept { return expected_ == error::schema::none; }
    bool EndObject(std::size_t) const noexcept { return expected_ == error::schema::none; }
  };

  void json_reader::increment_depth()
  {
    if (++depth_ == max_json_read_depth)
      MOT_THROW(error::schema::maximum_depth, nullptr);
  }

  void json_reader::read_next_value(rapidjson_sax& handler)
  {
    rapidjson::MemoryStream stream{reinterpret_cast<const char*>(current_.data()), current_.size()};
    if (!reader_.Parse<rapidjson::kParseStopWhenDoneFlag>(stream, handler))
      throw_json_error(reader_, handler.expected_);
    current_.remove_prefix(stream.Tell());
  }

  char json_reader::get_next_token()
  {
    rapidjson::MemoryStream stream{reinterpret_cast<const char*>(current_.data()), current_.size()};
    rapidjson::SkipWhitespace(stream);
    current_.remove_prefix(stream.Tell());
    return stream.Peek();
  }

  span<const char> json_reader::get_next_string()
  {
    if (get_next_token() != '"')
      MOT_THROW(error::schema::string, nullptr);
    current_.remove_prefix(1);

    void const* const end = std::memchr(current_.data(), '"', current_.size());
    if (!end)
      MOT_THROW(error::rapidjson_e(rapidjson::kParseErrorStringMissQuotationMark), nullptr);

    char const* const begin = reinterpret_cast<const char*>(current_.data());
    const std::size_t length = current_.remove_prefix(static_cast<const std::uint8_t*>(end) - current_.data() + 1);
    return {begin, length - 1};
  }

  void json_reader::skip_value()
  {
    rapidjson_sax accept_all{error::schema::none};
    read_next_value(accept_all);
  }

  json_reader::json_reader(byte_slice source)
    : source_(std::move(source)),
      current_(source_.data(), source_.size()),
      temp_str_(),
      depth_(0),
      reader_()
  {}

  void json_reader::check_complete() const
  {
    if (depth())
      MOT_THROW(error::rapidjson_e(rapidjson::kParseErrorUnspecificSyntaxError), "Unexpected end");
  }

  bool json_reader::boolean()
  {
    rapidjson_sax json_bool{error::schema::boolean};
    read_next_value(json_bool);
    return json_bool.value.boolean;
  }

  std::intmax_t json_reader::integer()
  {
    rapidjson_sax json_int{error::schema::integer};
    read_next_value(json_int);
    if (json_int.negative)
      return json_int.value.integer;
    return integer::convert_to<std::intmax_t>(json_int.value.unsigned_integer);
  }

  std::uintmax_t json_reader::unsigned_integer()
  {
    rapidjson_sax json_uint{error::schema::integer};
    read_next_value(json_uint);
    if (!json_uint.negative)
      return json_uint.value.unsigned_integer;
    return integer::convert_to<std::uintmax_t>(json_uint.value.integer);
  }

  double json_reader::real()
  {
    rapidjson_sax json_number{error::schema::number};
    read_next_value(json_number);
    return json_number.value.number;
  }

  std::string json_reader::string()
  {
    rapidjson_sax json_string{error::schema::string, std::addressof(temp_str_)};
    read_next_value(json_string);
    return std::string{json_string.value.string.ptr, json_string.value.string.length};
  }

  void json_reader::binary(span<std::uint8_t> dest)
  {
    const span<const char> value = get_next_string();
    if (!from_hex::to_buffer(dest, value))
      MOT_THROW(error::schema::fixed_binary, ("of size" + std::to_string(dest.size() * 2) + " but got " + std::to_string(value.size())).c_str());
  }

  std::size_t json_reader::enumeration(span<char const* const> enums)
  {
    rapidjson_sax json_enum{error::schema::string, std::addressof(temp_str_)};
    read_next_value(json_enum);

    for (std::size_t i = 0; i < enums.size(); ++i)
    {
      const std::size_t current_length = std::strlen(enums[i]);
      if (json_enum.value.string.length == current_length && std::memcmp(json_enum.value.string.ptr, enums[i], current_length) == 0)
        return i;
    }

    MOT_THROW(error::schema::enumeration, (std::string{json_enum.value.string.ptr} + " is not a valid enum").c_str());
    return enums.size();
  }

  void json_reader::start_array()
  {
    if (get_next_token() != '[')
      MOT_THROW(error::schema::array, nullptr);
    current_.remove_prefix(1);
    increment_depth();
  }

  bool json_reader::is_array_end(const std::size_t count)
  {
    const char next = get_next_token();
    if (next == 0)
      MOT_THROW(error::rapidjson_e(rapidjson::kParseErrorArrayMissCommaOrSquareBracket), nullptr);
    if (next == ']')
    {
      current_.remove_prefix(1);
      return true;
    }

    if (count)
    {
      if (next != ',')
        MOT_THROW(error::rapidjson_e(rapidjson::kParseErrorArrayMissCommaOrSquareBracket), nullptr);
      current_.remove_prefix(1);
    }
    return false;
  }

  void json_reader::start_object()
  {
    if (get_next_token() != '{')
      MOT_THROW(error::schema::object, nullptr);
    current_.remove_prefix(1);
    increment_depth();
  }

  bool json_reader::key(const span<const key_map> map, std::size_t count, std::size_t& index)
  {
    rapidjson_sax json_key{error::schema::string, std::addressof(temp_str_)};
    const auto process_key = [map] (const rapidjson_sax::string_contents value)
    {
      for (std::size_t i = 0; i < map.size(); ++i)
      {
	const std::size_t current_length = std::strlen(map[i].name);
        if (value.length == current_length && std::memcmp(value.ptr, map[i].name, current_length) == 0)
          return i;
      }
      return map.size();
    };

    index = map.size();
    for (;;)
    {
      // check for object or text end
      const char next = get_next_token();
      if (next == 0)
        MOT_THROW(error::rapidjson_e(rapidjson::kParseErrorObjectMissCommaOrCurlyBracket), nullptr);
      if (next == '}')
      {
        current_.remove_prefix(1);
        return false;
      }

      // parse next field token
      if (count)
      {
        if (next != ',')
          MOT_THROW(error::rapidjson_e(rapidjson::kParseErrorObjectMissCommaOrCurlyBracket), nullptr);
        current_.remove_prefix(1);
      }
      ++count;

      // parse key
      read_next_value(json_key);
      index = process_key(json_key.value.string);
      if (get_next_token() != ':')
        MOT_THROW(error::rapidjson_e(rapidjson::kParseErrorObjectMissColon), nullptr);
      current_.remove_prefix(1);

      // parse value
      if (index != map.size())
        break;
      skip_value();
    }
    return true;
  }

  [[noreturn]] void integer::throw_exception(std::intmax_t source, std::intmax_t min)
  {
    MOT_THROW(error::schema::larger_integer, (std::to_string(source) + " given when " + std::to_string(min) + " is minimum permitted").c_str());
  }
  [[noreturn]] void integer::throw_exception(std::uintmax_t source, std::uintmax_t max)
  {
    MOT_THROW(error::schema::smaller_integer, (std::to_string(source) + " given when " + std::to_string(max) + "is maximum permitted").c_str());
  }
}

[[noreturn]] void read_json::throw_exception(const wire::error::schema code, const char* display, span<char const* const> names)
{
  const char* name = nullptr;
  for (const char* elem : names)
  {
    if (elem != nullptr)
    {
      name = elem;
      break;
    }
  }
  MOT_THROW(code, (std::string{display} + (name ? name : "")).c_str());
}
