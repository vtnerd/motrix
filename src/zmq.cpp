// Copyright (c) 2019, The Monero Project
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
#include <unistd.h>
#include "zmq.hpp"

#include <cassert>
#include <cerrno>
#include <limits>
#include <utility>

#include "byte_stream.hpp"
#include "engine.hpp"

namespace zmq
{
    const std::error_category& error_category() noexcept
    {
        struct category final : std::error_category
        {
            virtual const char* name() const noexcept override final
            {
                return "error::error_category()";
            }

            virtual std::string message(int value) const override final
            {
                char const* const msg = zmq_strerror(value);
                if (msg)
                    return msg;
                return "zmq_strerror failure";
            }

            virtual std::error_condition default_error_condition(int value) const noexcept override final
            {
                // maps specific errors to generic `std::errc` cases.
                switch (value)
                {
                case EFSM:
                case ETERM:
                    break;
                default:
                    /* zmq is using cerrno errors. C++ spec indicates that
                       `std::errc` values must be identical to the cerrno value.
                       So just map every zmq specific error to the generic errc
                       equivalent. zmq extensions must be in the switch or they
                       map to a non-existent errc enum value. */
                    return std::errc(value);
                }
                return std::error_condition{value, *this};
            }

        };
        static const category instance{};
        return instance;
    }

    void terminate::call(void* ptr) noexcept
    {
        assert(ptr != nullptr); // see header
        while (zmq_term(ptr))
        {
            if (zmq_errno() != EINTR)
                break;
        }
    }

    socket connect(void* ctx, int type, const char* address)
    {
      socket out{zmq_socket(ctx, type)};
      if (!out)
        MOT_ZMQ_THROW("Failed to create socket");
      if (zmq_connect(out.get(), address) != 0)
        MOT_ZMQ_THROW("Failed to connect socket");

      int linger = 0;
      if (zmq_setsockopt(out.get(), ZMQ_LINGER, &linger, sizeof(linger)) != 0)
        MOT_ZMQ_THROW("Failed to set ZMQ linger option");

      return out;
    }

    namespace
    {
        //! RAII wrapper for `zmq_msg_t`.
        class message
        {
            zmq_msg_t handle_;

        public:
            message() noexcept
              : handle_()
            {
                zmq_msg_init(handle());
            }

            message(message&& rhs) = delete;
            message(const message& rhs) = delete;
            message& operator=(message&& rhs) = delete;
            message& operator=(const message& rhs) = delete;

            ~message() noexcept
            {
                zmq_msg_close(handle());
            }

            zmq_msg_t* handle() noexcept
            {
                return std::addressof(handle_);
            }

            const char* data() noexcept
            {
                return static_cast<const char*>(zmq_msg_data(handle()));
            }

            std::size_t size() noexcept
            {
                return zmq_msg_size(handle());
            }
        };

        struct do_receive
        {
            /* ZMQ documentation states that message parts are atomic - either
               all are received or none are. Looking through ZMQ code and
               Github discussions indicates that after part 1 is returned,
               `EAGAIN` cannot be returned to meet these guarantees. Unit tests
               verify (for the `inproc://` case) that this is the behavior. 
               Therefore, read errors after the first part are treated as a
               failure for the entire message (probably `ETERM`). */
            int operator()(byte_stream& payload, void* const socket, const int flags) const
            {
                static constexpr const int max_out = std::numeric_limits<int>::max();
                const byte_slice::size_type initial = payload.size();
                message part{};
                for (;;)
                {
                    int last = 0;
                    if ((last = zmq_msg_recv(part.handle(), socket, flags)) < 0)
                        return last;

                    payload.write(part.data(), part.size());
                    if (!zmq_msg_more(part.handle()))
                        break;
                }
                const byte_slice::size_type added = payload.size() - initial;
                return unsigned(max_out) < added ? max_out : int(added);
            }
        };
    } // anonymous

    expect<byte_slice> receive(void* const socket, const int flags)
    {
        byte_stream payload{};
        MOT_CHECK(retry_op(do_receive{}, payload, socket, flags));
        return {byte_slice{std::move(payload)}};
    }

    expect<void> send(const span<const std::uint8_t> payload, void* const socket, const int flags) noexcept
    {
        return retry_op(zmq_send, socket, payload.data(), payload.size(), flags);
    }

    expect<void> send(byte_slice&& payload, void* socket, int flags) noexcept
    {
        void* const data = const_cast<std::uint8_t*>(payload.data());
        const std::size_t size = payload.size();
        auto buffer = payload.take_buffer(); // clears `payload` from callee

        zmq_msg_t msg{};
        MOT_ZMQ_CHECK(zmq_msg_init_data(std::addressof(msg), data, size, release_byte_slice::call, buffer.get()));
        buffer.release(); // zmq will now decrement byte_slice ref-count

        expect<void> sent = retry_op(zmq_msg_send, std::addressof(msg), socket, flags);
        if (!sent) // beware if removing `noexcept` from this function - possible leak here
            zmq_msg_close(std::addressof(msg));
        return sent;
    }

    expect<void> wait_for(void* sock)
    {
        zmq_pollitem_t items[2] = {{sock, 0, ZMQ_POLLIN}, {NULL, engine::exit_fd(), ZMQ_POLLIN}};
	MOT_CHECK(retry_op(zmq_poll, items, 2, -1));
	if (items[1].revents & ZMQ_POLLIN)
	    return make_error_code(ETERM);
	return success();
    }
} // zmq


