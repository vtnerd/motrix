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

#include "method.hpp"

#include "wire/field.hpp"
#include "wire/json/read.hpp"
#include "wire/json/write.hpp"
#include "wire/vector.hpp"

namespace method
{
  void write_bytes(wire::json_writer& dest, const get_info::request&)
  {
    wire::object(dest);
  }
  void read_bytes(wire::json_reader& source, get_info::data& self)
  {
    wire::object(
      source,
      WIRE_FIELD(height),
      WIRE_FIELD(target_height),
      WIRE_FIELD(outgoing_connections_count),
      WIRE_FIELD(incoming_connections_count),
      WIRE_FIELD(top_block_hash),
      WIRE_FIELD(mainnet),
      WIRE_FIELD(testnet),
      WIRE_FIELD(stagenet)
    );
  }
  void read_bytes(wire::json_reader& source, get_info::response& self)
  {
    wire::object(source, WIRE_FIELD(info));
  }

  void write_bytes(wire::json_writer& dest, const get_transaction_pool::request&)
  {
    wire::object(dest);
  }
  void read_bytes(wire::json_reader& source, get_transaction_pool::entry& self)
  {
    wire::object(source, WIRE_FIELD(tx_hash));
  }
  void read_bytes(wire::json_reader& source, get_transaction_pool::response& self)
  {
    wire::object(source, WIRE_FIELD(transactions));
  }
}
