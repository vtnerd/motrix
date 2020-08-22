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

#include "wire/json/write.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>

namespace wire
{
  void json_writer::integer(const int source)
  {
    writer_.Int(source);
  }
  void json_writer::integer(const std::intmax_t source)
  {
    static_assert(std::numeric_limits<std::int64_t>::min() <= std::numeric_limits<std::intmax_t>::min(), "too small");
    static_assert(std::numeric_limits<std::intmax_t>::max() <= std::numeric_limits<std::int64_t>::max(), "too large");
    writer_.Int64(source);
  }
  void json_writer::unsigned_integer(const unsigned source)
  {
    writer_.Uint(source);
  }
  void json_writer::unsigned_integer(const std::uintmax_t source)
  {
    static_assert(std::numeric_limits<std::uintmax_t>::max() <= std::numeric_limits<std::uint64_t>::max(), "too large");
    writer_.Uint64(source);
  }

  void json_writer::string(span<const char> source)
  {
    writer_.String(source.data(), source.size());
  }

  void json_writer::start_array()
  {
    writer_.StartArray();
  }
  void json_writer::end_array()
  {
    writer_.EndArray();
  }

  void json_writer::start_object()
  {
    writer_.StartObject();
  }
  void json_writer::key(const char* str)
  {
    writer_.Key(str, std::strlen(str));
  }
  void json_writer::end_object()
  {
    writer_.EndObject();
  }

  byte_slice json_writer::take_json()
  {
    if (!writer_.IsComplete())
      throw std::logic_error{"json_writer::take_json() failed with incomplete JSON tree"};

    byte_slice out{std::move(bytes_)};
    writer_.Reset(bytes_);
    return out;
  }


  void write_bytes(json_writer& dest, const char* source)
  {
    dest.string({source, std::strlen(source)});
  }
}
