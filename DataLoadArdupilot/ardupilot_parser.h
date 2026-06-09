/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

#include <PlotJuggler/plotdata.h>

struct ApSeries;  // forward declaration for ApMessageDef::series_ptrs / instance_ptrs

struct ApFieldDef
{
  char        fmt_char  = 0;
  int         byte_size = 0;
  bool        is_string = false;
  bool        is_array  = false;
  std::string label;
  char        unit_id  = '?';
  char        mult_id  = '?';
  double      mult_val = 1.0;   // cached scaling factor, populated after pass 1
};

struct ApMessageDef
{
  uint8_t                  msg_type             = 0;
  uint8_t                  msg_len              = 0;
  std::string              name;
  std::vector<ApFieldDef>  fields;
  int                      timestamp_idx        = -1;
  int                      timestamp_byte_offset = -1;  // pre-cached byte offset of TimeUS field
  int                      instance_idx         = -1;
  std::vector<std::string>         series_keys;          // pre-built "Name/Field" per field (non-instance only)
  std::vector<ApSeries*>           series_ptrs;          // parallel to series_keys; cached ApSeries*
  std::vector<PJ::PlotData*>       plot_ptrs;            // parallel to series_keys; write-through PlotData*
  std::vector<PJ::StringSeries*>   str_ptrs;             // parallel; non-null for string fields (non-instance)
  mutable std::unordered_map<int, std::vector<ApSeries*>>          instance_ptrs;       // keyed by instance id
  mutable std::unordered_map<int, std::vector<PJ::PlotData*>>      instance_plot_ptrs;  // write-through (instance)
  mutable std::unordered_map<int, std::vector<PJ::StringSeries*>>  instance_str_ptrs;   // string series (instance)
  int                        stats_idx  = -1;   // index into _stats[], set in finalizeOneDef()
  bool                       finalized  = false; // true after finalizeOneDef() has run
};

struct ApFmtuPending
{
  char units[16]       = {};
  char multipliers[16] = {};
};

struct ApSeries
{
  std::vector<std::pair<double,double>> points;   // interleaved {timestamp, value}
  std::string unit;
  char        unit_id       = '?';
  bool        unit_resolved = false;  // set after first resolution attempt; _unitTable is complete before decode
};

using ApSeriesMap = std::unordered_map<std::string, ApSeries>;

struct ApMessageStats
{
  std::string name;
  uint64_t    count = 0;
};

struct ApParameter
{
  std::string name;
  double      value = 0.0;
};

struct ApEmbeddedFile
{
  std::string           name;
  std::vector<uint8_t>  data;
};

struct ApLogMessage
{
  double      timestamp = 0.0;
  std::string message;
};

struct ApVersionInfo
{
  std::string firmware_str;      // VER/FWS  e.g. "ArduCopter V4.5.2"
  uint32_t    git_hash    = 0;   // VER/GH   firmware git hash
  uint16_t    board_sub   = 0;   // VER/BST  board subtype
  uint8_t     board_type  = 0;   // VER/BT   board type
  uint8_t     fw_major    = 0;   // VER/Maj
  uint8_t     fw_minor    = 0;   // VER/Min
  uint8_t     fw_patch    = 0;   // VER/Pat
  uint8_t     fw_type     = 0;   // VER/FWT  0=dev, 64=beta, 128=official
  bool        valid       = false;
};

class ArdupilotParser
{
public:
  using ProgressCallback = std::function<bool(size_t pos, size_t total)>;
  using PlotSink         = std::function<PJ::PlotData*(const std::string& key, const std::string& unit)>;
  using StringSink       = std::function<PJ::StringSeries*(const std::string& key)>;

  explicit ArdupilotParser(const uint8_t* data, size_t length,
                           bool loadFiles = true,
                           bool hashInstance = false,
                           ProgressCallback progressCb = nullptr,
                           PlotSink plotSink = nullptr,
                           StringSink stringSink = nullptr);

  const ApSeriesMap&                    getSeriesMap()      const { return _series;        }
  const std::vector<ApMessageStats>&   getMessageStats()   const { return _stats;         }
  const std::vector<ApParameter>&      getParameters()     const { return _params;        }
  const std::vector<ApEmbeddedFile>&   getEmbeddedFiles()  const { return _embeddedFiles; }
  const std::vector<ApLogMessage>&     getLogMessages()    const { return _msgLog;        }
  const ApVersionInfo&                 getVersionInfo()    const { return _versionInfo;   }
  size_t                               getTotalSamples()   const { return _totalSamples;  }

private:
  void parse();
  bool parseSinglePass();                 // single scan: tables + lazy finalize + decode
  void finalizeOneDef(ApMessageDef& def); // per-type lazy finalize (called on first data packet)

  static ApMessageDef buildMessageDef(const uint8_t* payload86);
  static int          fieldByteSize(char c);
  static bool         isStringField(char c);
  static bool         isArrayField(char c);
  static std::vector<std::string> splitLabels(const char* buf, int len);
  static double       float16ToDouble(uint16_t bits);

  void applyFmtu(ApMessageDef& def, const char* units16, const char* mults16);
  void applyPendingFmtu(uint8_t msg_type);

  void parseDataPacket(const uint8_t* payload, const ApMessageDef& def);
  void parseUnitPacket(const uint8_t* payload, const ApMessageDef& def);
  void parseMultPacket(const uint8_t* payload, const ApMessageDef& def);
  void parseFmtuPacket(const uint8_t* payload, const ApMessageDef& def);
  void parseFilePacket(const uint8_t* payload, const ApMessageDef& def);
  void assembleEmbeddedFiles();

  double decodeField(const uint8_t* payload, size_t& offset, char fmt_char);

  const uint8_t* _data         = nullptr;
  size_t         _length       = 0;
  bool           _loadFiles    = true;
  bool           _hashInstance = false;
  ProgressCallback _progressCb;
  PlotSink         _plotSink;
  StringSink       _stringSink;
  uint8_t          _mavType = 0;  // MAV_TYPE from VER/BU; selects the mode name table

  std::array<ApMessageDef,  256> _fmtTable;
  bool                           _fmtValid[256]       = {};
  std::unordered_map<char,   std::string>    _unitTable;
  std::unordered_map<char,   double>         _multTable;
  std::array<ApFmtuPending,  256> _pendingFmtu;
  bool                            _pendingFmtuValid[256] = {};

  uint8_t _unitMsgType = 0;
  uint8_t _multMsgType = 0;
  uint8_t _fmtuMsgType = 0;
  uint8_t _fileMsgType = 0;

  double _lastTimestamp = 0.0;

  ApVersionInfo _versionInfo;
  bool          _verParsed = false;

  ApSeriesMap                  _series;
  std::vector<ApMessageStats>  _stats;
  size_t                       _totalSamples = 0;
  std::vector<ApParameter>                _params;
  std::unordered_map<std::string, size_t> _paramsIndex;

  std::unordered_map<std::string,
      std::vector<std::pair<uint32_t, std::vector<uint8_t>>>> _fileChunks;
  std::vector<ApEmbeddedFile> _embeddedFiles;
  std::vector<ApLogMessage>   _msgLog;
};
