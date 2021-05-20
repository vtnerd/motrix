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

#include <thread>
#include "engine.hpp"

#include <array>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <ncurses.h>
#include <poll.h>
#include <random>
#include <string>
#include <unistd.h>

#include "error.hpp"
#include "expect.hpp"
#include "display/colors.hpp"
#include "display/exit.hpp"
#include "display/falling_text.hpp"
#include "display/sync_meter.hpp"
#include "display/system_warning.hpp"
#include "method.hpp"
#include "pub.hpp"
#include "rpc/json.hpp"
#include "wire/json/read.hpp"
#include "zmq.hpp"

//! Executes the POSIX function. \throw std::system_error on failures.
#define POSIX_UNWRAP(...)                                      \
  if ((__VA_ARGS__) < 0)                                       \
    MOT_THROW(make_error_code(std::errc(errno)), #__VA_ARGS__)

//! Executes the curses function. \throw std::system_error on failure.
#define CURSES_UNWRAP(...)                                      \
  if ((__VA_ARGS__) == ERR)                                     \
    MOT_THROW(make_error_code(std::errc(errno)), #__VA_ARGS__)

//! Returns on shutdown error codes, and throws other
#define ETERM_CHECK(result, msg)                 \
  if (!result)                                   \
  {                                              \
    if (result == ::zmq::make_error_code(ETERM)) \
      return;                                    \
    MOT_THROW(result.error(), msg);              \
  }

int engine::exit_fd_{-1};
std::atomic<bool> engine::running_{true};

namespace
{
  //! Maximum number of block hashes to keep around for "falling text" during sync
  constexpr const std::size_t max_block_hash_buffer = 50;

  //! Delay when showing new block "system warning"
  constexpr const std::chrono::seconds block_display_time{16};

  //! ZMQ_REQ socket temporarily closed if daemon this many blocks behind
  constexpr const std::uint64_t big_sync_interval = 5000;

  //! Update blockchain target height at this frequency while syncing
  constexpr const std::chrono::minutes target_sync_interval{15};

  //! Re-check daemon status if no pub events within this interval. Watching synced daemon should still have txpool events.
  constexpr const std::chrono::minutes no_pubs_timeout{5};

  constexpr const char full_chain_topic[] = "json-full-chain_main";
  constexpr const char minimal_chain_topic[] = "json-minimal-chain_main";
  constexpr const char minimal_txpool_topic[] = "json-minimal-txpool_add";

  //! Sleeps without blocking UI.
  void wait_for(std::chrono::milliseconds delay)
  {
    pollfd item[1] = {{engine::exit_fd(), POLLIN}};
    MOT_UNWRAP(zmq::retry_op(poll, item, 1, delay.count()));
  }

  template<std::size_t N>
  void topic_change(void* socket, int option, const char (&topic)[N])
  {
    if (zmq_setsockopt(socket, option, topic, N - 1) != 0)
      MOT_ZMQ_THROW("Subscription change failed");
  }

  template<std::size_t N>
  bool matches(const byte_slice& actual, const char (&expected)[N]) noexcept
  {
    return actual.size() == N - 1 && std::memcmp(actual.data(), expected, N - 1) == 0;
  }

  struct base85
  {
    std::array<char, 41> text;
    bool cached;
  };

  struct motrix
  {
    explicit motrix(const char* pub_address, const char* rpc_address) :
      rpc_address(rpc_address),
      ctx(zmq_init(1)),
      sub(),
      daemon_height(0),
      target_height(0),
      text(),
      rand_(std::random_device{}()),
      last_block_id{}
    {
      if (!ctx)
        MOT_ZMQ_THROW("Failed to create context");

      sub = zmq::connect(ctx.get(), ZMQ_SUB, pub_address);
      if (!sub)
        throw std::logic_error{"zmq::connect returned nullptr"};

      // permanently subscribed to this topic
      topic_change(sub.get(), ZMQ_SUBSCRIBE, minimal_chain_topic);
    }

    const char* rpc_address;
    const zmq::context ctx;
    zmq::socket sub;
    zmq::socket rpc;
    std::uint64_t daemon_height;
    std::uint64_t target_height;
    display::falling_text text;
    std::mt19937 rand_;
    monero::hash last_block_id;
  };

  void update_screen(const motrix& state, WINDOW* overlay)
  {
    wnoutrefresh(state.text.handle());
    if (overlay)
    {
      redrawwin(overlay);
      wnoutrefresh(overlay);
    }
    doupdate();
  }

  void to_z85(std::array<char, 41>& out, const monero::hash& in)
  {
    if (!zmq_z85_encode(out.data(), in.data, sizeof(in.data)))
      throw std::runtime_error{"z85 encoding failed"};
  }

  template<typename T>
  expect<pub::message> wait_for_pub(motrix& state, T& hashes, WINDOW* overlay)
  {
    bool init = false;
    typename T::iterator next;
    std::chrono::steady_clock::duration slippage{0};

    const auto start = std::chrono::steady_clock::now();
    auto now = start;
    while (engine::is_running())
    {
      if (no_pubs_timeout <= now - start)
        return pub::message{byte_slice{}}; // hackey but both callers handle it as desired;

      if (state.text.next_fall() <= now)
      {
        while (!state.text.draw_next(now))
        {
          if (!hashes.empty())
          {
            if (!init)
            {
              init = true;
              std::uniform_int_distribution<std::size_t> dist{0, hashes.size()};
              next = hashes.begin();
              std::advance(next, dist(state.rand_));
            }

            if (next == hashes.end())
              next = hashes.begin();
            if (!next->second.cached)
              to_z85(next->second.text, next->first);

            next->second.cached = true;
            state.text.add_text(next->second.text);
          }
          else // nothing in mempool or recent block list to show
          {
            std::array<char, 41> text;
            to_z85(text, state.last_block_id);
            state.text.add_text(text);
          }
        }
      }

      update_screen(state, overlay);

      {
        using namespace std::chrono;
        const auto text_delay = (state.text.next_fall() - steady_clock::now()) - slippage;

        zmq_pollitem_t item[1] = {{state.sub.get(), 0, ZMQ_POLLIN}};
        if (steady_clock::duration{0} < text_delay)
          MOT_CHECK(zmq::retry_op(zmq_poll, item, 1, duration_cast<milliseconds>(text_delay).count()));
        else
          item[0].revents = POLLIN;

        if (item[0].revents & POLLIN)
        {
          expect<byte_slice> event = zmq::receive(state.sub.get(), ZMQ_DONTWAIT);
          if (event)
            return pub::message{std::move(*event)};
          else if (steady_clock::duration{0} < text_delay || event != zmq::make_error_code(EAGAIN))
            return event.error();
        }
        now = steady_clock::now();
        slippage = (now - state.text.next_fall());
      }
    }
    return zmq::make_error_code(ETERM);
  }

  void sync_mempool(motrix& state, std::map<monero::hash, base85>& txpool)
  {
    txpool.clear();

    if (!state.rpc && !(state.rpc = zmq::connect(state.ctx.get(), ZMQ_REQ, state.rpc_address)))
      throw std::logic_error{"zmq::connect returned nullptr"};

    const auto pool = zmq::invoke<rpc::json<method::get_transaction_pool>>(state.rpc.get());
    ETERM_CHECK(pool, "Failed to get current transaction pool");

    for (const auto& tx : pool->result.transactions)
      txpool.emplace(tx.tx_hash, base85{});

    state.rpc.reset();
  }

  void show_system_warning(motrix& state, monero::hash& head_out, const monero::hash& expected_head, const std::size_t tx_count, std::map<monero::hash, base85>& txpool)
  {
    const display::system_warning warning{state.last_block_id, state.daemon_height, tx_count};
    update_screen(state, warning.handle());

    if (head_out != expected_head)
      sync_mempool(state, txpool);

    head_out = state.last_block_id;
    wait_for(block_display_time);
  }

  void display_sync_progress(motrix& state)
  {
    using clock = std::chrono::steady_clock;
    std::deque<std::pair<monero::hash, base85>> chain{};

    // only subscribe to minimal chain while syncing, lowest overhead possible

    std::uint64_t target_height = 0;
    auto last_sync = clock::time_point::min();

    display::sync_meter progress{};
    progress.set_header("", "disconnected");
    update_screen(state, progress.handle());

    while (engine::is_running())
    {
      while (!target_height || target_sync_interval <= clock::now() - last_sync)
      {
        if (!state.rpc && !(state.rpc = zmq::connect(state.ctx.get(), ZMQ_REQ, state.rpc_address)))
          throw std::logic_error{"Unexpected nullptr from zmq::connect"};

        const auto get_info = zmq::invoke<rpc::json<method::get_info>>(state.rpc.get());
        ETERM_CHECK(get_info, "get_info RPC failed");
        if (!get_info->result.info.outgoing_connections_count && !get_info->result.info.incoming_connections_count)
        {
          progress.set_header("offline", state.rpc_address);
          update_screen(state, progress.handle()); // before blocking call

          // no connections, definitely behind. wait until a block is pushed
          const expect<void> event = zmq::wait_for(state.sub.get());
          ETERM_CHECK(event, "sub socket failed");
        }
        else
        {
          last_sync = clock::now();
          state.last_block_id = get_info->result.info.top_block_hash;
          state.daemon_height = get_info->result.info.height;
          target_height = get_info->result.info.target_height;

          const char* chain_type = "";
          if (get_info->result.info.mainnet)
            chain_type = "mainnet";
          else if (get_info->result.info.stagenet)
            chain_type = "stagenet";
          else if (get_info->result.info.testnet)
            chain_type = "testnet";

          progress.set_header(chain_type, state.rpc_address);
          if (target_height - state.daemon_height <= big_sync_interval)
            state.rpc.reset();
        }
      }

      progress.set_progress(state.daemon_height, target_height);
      if (target_height <= state.daemon_height)
      {
        update_screen(state, progress.handle());
        wait_for(std::chrono::seconds{3});
        break;
      }

      auto event = wait_for_pub(state, chain, progress.handle());
      ETERM_CHECK(event, "Failed to read daemon pub message");

      if (matches(event->topic, minimal_chain_topic))
      {
        const auto block = wire::json::from_bytes<pub::minimal_chain>(std::move(event->contents));
        if (block.ids.empty())
          throw std::runtime_error{"Chain missing ids"};

        state.daemon_height = block.first_height;
        state.last_block_id = block.ids.back();
        if (max_block_hash_buffer <= chain.size())
          chain.pop_front();

        chain.emplace_back(state.last_block_id, base85{});
      }
      else if (event->topic.empty() && event->contents.empty())
      {
        /* No block events in a while, recheck daemon status. Value does not get
           displayed to user until a `progress.set_progress(...)` call. */
        target_height = 0;
        progress.set_header("", "disconnected");
        update_screen(state, progress.handle());
      }
    }
  }

  void display_txpool(motrix& state)
  {
    std::map<monero::hash, base85> txpool{};

    topic_change(state.sub.get(), ZMQ_SUBSCRIBE, full_chain_topic);
    topic_change(state.sub.get(), ZMQ_SUBSCRIBE, minimal_txpool_topic);
    sync_mempool(state, txpool);

    unsigned last_txs_count = 0;
    monero::hash full_block_prev{};
    monero::hash minimal_block_prev{};
    monero::hash current_head = state.last_block_id;

    // Note this algorithm is cheating. you can't subscribe to full and minimal
    // and sync unless you check the hash for both (currently full doesn't send
    // hash it must be computed).

    while (engine::is_running())
    {
      auto event = wait_for_pub(state, txpool, nullptr);
      ETERM_CHECK(event, "Failed to read daemon pub message");

      if (matches(event->topic, minimal_chain_topic))
      {
        const auto minimal_block = wire::json::from_bytes<pub::minimal_chain>(std::move(event->contents));
        if (minimal_block.ids.empty())
          throw std::runtime_error{"bad block ids"};

        const bool reorg = minimal_block.first_height < state.daemon_height;
        state.daemon_height = minimal_block.first_height;
        if (reorg)
          break; // re-check daemon status

        const bool gap = (state.last_block_id != minimal_block.first_prev_id);
        state.last_block_id = minimal_block.ids.back();
        minimal_block_prev = minimal_block.ids.size() == 1 ?
          minimal_block.first_prev_id : minimal_block.ids[minimal_block.ids.size()];

        if (gap)
          sync_mempool(state, txpool);

        // full block pub received
        if (full_block_prev == minimal_block.first_prev_id)
          show_system_warning(state, current_head, full_block_prev, last_txs_count, txpool);
      }
      else if (matches(event->topic, full_chain_topic))
      {
        const auto full_blocks = wire::json::from_bytes<pub::full_chain>(std::move(event->contents));
        if (full_blocks.empty())
          throw std::runtime_error{"empty full-chain_main"};

        last_txs_count = full_blocks.back().tx_hashes.size();
        full_block_prev = full_blocks.back().prev_id;
        for (const monero::block& bl : full_blocks)
        {
          for (const monero::hash& hash : bl.tx_hashes)
            txpool.erase(hash);
        }

        // minimal block pub received
        if (minimal_block_prev == full_blocks.back().prev_id)
          show_system_warning(state, current_head, full_block_prev, last_txs_count, txpool);
      }
      else if (matches(event->topic, minimal_txpool_topic))
      {
        const auto daemon_pool = wire::json::from_bytes<pub::minimal_txpool>(std::move(event->contents));
        for (const monero::minimal_tx& tx : daemon_pool)
          txpool.emplace(tx.id, base85{});
      }
      else if (event->topic.empty() && event->contents.empty())
        break; // no events (no txpool nor chain) in a while, re-check daemon status
    }

    topic_change(state.sub.get(), ZMQ_UNSUBSCRIBE, minimal_txpool_topic);
    topic_change(state.sub.get(), ZMQ_UNSUBSCRIBE, full_chain_topic);
  }
}

void engine::run(const char* pub_address, const char* rpc_address, const char* color_scheme)
{
  if (!rpc_address || !pub_address)
    throw std::logic_error{"engine::run given nullptr address"};

  initscr();
  display::exit cleanup{};

  {
    int exit_pipe[2] = {-1, -1};
    POSIX_UNWRAP(pipe(exit_pipe));
    exit_fd_ = exit_pipe[0];

    static const int signal_exit = exit_pipe[1];
    std::signal(SIGINT, [](int)
    {
      running_ = false;
      if (::write(signal_exit, "\0", 1) != 1)
        std::abort();
    });
  }

  cbreak();
  noecho();
  curs_set(0);

  CURSES_UNWRAP(start_color());

  const bool limited_colors = COLORS < 256;
  const bool is_auto = std::strcmp(color_scheme, "auto") == 0;
  if ((is_auto && !limited_colors) || std::strcmp(color_scheme, "monero") == 0)
  {
    CURSES_UNWRAP(init_pair(display::kInfoText, COLOR_WHITE, COLOR_BLACK));
    CURSES_UNWRAP(init_pair(display::kProgressMeterNoHighlight, COLOR_WHITE, 239));
    CURSES_UNWRAP(init_pair(display::kProgressMeterHighlight, COLOR_BLACK, 202));
    CURSES_UNWRAP(init_pair(display::kFallingText1, 239, COLOR_BLACK));
    CURSES_UNWRAP(init_pair(display::kFallingText2, 202, COLOR_BLACK));
  }
  else if (std::strcmp(color_scheme, "monero_alt") == 0)
  {
    CURSES_UNWRAP(init_pair(display::kInfoText, COLOR_BLACK, 231));
    CURSES_UNWRAP(init_pair(display::kProgressMeterNoHighlight, 231, 239));
    CURSES_UNWRAP(init_pair(display::kProgressMeterHighlight, 231, 202));
    CURSES_UNWRAP(init_pair(display::kFallingText1, 239, 231));
    CURSES_UNWRAP(init_pair(display::kFallingText2, 202, 231));
  }
  else if (is_auto || std::strcmp(color_scheme, "standard") == 0)
  {
    CURSES_UNWRAP(init_pair(display::kInfoText, COLOR_WHITE, COLOR_BLACK));
    CURSES_UNWRAP(init_pair(display::kProgressMeterNoHighlight, COLOR_WHITE, COLOR_BLACK));
    CURSES_UNWRAP(init_pair(display::kProgressMeterHighlight, COLOR_BLACK, COLOR_GREEN));
    CURSES_UNWRAP(init_pair(display::kFallingText1, COLOR_GREEN, COLOR_BLACK));
    CURSES_UNWRAP(init_pair(display::kFallingText2, COLOR_GREEN, COLOR_BLACK));
  }
  else
    throw std::runtime_error{color_scheme + std::string{"is not a valid color scheme argument"}};

  motrix state{pub_address, rpc_address};
  while (engine::is_running())
  {
    display_sync_progress(state);
    redrawwin(state.text.handle());

    display_txpool(state);
    redrawwin(state.text.handle());
  }
}
