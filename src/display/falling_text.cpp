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

#include "display/falling_text.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>

#include "display/colors.hpp"

namespace
{
  constexpr const unsigned text_size = 40;
  constexpr const unsigned group_count = 8;
  constexpr const unsigned color_count = 2;
  constexpr const unsigned screen_fill_percent = 60;
  constexpr const std::chrono::milliseconds text_fall_delay{80};
}

namespace display
{
  struct falling_text_location
  {
    falling_text_location()
      : x(std::numeric_limits<int>::max()), y(x), old_x(x), old_y(x)
    {}

    int x;
    int y;
    int old_x;
    int old_y;
  };
  struct falling_text_group
  {
    falling_text_group()
      : text(), count(text.size())
    {}

    std::array<char, text_size> text;
    unsigned char count;
  };

  namespace
  {
    void print_active_character(WINDOW* win, const falling_text_location& loc, const falling_text_group& group) noexcept
    {
      if (group.count < group.text.size())
        mvwaddch(win, loc.y, loc.x, group.text[group.count]);
    }
  }

  falling_text::falling_text()
    : win_(newwin(LINES, COLS, 0, 0)),
      groups_(),
      locations_(),
      next_(clock::time_point::min()),
      offset_(0),
      rand_(std::random_device{}())
  {
    if (!win_)
      throw std::runtime_error{"failed to create ncurses window"};

    wbkgd(handle(), COLOR_PAIR(display::kFallingText1));

    int lines, cols;
    getmaxyx(win_.get(), lines, cols);

    groups_.resize(group_count);
    locations_.resize(std::max(group_count, percent{screen_fill_percent}.compute_center(unsigned(cols)).characters));
    for (std::size_t i = 0; i < groups_.size(); ++i)
      groups_[i].count = std::numeric_limits<unsigned char>::max() - ((text_size * i) / group_count) - 1;
  }

  falling_text::~falling_text() noexcept
  {}

  void falling_text::add_text(const std::array<char, 41>& src)
  {
    int lines, cols;
    getmaxyx(handle(), lines, cols);

    std::uniform_int_distribution<int> select_line{
      -1, text_size <= lines ? int(lines - text_size) : lines
    };
    std::uniform_int_distribution<int> select_column{0, cols};

    falling_text_group& group = groups_.at(offset_);
    std::copy(src.begin(), src.end() - 1, groups_.at(offset_).text.data());
    group.count = std::numeric_limits<unsigned char>::max();

    for (std::size_t i = 0; i < locations_.size(); ++i)
    {
      if (i % group_count != offset_)
        continue;

      falling_text_location& current = locations_[i];
      current.old_x = current.x;
      current.old_y = current.y - group.text.size();
      current.x = select_column(rand_);
      current.y = select_line(rand_);
    }

    offset_ = ++offset_ % group_count;
  }

  bool falling_text::draw_next(const clock::time_point now)
  {
    falling_text_group& active = groups_.at(offset_);
    if (active.text.size() == active.count || active.count == std::numeric_limits<unsigned char>::max() - 1)
      return false;

    const std::size_t color_range = locations_.size() / color_count;

    for (unsigned color = 0; color < color_count; ++color)
    {
      if (color)
        wattron(handle(), COLOR_PAIR(display::kFallingText1 + color));

      for (std::size_t i = color_range * color; i < locations_.size(); ++i)
      {
        const falling_text_location& loc = locations_[i];
        mvwaddch(handle(), loc.old_y, loc.old_x, ' ');
        print_active_character(handle(), loc, groups_[i % group_count]);
      }

      if (color)
        wattroff(handle(), COLOR_PAIR(display::kFallingText1 + color));
    }

    for (falling_text_group& group : groups_)
      ++group.count;

    for (unsigned color = 0; color < color_count; ++color)
    {
      if (color)
        wattron(handle(), A_BOLD | COLOR_PAIR(display::kFallingText1 + color));
      else
        wattron(handle(), A_BOLD);

      for (std::size_t i = color_range * color; i < color_range * (color + 1); ++i)
      {
        falling_text_location& loc = locations_[i];
        ++loc.y;
        ++loc.old_y;
        print_active_character(handle(), loc, groups_[i % group_count]);
      }

      if (color)
        wattroff(handle(), A_BOLD | COLOR_PAIR(display::kFallingText1 + color));
      else
        wattroff(handle(), A_BOLD);
    }

    next_ = now + text_fall_delay;
    return true;
  }
}
