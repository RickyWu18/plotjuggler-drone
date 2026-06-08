/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ardupilot_parser.h"

#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <QDebug>

static constexpr uint8_t HEAD_BYTE1     = 0xA3;
static constexpr uint8_t HEAD_BYTE2     = 0x95;
static constexpr uint8_t FMT_MSGID      = 128;
static constexpr int     FMT_PAYLOAD_LEN = 86;

ArdupilotParser::ArdupilotParser(const uint8_t* data, size_t length,
                                 bool loadFiles, bool hashInstance,
                                 ProgressCallback progressCb,
                                 PlotSink plotSink,
                                 StringSink stringSink)
  : _data(data), _length(length), _loadFiles(loadFiles), _hashInstance(hashInstance),
    _progressCb(std::move(progressCb)), _plotSink(std::move(plotSink)),
    _stringSink(std::move(stringSink))
{
  // Seed built-in multiplier table so scaling works before MULT packets arrive
  _multTable['?'] = 1.0;
  _multTable['-'] = 1.0;   // "no units" — treat as identity rather than zero
  _multTable['0'] = 1.0;
  _multTable['A'] = 1e-1;
  _multTable['B'] = 1e-2;
  _multTable['C'] = 1e-3;
  _multTable['D'] = 1e-4;
  _multTable['E'] = 1e-5;
  _multTable['F'] = 1e-6;
  _multTable['G'] = 1e-7;
  _multTable['I'] = 1e-9;
  _multTable['1'] = 1e1;
  _multTable['2'] = 1e2;
  _multTable['!'] = 3.6;
  _multTable['/'] = 3600.0;

  parse();
}

void ArdupilotParser::parse()
{
  if (!parseSinglePass()) return;

  // Post-pass: resolve any units still unset (unit_id set, unit string empty)
  for (auto& [key, series] : _series)
  {
    if (series.unit.empty() && series.unit_id != '?')
    {
      auto unit_it = _unitTable.find(series.unit_id);
      if (unit_it != _unitTable.end() && !unit_it->second.empty())
        series.unit = unit_it->second;
    }
  }

  if (_loadFiles) assembleEmbeddedFiles();
}

void ArdupilotParser::finalizeOneDef(ApMessageDef& def)
{
  def.finalized = true;

  const bool is_special = (def.msg_type == _unitMsgType || def.msg_type == _multMsgType ||
                           def.msg_type == _fmtuMsgType || def.msg_type == _fileMsgType);

  // Cache mult_val on every field so the decode hot-path needs no map lookup
  for (auto& field : def.fields)
  {
    if (field.mult_id == '?')
    {
      switch (field.fmt_char)
      {
        case 'c': case 'C': field.mult_val = 1e-2; break;
        case 'e': case 'E': field.mult_val = 1e-4; break;
        case 'L':           field.mult_val = 1e-7; break;
        default:            field.mult_val = 1.0;  break;
      }
    }
    else if (field.mult_id == '-')
    {
      field.mult_val = 1.0;
    }
    else
    {
      auto it = _multTable.find(field.mult_id);
      field.mult_val = (it != _multTable.end() && it->second != 0.0) ? it->second : 1.0;
    }
  }

  if (is_special) return;

  // Pre-cache timestamp byte offset
  if (def.timestamp_idx >= 0)
  {
    int off = 0;
    for (int i = 0; i < def.timestamp_idx; i++)
      off += def.fields[i].byte_size;
    def.timestamp_byte_offset = off;
  }

  // Pre-build series keys for non-instance messages
  if (def.instance_idx < 0)
  {
    def.series_keys.resize(def.fields.size());
    for (size_t i = 0; i < def.fields.size(); i++)
    {
      const auto& f = def.fields[i];
      if (!f.is_string && !f.is_array)
        def.series_keys[i] = def.name + "/" + f.label;
    }
  }

  // Init stats entry
  def.stats_idx = static_cast<int>(_stats.size());
  _stats.push_back({def.name, 0});

  // Cache series pointers for non-instance messages (①⑤)
  if (def.instance_idx < 0)
  {
    def.series_ptrs.resize(def.fields.size(), nullptr);
    if (_plotSink) def.plot_ptrs.resize(def.fields.size(), nullptr);
    for (size_t i = 0; i < def.series_keys.size(); i++)
    {
      if (!def.series_keys[i].empty())
      {
        def.series_ptrs[i] = &_series[def.series_keys[i]];
        if (_plotSink)
        {
          const auto& f = def.fields[i];
          std::string unit_str;
          if (f.unit_id != '?' && f.unit_id != '-')
          {
            auto uit = _unitTable.find(f.unit_id);
            if (uit != _unitTable.end()) unit_str = uit->second;
          }
          def.plot_ptrs[i] = _plotSink(def.series_keys[i], unit_str);
        }
      }
    }

    if (_stringSink)
    {
      def.str_ptrs.resize(def.fields.size(), nullptr);
      for (size_t i = 0; i < def.fields.size(); i++)
        if (def.fields[i].is_string)
          def.str_ptrs[i] = _stringSink(def.name + "/" + def.fields[i].label);
    }
  }
}

bool ArdupilotParser::parseSinglePass()
{
  size_t pos = 0;
  int last_percent = -1;

  while (pos + 3 <= _length)
  {
    if (_data[pos] != HEAD_BYTE1 || _data[pos + 1] != HEAD_BYTE2)
    {
      pos++;
      continue;
    }

    const uint8_t msgid = _data[pos + 2];
    pos += 3;

    if (msgid == FMT_MSGID)
    {
      if (pos + static_cast<size_t>(FMT_PAYLOAD_LEN) > _length) break;

      ApMessageDef def = buildMessageDef(_data + pos);
      pos += static_cast<size_t>(FMT_PAYLOAD_LEN);

      const uint8_t mtype = def.msg_type;
      _fmtTable[mtype] = std::move(def);
      _fmtValid[mtype] = true;

      const auto& name = _fmtTable[mtype].name;
      if      (name == "UNIT") _unitMsgType = mtype;
      else if (name == "MULT") _multMsgType = mtype;
      else if (name == "FMTU") _fmtuMsgType = mtype;
      else if (name == "FILE") _fileMsgType = mtype;

      applyPendingFmtu(mtype);
    }
    else
    {
      if (!_fmtValid[msgid]) break;

      ApMessageDef& def = _fmtTable[msgid];
      if (def.msg_len < 3) break;

      const size_t payload_len = static_cast<size_t>(def.msg_len) - 3;
      if (pos + payload_len > _length) break;

      const uint8_t* payload = _data + pos;
      pos += payload_len;

      if      (msgid == _unitMsgType) parseUnitPacket(payload, def);
      else if (msgid == _multMsgType) parseMultPacket(payload, def);
      else if (msgid == _fmtuMsgType) parseFmtuPacket(payload, def);
      else if (msgid == _fileMsgType) { if (_loadFiles) parseFilePacket(payload, def); }
      else
      {
        if (!def.finalized) finalizeOneDef(def);
        parseDataPacket(payload, def);
      }
    }

    if (_progressCb)
    {
      const int pct = static_cast<int>(100.0 * pos / _length);
      if (pct != last_percent)
      {
        last_percent = pct;
        if (!_progressCb(pos, _length)) return false;
      }
    }
  }

  return true;
}

ApMessageDef ArdupilotParser::buildMessageDef(const uint8_t* payload86)
{
  ApMessageDef def;
  def.msg_type = payload86[0];
  def.msg_len  = payload86[1];

  const char* name_buf = reinterpret_cast<const char*>(payload86 + 2);
  const size_t name_len = strnlen(name_buf, 4);
  def.name = std::string(name_buf, name_len);

  const char* fmt_buf    = reinterpret_cast<const char*>(payload86 + 6);
  const char* labels_buf = reinterpret_cast<const char*>(payload86 + 22);
  auto labels = splitLabels(labels_buf, 64);

  for (size_t i = 0; i < 16 && fmt_buf[i] != '\0'; i++)
  {
    const char c = fmt_buf[i];
    ApFieldDef field;
    field.fmt_char  = c;
    field.byte_size = fieldByteSize(c);
    field.is_string = isStringField(c);
    field.is_array  = isArrayField(c);
    if (i < labels.size()) field.label = labels[i];

    if (c == 'Q' && field.label == "TimeUS")
      def.timestamp_idx = static_cast<int>(i);

    if ((c == 'B' || c == 'M') &&
        (field.label == "I" || field.label == "Instance"))
      def.instance_idx = static_cast<int>(i);

    def.fields.push_back(std::move(field));
  }

  return def;
}

int ArdupilotParser::fieldByteSize(char c)
{
  switch (c)
  {
    case 'Q': case 'q': case 'd':              return 8;
    case 'f': case 'I': case 'i':
    case 'L': case 'e': case 'E':              return 4;
    case 'H': case 'h': case 'c':
    case 'C': case 'g':                        return 2;
    case 'B': case 'M': case 'b':              return 1;
    case 'n':                                  return 4;
    case 'N':                                  return 16;
    case 'Z':                                  return 64;
    case 'a':                                  return 64;
    default:                                   return 0;
  }
}

bool ArdupilotParser::isStringField(char c)
{
  return c == 'n' || c == 'N' || c == 'Z';
}

bool ArdupilotParser::isArrayField(char c)
{
  return c == 'a';
}

std::vector<std::string> ArdupilotParser::splitLabels(const char* buf, int len)
{
  std::vector<std::string> result;
  std::string token;
  for (int i = 0; i < len && buf[i] != '\0'; i++)
  {
    if (buf[i] == ',')
    {
      result.push_back(token);
      token.clear();
    }
    else
    {
      token += buf[i];
    }
  }
  if (!token.empty()) result.push_back(token);
  return result;
}

double ArdupilotParser::float16ToDouble(uint16_t bits)
{
  const uint32_t sign     = (bits >> 15) & 0x1;
  const uint32_t exponent = (bits >> 10) & 0x1F;
  const uint32_t mantissa =  bits        & 0x3FF;

  uint32_t f;
  if (exponent == 0)
  {
    if (mantissa == 0)
    {
      f = sign << 31;
    }
    else
    {
      int32_t  e = 1;
      uint32_t m = mantissa;
      while (!(m & 0x400)) { m <<= 1; e--; }
      m &= ~0x400u;
      f = (sign << 31) | (static_cast<uint32_t>(e + 127 - 15) << 23) | (m << 13);
    }
  }
  else if (exponent == 31)
  {
    f = (sign << 31) | (0xFFu << 23) | (mantissa << 13);
  }
  else
  {
    f = (sign << 31) | ((exponent + 127 - 15) << 23) | (mantissa << 13);
  }

  float result;
  memcpy(&result, &f, 4);
  return static_cast<double>(result);
}

double ArdupilotParser::decodeField(const uint8_t* payload, size_t& offset, char fmt_char)
{
  double value = 0.0;
  switch (fmt_char)
  {
    case 'Q': { uint64_t v; memcpy(&v, payload + offset, 8); value = static_cast<double>(v); offset += 8; break; }
    case 'q': { int64_t  v; memcpy(&v, payload + offset, 8); value = static_cast<double>(v); offset += 8; break; }
    case 'd': { double   v; memcpy(&v, payload + offset, 8); value = v;                       offset += 8; break; }
    case 'f': { float    v; memcpy(&v, payload + offset, 4); value = static_cast<double>(v); offset += 4; break; }
    case 'I': { uint32_t v; memcpy(&v, payload + offset, 4); value = static_cast<double>(v); offset += 4; break; }
    case 'i': { int32_t  v; memcpy(&v, payload + offset, 4); value = static_cast<double>(v); offset += 4; break; }
    case 'L': { int32_t  v; memcpy(&v, payload + offset, 4); value = static_cast<double>(v); offset += 4; break; }
    case 'e': { int32_t  v; memcpy(&v, payload + offset, 4); value = static_cast<double>(v); offset += 4; break; }
    case 'E': { uint32_t v; memcpy(&v, payload + offset, 4); value = static_cast<double>(v); offset += 4; break; }
    case 'H': { uint16_t v; memcpy(&v, payload + offset, 2); value = static_cast<double>(v); offset += 2; break; }
    case 'h': { int16_t  v; memcpy(&v, payload + offset, 2); value = static_cast<double>(v); offset += 2; break; }
    case 'c': { int16_t  v; memcpy(&v, payload + offset, 2); value = static_cast<double>(v); offset += 2; break; }
    case 'C': { uint16_t v; memcpy(&v, payload + offset, 2); value = static_cast<double>(v); offset += 2; break; }
    case 'g': { uint16_t v; memcpy(&v, payload + offset, 2); value = float16ToDouble(v);     offset += 2; break; }
    case 'B': case 'M': { uint8_t v; memcpy(&v, payload + offset, 1); value = static_cast<double>(v); offset += 1; break; }
    case 'b': { int8_t   v; memcpy(&v, payload + offset, 1); value = static_cast<double>(v); offset += 1; break; }
    default: break;
  }
  return value;
}

void ArdupilotParser::parseDataPacket(const uint8_t* payload, const ApMessageDef& def)
{
  // Extract PARM name/value before the generic numeric pass skips string fields
  if (def.name == "PARM")
  {
    std::string parm_name;
    double      parm_value = 0.0;
    size_t      parm_offset = 0;
    for (const auto& field : def.fields)
    {
      if (field.label == "Name" && field.is_string)
      {
        const char* s = reinterpret_cast<const char*>(payload + parm_offset);
        parm_name = std::string(s, strnlen(s, static_cast<size_t>(field.byte_size)));
      }
      else if (field.label == "Value" && field.fmt_char == 'f')
      {
        float v; memcpy(&v, payload + parm_offset, 4);
        parm_value = static_cast<double>(v);
      }
      parm_offset += static_cast<size_t>(field.byte_size);
    }
    if (!parm_name.empty())
    {
      auto it = _paramsIndex.find(parm_name);
      if (it != _paramsIndex.end())
        _params[it->second].value = parm_value;
      else
      {
        _paramsIndex[parm_name] = _params.size();
        _params.push_back({parm_name, parm_value});
      }
    }
  }

  // Decode all numeric fields; skip the timestamp field (P4) — extracted separately below
  size_t offset = 0;
  double values[16] = {};        // stack buffer; max 16 fields per message
  size_t str_offsets[16] = {};   // byte offset in payload for each string field

  for (size_t i = 0; i < def.fields.size(); i++)
  {
    const auto& field = def.fields[i];
    if (field.is_string || field.is_array)
    {
      str_offsets[i] = offset;
      offset += static_cast<size_t>(field.byte_size);
    }
    else
      values[i] = decodeField(payload, offset, field.fmt_char);
  }

  // Extract timestamp from raw bytes to preserve full uint64 precision (P4)
  double timestamp = _lastTimestamp;
  if (def.timestamp_byte_offset >= 0)
  {
    uint64_t ts_raw;
    memcpy(&ts_raw, payload + def.timestamp_byte_offset, 8);
    timestamp = static_cast<double>(ts_raw) * 1e-6;
    _lastTimestamp = timestamp;
  }

  // Extract MSG text and store with timestamp
  if (def.name == "MSG")
  {
    size_t msg_off = 0;
    for (const auto& field : def.fields)
    {
      if (field.label == "Message" && field.is_string)
      {
        const char* s = reinterpret_cast<const char*>(payload + msg_off);
        std::string text(s, strnlen(s, static_cast<size_t>(field.byte_size)));
        if (!text.empty())
          _msgLog.push_back({timestamp, std::move(text)});
        break;
      }
      msg_off += static_cast<size_t>(field.byte_size);
    }
  }

  // Extract instance index for multi-sensor messages
  int instance = -1;
  if (def.instance_idx >= 0 &&
      def.instance_idx < static_cast<int>(def.fields.size()))
    instance = static_cast<int>(values[def.instance_idx]);

  // Ensure instance series pointers are built on first encounter (①)
  std::vector<ApSeries*>*          inst_ptrs    = nullptr;
  std::vector<PJ::PlotData*>*      inst_pptrs   = nullptr;
  std::vector<PJ::StringSeries*>*  inst_str_ptrs = nullptr;
  if (instance >= 0)
  {
    auto& ptr_vec   = def.instance_ptrs[instance];
    auto& pplot_vec = def.instance_plot_ptrs[instance];
    auto& str_vec   = def.instance_str_ptrs[instance];
    if (ptr_vec.empty())
    {
      ptr_vec.resize(def.fields.size(), nullptr);
      if (_plotSink)   pplot_vec.resize(def.fields.size(), nullptr);
      if (_stringSink) str_vec.resize(def.fields.size(), nullptr);
      const std::string prefix = def.name + "/" + (_hashInstance ? "#" : "") + std::to_string(instance);
      for (size_t j = 0; j < def.fields.size(); j++)
      {
        const auto& f = def.fields[j];
        const std::string key = prefix + "/" + f.label;
        if (!f.is_string && !f.is_array)
        {
          ptr_vec[j] = &_series[key];
          if (_plotSink)
          {
            std::string unit_str;
            if (f.unit_id != '?' && f.unit_id != '-')
            {
              auto uit = _unitTable.find(f.unit_id);
              if (uit != _unitTable.end()) unit_str = uit->second;
            }
            pplot_vec[j] = _plotSink(key, unit_str);
          }
        }
        else if (f.is_string && _stringSink)
        {
          str_vec[j] = _stringSink(key);
        }
      }
    }
    inst_ptrs     = &ptr_vec;
    inst_pptrs    = &pplot_vec;
    inst_str_ptrs = &str_vec;
  }

  // Emit numeric series (hot path: use cached ApSeries* to skip per-sample string hash)
  for (size_t i = 0; i < def.fields.size(); i++)
  {
    const auto& field = def.fields[i];
    if (field.is_string || field.is_array) continue;

    const double scaled = values[i] * field.mult_val;  // mult_val pre-cached in finalizeDefs

    // Resolve ApSeries pointer: cached for non-instance (finalizeDefs) and instance (first encounter above)
    ApSeries* sp;
    if (inst_ptrs)
    {
      sp = (*inst_ptrs)[i];
    }
    else if (i < def.series_ptrs.size() && def.series_ptrs[i])
    {
      sp = def.series_ptrs[i];
    }
    else
    {
      // Fallback: messages not counted in pass 1 (rare)
      const std::string* key_ptr;
      std::string key_buf;
      if (!def.series_keys.empty() && !def.series_keys[i].empty())
        key_ptr = &def.series_keys[i];
      else
      {
        key_buf = def.name + "/" + field.label;
        key_ptr = &key_buf;
      }
      sp = &_series[*key_ptr];
    }

    auto& series = *sp;
    if (!series.unit_resolved)
    {
      if (series.unit_id == '?' && field.unit_id != '?')
        series.unit_id = field.unit_id;
      if (series.unit_id != '?')
      {
        auto unit_it = _unitTable.find(series.unit_id);
        if (unit_it != _unitTable.end() && !unit_it->second.empty())
          series.unit = unit_it->second;
      }
      series.unit_resolved = true;
    }

    // Resolve write-through PlotData* (⑤): cached alongside series_ptrs / instance_ptrs
    PJ::PlotData* pp = nullptr;
    if (inst_pptrs && i < inst_pptrs->size())
      pp = (*inst_pptrs)[i];
    else if (i < def.plot_ptrs.size())
      pp = def.plot_ptrs[i];

    if (pp)
      pp->pushBack({timestamp, scaled});
    else
      series.points.push_back({timestamp, scaled});
    _totalSamples++;
  }

  // Emit string series
  if (_stringSink)
  {
    for (size_t i = 0; i < def.fields.size(); i++)
    {
      const auto& field = def.fields[i];
      if (!field.is_string) continue;

      PJ::StringSeries* ssp = nullptr;
      if (inst_str_ptrs && i < inst_str_ptrs->size())
        ssp = (*inst_str_ptrs)[i];
      else if (i < def.str_ptrs.size())
        ssp = def.str_ptrs[i];

      if (!ssp) continue;
      const char* s = reinterpret_cast<const char*>(payload + str_offsets[i]);
      const size_t slen = strnlen(s, static_cast<size_t>(field.byte_size));
      if (slen > 0)
        ssp->pushBack({ timestamp, PJ::StringRef(s, slen) });
    }
  }

  // Update message statistics via pre-cached index (P3)
  if (def.stats_idx >= 0)
    _stats[def.stats_idx].count++;
}

void ArdupilotParser::parseUnitPacket(const uint8_t* payload, const ApMessageDef& def)
{
  size_t offset = 0;
  char type_id = 0;
  std::string unit_str;

  for (const auto& field : def.fields)
  {
    if (field.label == "Id" && (field.fmt_char == 'b' || field.fmt_char == 'B'))
    {
      int8_t v; memcpy(&v, payload + offset, 1);
      type_id = static_cast<char>(v);
    }
    else if (field.label == "Label" && field.is_string)
    {
      const char* s = reinterpret_cast<const char*>(payload + offset);
      unit_str = std::string(s, strnlen(s, static_cast<size_t>(field.byte_size)));
    }
    offset += static_cast<size_t>(field.byte_size);
  }

  if (type_id != 0) _unitTable[type_id] = unit_str;
}

void ArdupilotParser::parseMultPacket(const uint8_t* payload, const ApMessageDef& def)
{
  size_t offset = 0;
  char type_id = 0;
  double mult_val = 1.0;

  for (const auto& field : def.fields)
  {
    if (field.label == "Id" && (field.fmt_char == 'b' || field.fmt_char == 'B'))
    {
      int8_t v; memcpy(&v, payload + offset, 1);
      type_id = static_cast<char>(v);
    }
    else if (field.label == "Mult" && field.fmt_char == 'd')
    {
      memcpy(&mult_val, payload + offset, 8);
    }
    offset += static_cast<size_t>(field.byte_size);
  }

  if (type_id != 0) _multTable[type_id] = mult_val;
}

void ArdupilotParser::parseFmtuPacket(const uint8_t* payload, const ApMessageDef& def)
{
  size_t offset = 0;
  uint8_t fmt_type = 0;
  char units[16] = {};
  char mults[16] = {};

  for (const auto& field : def.fields)
  {
    if (field.label == "FmtType")
    {
      if (field.fmt_char == 'B' || field.fmt_char == 'b')
      {
        memcpy(&fmt_type, payload + offset, 1);
      }
      else if (field.fmt_char == 'H')
      {
        uint16_t v; memcpy(&v, payload + offset, 2);
        fmt_type = static_cast<uint8_t>(v);
      }
    }
    else if (field.label == "UnitIds" && field.is_string)
    {
      const size_t copy_len = std::min(static_cast<size_t>(field.byte_size), size_t(16));
      memcpy(units, payload + offset, copy_len);
    }
    else if (field.label == "MultIds" && field.is_string)
    {
      const size_t copy_len = std::min(static_cast<size_t>(field.byte_size), size_t(16));
      memcpy(mults, payload + offset, copy_len);
    }
    offset += static_cast<size_t>(field.byte_size);
  }

  if (_fmtValid[fmt_type])
  {
    applyFmtu(_fmtTable[fmt_type], units, mults);
  }
  else
  {
    memcpy(_pendingFmtu[fmt_type].units,       units, 16);
    memcpy(_pendingFmtu[fmt_type].multipliers, mults, 16);
    _pendingFmtuValid[fmt_type] = true;
  }
}

void ArdupilotParser::applyFmtu(ApMessageDef& def, const char* units16, const char* mults16)
{
  const size_t n = std::min(def.fields.size(), size_t(16));
  for (size_t i = 0; i < n; i++)
  {
    if (units16[i] != '\0') def.fields[i].unit_id = units16[i];
    if (mults16[i] != '\0') def.fields[i].mult_id = mults16[i];
  }
}

void ArdupilotParser::applyPendingFmtu(uint8_t msg_type)
{
  if (!_pendingFmtuValid[msg_type]) return;

  applyFmtu(_fmtTable[msg_type], _pendingFmtu[msg_type].units, _pendingFmtu[msg_type].multipliers);
  _pendingFmtuValid[msg_type] = false;
}

void ArdupilotParser::parseFilePacket(const uint8_t* payload, const ApMessageDef& def)
{
  std::string    filename;
  uint32_t       offset   = 0;
  uint8_t        length   = 0;
  const uint8_t* data_ptr = nullptr;
  size_t         off      = 0;

  for (const auto& field : def.fields)
  {
    if (field.label == "FileName" && field.is_string)
    {
      const char* s = reinterpret_cast<const char*>(payload + off);
      filename = std::string(s, strnlen(s, static_cast<size_t>(field.byte_size)));
    }
    else if (field.label == "Offset" && field.fmt_char == 'I')
      memcpy(&offset, payload + off, 4);
    else if (field.label == "Length" && field.fmt_char == 'B')
      length = payload[off];
    else if (field.label == "Data")
      data_ptr = payload + off;

    off += static_cast<size_t>(field.byte_size);
  }

  if (filename.empty() || data_ptr == nullptr || length == 0) return;
  if (length > 64) length = 64;

  _fileChunks[filename].emplace_back(
      offset, std::vector<uint8_t>(data_ptr, data_ptr + length));
}

void ArdupilotParser::assembleEmbeddedFiles()
{
  for (auto& [name, chunks] : _fileChunks)
  {
    std::sort(chunks.begin(), chunks.end(),
              [](const auto& a, const auto& b){ return a.first < b.first; });

    size_t total = 0;
    for (const auto& [chunk_offset, chunk_data] : chunks)
    {
      const size_t end = chunk_offset + chunk_data.size();
      if (end > total) total = end;
    }

    ApEmbeddedFile ef;
    ef.name = name;
    ef.data.resize(total, 0);
    for (const auto& [chunk_offset, chunk_data] : chunks)
      std::copy(chunk_data.begin(), chunk_data.end(), ef.data.begin() + chunk_offset);

    _embeddedFiles.push_back(std::move(ef));
  }
}
