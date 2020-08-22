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

#include "display/window.hpp"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <stdexcept>

namespace display
{
  namespace
  {
    void do_print_center(WINDOW* win, const characters expected, const unsigned y, const char* fmt, va_list args) noexcept
    {
      assert(win != nullptr);

      int lines, cols;
      getmaxyx(win, lines, cols);

      const auto text_start = expected.compute_center(std::max(0, cols));

      wmove(win, y, text_start.begin);
      vw_printw(win, fmt, args);
    }
  }

  centering percent::compute_center(const unsigned total) const noexcept
  {
    const unsigned clamped = std::min(100u, value);
    const unsigned characters = (static_cast<unsigned long long>(clamped) * total) / 100u;
    const unsigned offset = (total - characters) / 2;
    return {offset, characters};
  }

  void print_center(WINDOW* win, const characters expected, const unsigned y, const char* fmt, ...)
  {
    if (!win)
      throw std::logic_error{"display_center given nullptr"};

    std::va_list args{};
    va_start(args, fmt);
    do_print_center(win, expected, y, fmt, args);
    va_end(args);
  }

  window do_make_center_box(const centering x, const centering y, const color_pair color)
  {
    window win{newwin(y.characters, x.characters, y.begin, x.begin)};

    if (!win)
      throw std::runtime_error{"Failed to create ncurses window"};

    wbkgd(win.get(), COLOR_PAIR(color));
    box(win.get(), 0, 0);

    return win;
  }
}
