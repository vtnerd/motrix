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

#ifndef MOTRIX_PUB_HPP
#define MOTRIX_PUB_HPP

#include <cstdint>
#include <vector>

#include "byte_slice.hpp"
#include "monero_data.hpp"
#include "wire/json/fwd.hpp"
#include "wire/vector.hpp"

namespace pub
{
  //! A ZMQ/Pub message from the Monero daemon.
  struct message
  {
    //! Construct from raw ZMQ/Sub socket message
    explicit message(byte_slice&& raw) noexcept;

    byte_slice topic;
    byte_slice contents;
  };

  struct minimal_chain
  {
    std::uint64_t first_height;
    std::vector<monero::hash> ids;
    monero::hash first_prev_id;
  };
  void read_bytes(wire::json_reader&, minimal_chain&);

  using full_chain = std::vector<monero::block>;
  using minimal_txpool = std::vector<monero::minimal_tx>;
}

#endif // MOTRIX_PUB_HPP
