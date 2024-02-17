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

#include "system_warning.hpp"

#include <array>
#include <stdexcept>

#include "display/string.hpp"
#include "hex.hpp"

namespace display
{
  system_warning::system_warning(const monero::hash& id, const std::uint64_t height, const std::size_t tx_count)
    : win_(make_center_box(characters{80}, characters{5}, kInfoText))
  {
    static constexpr const char header[] = "SYSTEM FAILURE";
    static constexpr const char txes_msg[] = "%u transaction(s) processed by Monero";
    static constexpr const char height_msg[] = "Case Number: %u";
    static constexpr const char id_msg[] = "Reference ID: %s";
    if (!win_)
      throw std::runtime_error{"Failed to create new curses window"};

    const auto hex = to_hex::array(id);

    std::array<char, 65> hack;
    std::copy(hex.begin(), hex.end(), hack.begin());
    hack[64] = 0;

    print_center(handle(), characters{static_length(header)}, 0, header);
    print_center(handle(), characters{static_length(txes_msg)}, 1, txes_msg, unsigned(tx_count));
    print_center(handle(), characters{static_length(height_msg) - 2 + 6}, 2, height_msg, unsigned(height));
    print_center(handle(), characters{static_length(id_msg) - 2 + 64}, 3, id_msg, hack.data());
  }
}
