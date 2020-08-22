#ifndef MOTRIX_WIRE_JSON_FWD_HPP
#define MOTRIX_WIRE_JSON_FWD_HPP

#define WIRE_JSON_DECLARE_OBJECT(type)			\
  void read_bytes(::wire::json_reader&, type&);         \
  void write_bytes(::wire::json_writer&, const type&)

namespace wire
{
  struct json;
  class json_reader;
  class json_writer;
}

#endif // MOTRIX_WIRE_JSON_FWD_HPP
