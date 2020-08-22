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

#ifndef MOTRIX_METHOD_HPP
#define MOTRIX_METHOD_HPP

#include <cstdint>
#include <vector>

#include "monero_data.hpp"
#include "wire/json/fwd.hpp"

namespace method
{
  struct get_info
  {
    struct data
    {
      data() = delete;
      std::uint64_t height;
      std::uint64_t target_height;
      std::uint64_t outgoing_connections_count;
      std::uint64_t incoming_connections_count;
      monero::hash top_block_hash;
      bool mainnet;
      bool testnet;
      bool stagenet;
    };

    static constexpr const char* name() noexcept { return "get_info"; }
    struct request {};
    struct response
    {
      response() = delete;
      data info;
    };
  };
  void write_bytes(wire::json_writer&, const get_info::request&);
  void read_bytes(wire::json_reader&, get_info::response&);

  struct get_transaction_pool
  {
    struct entry
    {
      entry()
        : tx_hash{}
      {}

      monero::hash tx_hash;
    };

    static constexpr const char* name() noexcept { return "get_transaction_pool"; }
    struct request {};
    struct response
    {
      response() = delete;
      std::vector<entry> transactions;
    };
  };
  void write_bytes(wire::json_writer&, const get_transaction_pool::request&);
  void read_bytes(wire::json_reader&, get_transaction_pool::response&);
}

#endif // MOTRIX_METHOD_HPP
