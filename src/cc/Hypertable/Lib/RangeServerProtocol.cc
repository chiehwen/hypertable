/*
 * Copyright (C) 2007-2013 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 3 of the
 * License, or any later version.
 *
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/** @file
 * Definitions for RangeServerProtocol
 * This file contains definitions for RangeServerProtocol, a class for
 * generating RangeServer protocol messages.
 */

#include "Common/Compat.h"
#include "AsyncComm/CommBuf.h"
#include "AsyncComm/CommHeader.h"

#include "RangeServerProtocol.h"

namespace Hypertable {

  using namespace Serialization;

  const char *RangeServerProtocol::m_command_strings[] = {
    "load range",
    "update",
    "create scanner",
    "fetch scanblock",
    "compact",
    "status",
    "shutdown",
    "dump",
    "destroy scanner",
    "drop table",
    "drop range",
    "replay begin",
    "replay load range",
    "replay update",
    "replay commit",
    "get statistics",
    "update schema",
    "commit log sync",
    "close",
    "wait for maintenance",
    "acknowledge load",
    "relinquish range",
    "heapcheck",
    "metadata sync",
    "replay fragments",
    "phantom receive",
    "phantom update",
    "phantom prepare ranges",
    "phantom commit ranges",
    "dump pseudo table",
    "set state",
    (const char *)0
  };

  const char *RangeServerProtocol::command_text(uint64_t command) {
    if (command >= COMMAND_MAX)
      return "UNKNOWN";
    return m_command_strings[command];
  }

  String
  RangeServerProtocol::compact_flags_to_string(uint32_t flags) {
    String str;
    if ((flags & COMPACT_FLAG_ALL) == COMPACT_FLAG_ALL)
      str += "ALL";
    else {
      bool first=true;
      if ((flags & COMPACT_FLAG_ROOT) == COMPACT_FLAG_ROOT) {
        str += "ROOT";
        first=false;
      }
      if ((flags & COMPACT_FLAG_METADATA) == COMPACT_FLAG_METADATA) {
        if (!first) {
          str += "|";
          first=false;
        }
        str += "METADATA";
      }
      if ((flags & COMPACT_FLAG_SYSTEM) == COMPACT_FLAG_SYSTEM) {
        if (!first) {
          str += "|";
          first=false;
        }
        str += "SYSTEM";
      }
      if ((flags & COMPACT_FLAG_USER) == COMPACT_FLAG_USER) {
        if (!first) {
          str += "|";
          first=false;
        }
        str += "USER";
      }
    }
    return str;
  }

  CommBuf *
  RangeServerProtocol::create_request_compact(const TableIdentifier &table,
                                              const String &row,
                                              uint32_t flags) {
    CommHeader header(COMMAND_COMPACT);
    CommBuf *cbuf = new CommBuf(header, table.encoded_length() +
                                Serialization::encoded_length_vstr(row) + 4);
    table.encode(cbuf->get_data_ptr_address());
    Serialization::encode_vstr(cbuf->get_data_ptr_address(), row);
    Serialization::encode_i32(cbuf->get_data_ptr_address(), flags);
    return cbuf;
  }

  CommBuf *
  RangeServerProtocol::create_request_metadata_sync(const String &table_id, uint32_t flags,
                                                    std::vector<String> &columns) {
    CommHeader header(COMMAND_METADATA_SYNC);
    size_t length = Serialization::encoded_length_vstr(table_id) + 8;
    for (size_t i=0; i<columns.size(); i++)
      length += Serialization::encoded_length_vstr(columns[i]);
    CommBuf *cbuf = new CommBuf(header, length);
    Serialization::encode_vstr(cbuf->get_data_ptr_address(), table_id);
    Serialization::encode_i32(cbuf->get_data_ptr_address(), flags);
    Serialization::encode_i32(cbuf->get_data_ptr_address(), columns.size());
    for (size_t i=0; i<columns.size(); i++)
      Serialization::encode_vstr(cbuf->get_data_ptr_address(), columns[i]);
    return cbuf;
  }

  CommBuf *
  RangeServerProtocol::create_request_load_range(const TableIdentifier &table,
      const RangeSpec &range, const RangeState &range_state, bool needs_compaction) {
    CommHeader header(COMMAND_LOAD_RANGE);
    header.flags |= CommHeader::FLAGS_BIT_URGENT;
    CommBuf *cbuf = new CommBuf(header, table.encoded_length()
        + range.encoded_length() + range_state.encoded_length() + 1);
    table.encode(cbuf->get_data_ptr_address());
    range.encode(cbuf->get_data_ptr_address());
    range_state.encode(cbuf->get_data_ptr_address());
    Serialization::encode_bool(cbuf->get_data_ptr_address(), needs_compaction);
    return cbuf;
  }

  CommBuf *
  RangeServerProtocol::create_request_update(const TableIdentifier &table,
      uint32_t count, StaticBuffer &buffer, uint32_t flags) {
    CommHeader header(COMMAND_UPDATE);
    if (table.is_system()) // If system table, set the urgent bit
      header.flags |= CommHeader::FLAGS_BIT_URGENT;
    CommBuf *cbuf = new CommBuf(header, 8 + table.encoded_length(), buffer);
    table.encode(cbuf->get_data_ptr_address());
    cbuf->append_i32(count);
    cbuf->append_i32(flags);
    return cbuf;
  }

  CommBuf *
  RangeServerProtocol::create_request_update_schema(
      const TableIdentifier &table, const String &schema) {
    CommHeader header(COMMAND_UPDATE_SCHEMA);
    if (table.is_system()) // If system table, set the urgent bit
      header.flags |= CommHeader::FLAGS_BIT_URGENT;
    CommBuf *cbuf = new CommBuf(header, table.encoded_length()
        + encoded_length_vstr(schema));
    table.encode(cbuf->get_data_ptr_address());
    cbuf->append_vstr(schema);
    return cbuf;
  }

  CommBuf *RangeServerProtocol::create_request_commit_log_sync(const TableIdentifier &table) {
    CommHeader header(COMMAND_COMMIT_LOG_SYNC);
    if (table.is_system()) // If system table, set the urgent bit
      header.flags |= CommHeader::FLAGS_BIT_URGENT;
    CommBuf *cbuf = new CommBuf(header, table.encoded_length());
    table.encode(cbuf->get_data_ptr_address());
    return cbuf;
  }

  CommBuf *RangeServerProtocol::
  create_request_create_scanner(const TableIdentifier &table,
      const RangeSpec &range, const ScanSpec &scan_spec) {
    CommHeader header(COMMAND_CREATE_SCANNER);
    if (table.is_system()) // If system table, set the urgent bit
      header.flags |= CommHeader::FLAGS_BIT_URGENT;
    CommBuf *cbuf = new CommBuf(header, table.encoded_length()
        + range.encoded_length() + scan_spec.encoded_length());
    table.encode(cbuf->get_data_ptr_address());
    range.encode(cbuf->get_data_ptr_address());
    scan_spec.encode(cbuf->get_data_ptr_address());
    return cbuf;
  }

  CommBuf *RangeServerProtocol::create_request_destroy_scanner(int scanner_id) {
    CommHeader header(COMMAND_DESTROY_SCANNER);
    header.gid = scanner_id;
    CommBuf *cbuf = new CommBuf(header, 4);
    cbuf->append_i32(scanner_id);
    return cbuf;
  }

  CommBuf *RangeServerProtocol::create_request_fetch_scanblock(int scanner_id) {
    CommHeader header(COMMAND_FETCH_SCANBLOCK);
    header.gid = scanner_id;
    CommBuf *cbuf = new CommBuf(header, 4);
    cbuf->append_i32(scanner_id);
    return cbuf;
  }

  CommBuf *
  RangeServerProtocol::create_request_drop_table(const TableIdentifier &table) {
    CommHeader header(COMMAND_DROP_TABLE);
    CommBuf *cbuf = new CommBuf(header, table.encoded_length());
    table.encode(cbuf->get_data_ptr_address());
    return cbuf;
  }

  CommBuf *RangeServerProtocol::create_request_status() {
    CommHeader header(COMMAND_STATUS);
    header.flags |= CommHeader::FLAGS_BIT_URGENT;
    CommBuf *cbuf = new CommBuf(header);
    return cbuf;
  }

  CommBuf *RangeServerProtocol::create_request_close() {
    CommHeader header(COMMAND_CLOSE);
    header.flags |= CommHeader::FLAGS_BIT_URGENT;
    CommBuf *cbuf = new CommBuf(header);
    return cbuf;
  }

  CommBuf *RangeServerProtocol::create_request_wait_for_maintenance() {
    CommHeader header(COMMAND_WAIT_FOR_MAINTENANCE);
    header.flags |= CommHeader::FLAGS_BIT_URGENT;
    CommBuf *cbuf = new CommBuf(header);
    return cbuf;
  }

  CommBuf *RangeServerProtocol::create_request_shutdown() {
    CommHeader header(COMMAND_SHUTDOWN);
    header.flags |= CommHeader::FLAGS_BIT_URGENT;
    CommBuf *cbuf = new CommBuf(header);
    return cbuf;
  }

  CommBuf *RangeServerProtocol::create_request_dump(const String &outfile,
						    bool nokeys) {
    CommHeader header(COMMAND_DUMP);
    header.flags |= CommHeader::FLAGS_BIT_URGENT;
    CommBuf *cbuf =
      new CommBuf(header, Serialization::encoded_length_vstr(outfile)
		  + 1);
    Serialization::encode_vstr(cbuf->get_data_ptr_address(), outfile.c_str());
    Serialization::encode_bool(cbuf->get_data_ptr_address(), nokeys);
    return cbuf;
  }

  CommBuf *RangeServerProtocol::create_request_dump_pseudo_table(const TableIdentifier &table,
                                        const String &pseudo_table_name,
                                        const String &outfile) {
    CommHeader header(COMMAND_DUMP_PSEUDO_TABLE);
    CommBuf *cbuf =
      new CommBuf(header, table.encoded_length() +
                  Serialization::encoded_length_vstr(pseudo_table_name) +
                  Serialization::encoded_length_vstr(outfile));
    table.encode(cbuf->get_data_ptr_address());
    Serialization::encode_vstr(cbuf->get_data_ptr_address(),
                               pseudo_table_name.c_str());
    Serialization::encode_vstr(cbuf->get_data_ptr_address(), outfile.c_str());
    return cbuf;
  }

  CommBuf *
  RangeServerProtocol::create_request_drop_range(const TableIdentifier &table,
                                                 const RangeSpec &range) {
    CommHeader header(COMMAND_DROP_RANGE);
    CommBuf *cbuf = new CommBuf(header, table.encoded_length()
                                + range.encoded_length());
    table.encode(cbuf->get_data_ptr_address());
    range.encode(cbuf->get_data_ptr_address());
    return cbuf;
  }

  CommBuf *
  RangeServerProtocol::create_request_acknowledge_load(const vector<QualifiedRangeSpec*> &ranges) {
    CommHeader header(COMMAND_ACKNOWLEDGE_LOAD);
    header.flags |= CommHeader::FLAGS_BIT_URGENT;
    size_t len = 4;
    foreach_ht(const QualifiedRangeSpec *range, ranges) {
      len += range->encoded_length();
    }

    CommBuf *cbuf = new CommBuf(header, len);
    Serialization::encode_i32(cbuf->get_data_ptr_address(), ranges.size());
    foreach_ht(const QualifiedRangeSpec *range, ranges) {
      range->encode(cbuf->get_data_ptr_address());
    }

    return cbuf;
  }

  CommBuf *
  RangeServerProtocol::create_request_acknowledge_load(const TableIdentifier &table,
                                                       const RangeSpec &range) {
    CommHeader header(COMMAND_ACKNOWLEDGE_LOAD);
    header.flags |= CommHeader::FLAGS_BIT_URGENT;
    CommBuf *cbuf = new CommBuf(header, table.encoded_length()
                                + range.encoded_length());
    table.encode(cbuf->get_data_ptr_address());
    range.encode(cbuf->get_data_ptr_address());
    return cbuf;
  }

  CommBuf *RangeServerProtocol::create_request_get_statistics(std::vector<SystemVariable::Spec> &specs,
                                                              uint64_t generation) {
    CommHeader header(COMMAND_GET_STATISTICS);
    header.flags |= CommHeader::FLAGS_BIT_URGENT;
    CommBuf *cbuf = new CommBuf(header, 8 +
                                SystemVariable::encoded_length_specs(specs));
    cbuf->append_i64(generation);
    SystemVariable::encode_specs(specs, cbuf->get_data_ptr_address());
    return cbuf;
  }

  CommBuf *
  RangeServerProtocol::create_request_relinquish_range(const TableIdentifier &table,
                                                 const RangeSpec &range) {
    CommHeader header(COMMAND_RELINQUISH_RANGE);
    CommBuf *cbuf = new CommBuf(header, table.encoded_length()
                                + range.encoded_length());
    table.encode(cbuf->get_data_ptr_address());
    range.encode(cbuf->get_data_ptr_address());
    return cbuf;
  }

  CommBuf *RangeServerProtocol::create_request_heapcheck(const String &outfile) {
    CommHeader header(COMMAND_HEAPCHECK);
    header.flags |= CommHeader::FLAGS_BIT_URGENT;
    CommBuf *cbuf = new CommBuf(header, encoded_length_vstr(outfile));
    Serialization::encode_vstr(cbuf->get_data_ptr_address(), outfile.c_str());
    return cbuf;
  }

  CommBuf *RangeServerProtocol::create_request_set_state(std::vector<SystemVariable::Spec> &specs,
                                                         uint64_t generation) {
    CommHeader header(COMMAND_SET_STATE);
    header.flags |= CommHeader::FLAGS_BIT_URGENT;
    CommBuf *cbuf = new CommBuf(header, 8 +
                                SystemVariable::encoded_length_specs(specs));
    cbuf->append_i64(generation);
    SystemVariable::encode_specs(specs, cbuf->get_data_ptr_address());
    return cbuf;
  }


  CommBuf *RangeServerProtocol::create_request_replay_fragments(int64_t op_id,
          const String &recover_location, int plan_generation,
          int type, const vector<uint32_t> &fragments,
          const RangeRecoveryReceiverPlan &receiver_plan,
          uint32_t replay_timeout) {
    CommHeader header(COMMAND_REPLAY_FRAGMENTS);
    header.flags |= CommHeader::FLAGS_BIT_URGENT;
    size_t len = 8 + encoded_length_vstr(recover_location) + 4 + 4
        + 4 + fragments.size() * 4 + receiver_plan.encoded_length() + 4;

    CommBuf *cbuf = new CommBuf(header, len);
    cbuf->append_i64(op_id);
    cbuf->append_vstr(recover_location.c_str());
    cbuf->append_i32(plan_generation);
    cbuf->append_i32(type);
    cbuf->append_i32(fragments.size());
    for(size_t ii = 0; ii < fragments.size(); ++ii)
      cbuf->append_i32(fragments[ii]);
    receiver_plan.encode(cbuf->get_data_ptr_address());
    cbuf->append_i32(replay_timeout);

    return cbuf;
  }

  CommBuf *RangeServerProtocol::create_request_phantom_load(const String &location,
          int plan_generation,
          const vector<uint32_t> &fragments,
          const vector<QualifiedRangeSpec> &specs,
          const std::vector<RangeState> &states) {
    CommHeader header(COMMAND_PHANTOM_RECEIVE);
    header.flags |= CommHeader::FLAGS_BIT_URGENT;
    size_t len = encoded_length_vstr(location) + 4 + 4 + fragments.size() * 4;
    len += 4;
    HT_ASSERT(specs.size() == states.size());
    for (size_t ii = 0; ii < specs.size(); ++ii)
      len += specs[ii].encoded_length() + states[ii].encoded_length();

    CommBuf *cbuf = new CommBuf(header, len);
    cbuf->append_vstr(location);
    cbuf->append_i32(plan_generation);
    cbuf->append_i32(fragments.size());
    for (size_t ii = 0; ii < fragments.size(); ++ii)
      cbuf->append_i32(fragments[ii]);
    cbuf->append_i32(specs.size());
    for (size_t ii = 0; ii < specs.size(); ++ii)
      specs[ii].encode(cbuf->get_data_ptr_address());
    for (size_t ii = 0; ii < specs.size(); ++ii)
      states[ii].encode(cbuf->get_data_ptr_address());
    return cbuf;
  }

  CommBuf *RangeServerProtocol::create_request_phantom_update(const QualifiedRangeSpec &range,
          const String &location, int plan_generation, uint32_t fragment,
          StaticBuffer &buffer) {
    CommHeader header(COMMAND_PHANTOM_UPDATE);
    if (range.table.is_metadata())
      header.flags |= CommHeader::FLAGS_BIT_URGENT;
    size_t len = encoded_length_vstr(location) + 4 + range.encoded_length() + 4;
    CommBuf *cbuf = new CommBuf(header, len, buffer);
    cbuf->append_vstr(location);
    cbuf->append_i32(plan_generation);
    range.encode(cbuf->get_data_ptr_address());
    cbuf->append_i32(fragment);
    return cbuf;
  }

  CommBuf *RangeServerProtocol::create_request_phantom_prepare_ranges(int64_t op_id,
          const String &location, int plan_generation, 
          const vector<QualifiedRangeSpec> &ranges) {
    return create_request_phantom_ranges(COMMAND_PHANTOM_PREPARE_RANGES,
            op_id, location, plan_generation, ranges);
  }

  CommBuf *RangeServerProtocol::create_request_phantom_commit_ranges(int64_t op_id,
          const String &location, int plan_generation, const vector<QualifiedRangeSpec> &ranges) {
    return create_request_phantom_ranges(COMMAND_PHANTOM_COMMIT_RANGES,
            op_id, location, plan_generation, ranges);
  }

  CommBuf *RangeServerProtocol::create_request_phantom_ranges(uint64_t cmd_id,
          int64_t op_id, const String &location, int plan_generation,
          const vector<QualifiedRangeSpec> &ranges) {
    CommHeader header(cmd_id);
    header.flags |= CommHeader::FLAGS_BIT_URGENT;
    size_t len = 8 + encoded_length_vstr(location) + 4;
    for (size_t ii = 0; ii < ranges.size(); ++ii)
      len += ranges[ii].encoded_length();
    len += 4;

    CommBuf *cbuf = new CommBuf(header, len);
    cbuf->append_i64(op_id);
    cbuf->append_vstr(location);
    cbuf->append_i32(plan_generation);
    cbuf->append_i32(ranges.size());
    for(size_t ii = 0; ii < ranges.size(); ++ii)
      ranges[ii].encode(cbuf->get_data_ptr_address());

    return cbuf;
  }
} // namespace Hypertable
