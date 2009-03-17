/**
 * Autogenerated by Thrift
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 */
#ifndef Client_TYPES_H
#define Client_TYPES_H

#include <Thrift.h>
#include <protocol/TProtocol.h>
#include <transport/TTransport.h>



namespace Hypertable { namespace ThriftGen {

enum CellFlag {
  DELETE_ROW = 0,
  DELETE_CF = 1,
  DELETE_CELL = 2,
  INSERT = 255
};

typedef int64_t Scanner;

typedef int64_t Mutator;

typedef std::string Value;

typedef std::vector<std::string>  CellAsArray;

class RowInterval {
 public:

  static const char* ascii_fingerprint; // = "E1A4BCD94F003EFF8636F1C98591705A";
  static const uint8_t binary_fingerprint[16]; // = {0xE1,0xA4,0xBC,0xD9,0x4F,0x00,0x3E,0xFF,0x86,0x36,0xF1,0xC9,0x85,0x91,0x70,0x5A};

  RowInterval() : start_row(""), start_inclusive(true), end_row(""), end_inclusive(true) {
  }

  virtual ~RowInterval() throw() {}

  std::string start_row;
  bool start_inclusive;
  std::string end_row;
  bool end_inclusive;

  struct __isset {
    __isset() : start_row(false), start_inclusive(false), end_row(false), end_inclusive(false) {}
    bool start_row;
    bool start_inclusive;
    bool end_row;
    bool end_inclusive;
  } __isset;

  bool operator == (const RowInterval & rhs) const
  {
    if (__isset.start_row != rhs.__isset.start_row)
      return false;
    else if (__isset.start_row && !(start_row == rhs.start_row))
      return false;
    if (__isset.start_inclusive != rhs.__isset.start_inclusive)
      return false;
    else if (__isset.start_inclusive && !(start_inclusive == rhs.start_inclusive))
      return false;
    if (__isset.end_row != rhs.__isset.end_row)
      return false;
    else if (__isset.end_row && !(end_row == rhs.end_row))
      return false;
    if (__isset.end_inclusive != rhs.__isset.end_inclusive)
      return false;
    else if (__isset.end_inclusive && !(end_inclusive == rhs.end_inclusive))
      return false;
    return true;
  }
  bool operator != (const RowInterval &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const RowInterval & ) const;

  uint32_t read(apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(apache::thrift::protocol::TProtocol* oprot) const;

};

class CellInterval {
 public:

  static const char* ascii_fingerprint; // = "D8C6D6FAE68BF8B6CA0EB2AB01E82C6C";
  static const uint8_t binary_fingerprint[16]; // = {0xD8,0xC6,0xD6,0xFA,0xE6,0x8B,0xF8,0xB6,0xCA,0x0E,0xB2,0xAB,0x01,0xE8,0x2C,0x6C};

  CellInterval() : start_row(""), start_column(""), start_inclusive(true), end_row(""), end_column(""), end_inclusive(true) {
  }

  virtual ~CellInterval() throw() {}

  std::string start_row;
  std::string start_column;
  bool start_inclusive;
  std::string end_row;
  std::string end_column;
  bool end_inclusive;

  struct __isset {
    __isset() : start_row(false), start_column(false), start_inclusive(false), end_row(false), end_column(false), end_inclusive(false) {}
    bool start_row;
    bool start_column;
    bool start_inclusive;
    bool end_row;
    bool end_column;
    bool end_inclusive;
  } __isset;

  bool operator == (const CellInterval & rhs) const
  {
    if (__isset.start_row != rhs.__isset.start_row)
      return false;
    else if (__isset.start_row && !(start_row == rhs.start_row))
      return false;
    if (__isset.start_column != rhs.__isset.start_column)
      return false;
    else if (__isset.start_column && !(start_column == rhs.start_column))
      return false;
    if (__isset.start_inclusive != rhs.__isset.start_inclusive)
      return false;
    else if (__isset.start_inclusive && !(start_inclusive == rhs.start_inclusive))
      return false;
    if (__isset.end_row != rhs.__isset.end_row)
      return false;
    else if (__isset.end_row && !(end_row == rhs.end_row))
      return false;
    if (__isset.end_column != rhs.__isset.end_column)
      return false;
    else if (__isset.end_column && !(end_column == rhs.end_column))
      return false;
    if (__isset.end_inclusive != rhs.__isset.end_inclusive)
      return false;
    else if (__isset.end_inclusive && !(end_inclusive == rhs.end_inclusive))
      return false;
    return true;
  }
  bool operator != (const CellInterval &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const CellInterval & ) const;

  uint32_t read(apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(apache::thrift::protocol::TProtocol* oprot) const;

};

class ScanSpec {
 public:

  static const char* ascii_fingerprint; // = "AAC6EAC7CCF976D9159BC1D44D2DABF3";
  static const uint8_t binary_fingerprint[16]; // = {0xAA,0xC6,0xEA,0xC7,0xCC,0xF9,0x76,0xD9,0x15,0x9B,0xC1,0xD4,0x4D,0x2D,0xAB,0xF3};

  ScanSpec() : return_deletes(false), revs(0), row_limit(0), start_time(0), end_time(0) {
  }

  virtual ~ScanSpec() throw() {}

  std::vector<RowInterval>  row_intervals;
  std::vector<CellInterval>  cell_intervals;
  bool return_deletes;
  int32_t revs;
  int32_t row_limit;
  int64_t start_time;
  int64_t end_time;
  std::vector<std::string>  columns;

  struct __isset {
    __isset() : row_intervals(false), cell_intervals(false), return_deletes(false), revs(false), row_limit(false), start_time(false), end_time(false), columns(false) {}
    bool row_intervals;
    bool cell_intervals;
    bool return_deletes;
    bool revs;
    bool row_limit;
    bool start_time;
    bool end_time;
    bool columns;
  } __isset;

  bool operator == (const ScanSpec & rhs) const
  {
    if (__isset.row_intervals != rhs.__isset.row_intervals)
      return false;
    else if (__isset.row_intervals && !(row_intervals == rhs.row_intervals))
      return false;
    if (__isset.cell_intervals != rhs.__isset.cell_intervals)
      return false;
    else if (__isset.cell_intervals && !(cell_intervals == rhs.cell_intervals))
      return false;
    if (__isset.return_deletes != rhs.__isset.return_deletes)
      return false;
    else if (__isset.return_deletes && !(return_deletes == rhs.return_deletes))
      return false;
    if (__isset.revs != rhs.__isset.revs)
      return false;
    else if (__isset.revs && !(revs == rhs.revs))
      return false;
    if (__isset.row_limit != rhs.__isset.row_limit)
      return false;
    else if (__isset.row_limit && !(row_limit == rhs.row_limit))
      return false;
    if (__isset.start_time != rhs.__isset.start_time)
      return false;
    else if (__isset.start_time && !(start_time == rhs.start_time))
      return false;
    if (__isset.end_time != rhs.__isset.end_time)
      return false;
    else if (__isset.end_time && !(end_time == rhs.end_time))
      return false;
    if (__isset.columns != rhs.__isset.columns)
      return false;
    else if (__isset.columns && !(columns == rhs.columns))
      return false;
    return true;
  }
  bool operator != (const ScanSpec &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ScanSpec & ) const;

  uint32_t read(apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(apache::thrift::protocol::TProtocol* oprot) const;

};

class Cell {
 public:

  static const char* ascii_fingerprint; // = "7D0933CA0766D7C3EAFC61FC083091CE";
  static const uint8_t binary_fingerprint[16]; // = {0x7D,0x09,0x33,0xCA,0x07,0x66,0xD7,0xC3,0xEA,0xFC,0x61,0xFC,0x08,0x30,0x91,0xCE};

  Cell() : row_key(""), column_family(""), column_qualifier(""), value(""), timestamp(0), revision(0), flag(255) {
  }

  virtual ~Cell() throw() {}

  std::string row_key;
  std::string column_family;
  std::string column_qualifier;
  Value value;
  int64_t timestamp;
  int64_t revision;
  int16_t flag;

  struct __isset {
    __isset() : row_key(false), column_family(false), column_qualifier(false), value(false), timestamp(false), revision(false), flag(false) {}
    bool row_key;
    bool column_family;
    bool column_qualifier;
    bool value;
    bool timestamp;
    bool revision;
    bool flag;
  } __isset;

  bool operator == (const Cell & rhs) const
  {
    if (__isset.row_key != rhs.__isset.row_key)
      return false;
    else if (__isset.row_key && !(row_key == rhs.row_key))
      return false;
    if (__isset.column_family != rhs.__isset.column_family)
      return false;
    else if (__isset.column_family && !(column_family == rhs.column_family))
      return false;
    if (__isset.column_qualifier != rhs.__isset.column_qualifier)
      return false;
    else if (__isset.column_qualifier && !(column_qualifier == rhs.column_qualifier))
      return false;
    if (__isset.value != rhs.__isset.value)
      return false;
    else if (__isset.value && !(value == rhs.value))
      return false;
    if (__isset.timestamp != rhs.__isset.timestamp)
      return false;
    else if (__isset.timestamp && !(timestamp == rhs.timestamp))
      return false;
    if (__isset.revision != rhs.__isset.revision)
      return false;
    else if (__isset.revision && !(revision == rhs.revision))
      return false;
    if (__isset.flag != rhs.__isset.flag)
      return false;
    else if (__isset.flag && !(flag == rhs.flag))
      return false;
    return true;
  }
  bool operator != (const Cell &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const Cell & ) const;

  uint32_t read(apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(apache::thrift::protocol::TProtocol* oprot) const;

};

class ClientException : public apache::thrift::TException {
 public:

  static const char* ascii_fingerprint; // = "3F5FC93B338687BC7235B1AB103F47B3";
  static const uint8_t binary_fingerprint[16]; // = {0x3F,0x5F,0xC9,0x3B,0x33,0x86,0x87,0xBC,0x72,0x35,0xB1,0xAB,0x10,0x3F,0x47,0xB3};

  ClientException() : code(0), what("") {
  }

  virtual ~ClientException() throw() {}

  int32_t code;
  std::string what;

  struct __isset {
    __isset() : code(false), what(false) {}
    bool code;
    bool what;
  } __isset;

  bool operator == (const ClientException & rhs) const
  {
    if (!(code == rhs.code))
      return false;
    if (!(what == rhs.what))
      return false;
    return true;
  }
  bool operator != (const ClientException &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ClientException & ) const;

  uint32_t read(apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(apache::thrift::protocol::TProtocol* oprot) const;

};

}} // namespace

#endif
