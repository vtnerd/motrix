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

#include "display/sync_meter.hpp"

#include <algorithm>
#include <cstring>
#include <random>
#include <stdexcept>

#include "display/colors.hpp"
#include "display/loading_messages.hpp"
#include "display/string.hpp"

namespace
{
  constexpr const std::chrono::seconds minimum_footer_time{3};
}

namespace display
{
  sync_meter::sync_meter()
    : win_(display::make_center_box(percent{75}, characters{8}, kInfoText)),
      messages_(),
      current_(0),
      target_(0),
      last_footer_(std::chrono::steady_clock::time_point::min()),
      progress_(0)
  {
    if (!win_)
      throw std::logic_error{"make_center_box returned nullptr"};

    static_assert(100 <= all_messages.size(), "missing loading messages");

    messages_.resize(all_messages.size());
    std::copy(all_messages.begin(), all_messages.end(), messages_.begin());

    std::mt19937 rand{std::random_device{}()};
    std::shuffle(messages_.begin(), messages_.end(), rand);

    messages_.resize(100);
  }

  sync_meter::~sync_meter() noexcept
  {}

  void sync_meter::set_header(const char* chain_type, const char* address)
  {
    static constexpr const char header_fmt[] = "Watching %s chain sync @ %s";

    const unsigned header_length =
      static_length(header_fmt) - 4 + std::strlen(chain_type) + std::strlen(address);
    print_center(handle(), characters{header_length}, 0, header_fmt, chain_type, address);
  }

  void sync_meter::set_progress(std::uint64_t current, const std::uint64_t target)
  {
    static constexpr const char footer_fmt[] = "... %s ...";

    current = std::min(current, target);
    if (current_ == current && target_ == target)
      return;

    int lines, columns;
    getmaxyx(handle(), lines, columns);

    const double progress = double(current) / target;
    const unsigned progress_int = std::min(100u, unsigned(progress * 100));
    const unsigned draw_area = std::max(2, columns) - 2;
    const unsigned split = std::min(draw_area, unsigned(progress * draw_area));
    const unsigned tail = draw_area - split;
    
    mvwprintw(handle(), 3, std::max(3u, (draw_area / 2u)), "%u%%", progress_int);

    mvwchgat(handle(), 2, 1, split, 0, display::kProgressMeterHighlight, nullptr);
    mvwchgat(handle(), 2, split + 1, tail, 0, display::kProgressMeterNoHighlight, nullptr);

    mvwchgat(handle(), 3, 1, split, 0, display::kProgressMeterHighlight, nullptr);
    mvwchgat(handle(), 3, split + 1, tail, 0, display::kProgressMeterNoHighlight, nullptr);
    
    mvwchgat(handle(), 4, 1, split, 0, display::kProgressMeterHighlight, nullptr);
    mvwchgat(handle(), 4, split + 1, tail, 0, display::kProgressMeterNoHighlight, nullptr);

    const auto now = std::chrono::steady_clock::now();
    if (progress_ < progress && last_footer_ + minimum_footer_time <= now)
    {
      mvwhline(handle(), 6, 1, ' ', draw_area);
        
      const char* next_display = messages_.at(progress - 1);
      const unsigned footer_length =
	display::static_length(footer_fmt) - 2 + std::strlen(next_display);
      print_center(handle(), characters{footer_length}, 6, footer_fmt, next_display);
      
      progress_ = progress;
      last_footer_ = now;
    }

    current_ = current;
    target_ = target;
  }
}
