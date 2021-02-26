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

#ifndef MOTRIX_DISPLAY_WINDOW_HPP
#define MOTRIX_DISPLAY_WINDOW_HPP

#include <memory>
#include <ncurses.h>
#include <stdexcept>

#include "display/colors.hpp"

namespace display
{
  struct window_deleter
  {
    void operator()(WINDOW* ptr) const noexcept
    {
      if (ptr)
        delwin(ptr);
    }
  };
  using window = std::unique_ptr<WINDOW, window_deleter>;

  struct centering
  {
    unsigned begin;
    unsigned characters;
  };
  struct characters
  {
    unsigned value;

    centering compute_center(const unsigned total) const noexcept
    {
      return {(total - value) / 2, value};
    }
  };
  struct percent
  {
    unsigned value;

    centering compute_center(const unsigned total) const noexcept;
  };

  void print_center(WINDOW*, const characters expected, const unsigned y, const char* fmt, ...);
  window do_make_center_box(centering x, centering y, color_pair color);

  template<typename X, typename Y>
  inline window make_center_box(const X x, const Y y, color_pair color)
  {
    return do_make_center_box(x.compute_center(COLS), y.compute_center(LINES), color);
  }
}

#endif // MOTRIX_DISPLAY_WINDOW_HPP
