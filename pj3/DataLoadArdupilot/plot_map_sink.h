// SPDX-License-Identifier: MIT

#pragma once

#include <PlotJuggler/plotdata.h>

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "pj_drone_core/ardupilot/parser.hpp"

// Writes the parser's groups straight into the PlotJuggler 3 data map: one numeric series per numeric field and one
// string series per string field.
class PlotMapSink : public pj_drone::ardupilot::SampleSink
{
public:
  PlotMapSink(PJ::PlotDataMapRef& dest, bool show_units, bool official_compat)
    : _dest(dest), _show_units(show_units), _official_compat(official_compat)
  {
  }

  size_t declareGroup(const std::string& name, const std::vector<pj_drone::ardupilot::FieldSpec>& fields) override
  {
    std::vector<Column> series;
    series.reserve(fields.size());
    for (const auto& field : fields)
    {
      if (field.type == pj_drone::ardupilot::ValueType::kString)
        series.emplace_back(&_dest.getOrCreateStringSeries(displayName(name, field)));
      else
        series.emplace_back(&_dest.getOrCreateNumeric(displayName(name, field)));
    }
    _groups.push_back(std::move(series));
    return _groups.size() - 1;
  }

  bool append(size_t group, int64_t timestamp_ns, const pj_drone::ardupilot::SampleValue* values, size_t count) override
  {
    // The log stores microseconds; converting back keeps the exact value the log holds.
    const double t = static_cast<double>(timestamp_ns / 1000) * 1e-6;
    const auto& series = _groups[group];
    for (size_t i = 0; i < count; i++)
    {
      // The column alternative always matches the value alternative: both derive from FieldSpec::type.
      std::visit(
          [&](auto x) {
            if constexpr (std::is_same_v<decltype(x), std::string_view>)
              // Empty strings are dropped by StringSeries::pushBack.
              std::get<PJ::StringSeries*>(series[i])->pushBack(std::make_pair(t, PJ::StringRef(x)));
            else
              std::get<PJ::PlotData*>(series[i])->pushBack({ t, static_cast<double>(x) });
          },
          values[i]);
    }
    return true;  // cancelling goes through the parser's progress callback
  }

private:
  std::string displayName(const std::string& group, const pj_drone::ardupilot::FieldSpec& field) const
  {
    std::string key = group + "/" + field.name;
    if (_official_compat)
    {
      key.insert(key.begin(), '/');
    }
    if (_show_units && !field.unit.empty())
    {
      key.push_back('(');
      for (char ch : field.unit)
      {
        if (ch == '/')
          key += "\xe2\x88\x95";  // U+2215 DIVISION SLASH, so a unit never looks like a path separator
        else
          key.push_back(ch);
      }
      key.push_back(')');
    }
    return key;
  }

  PJ::PlotDataMapRef& _dest;
  bool _show_units;
  bool _official_compat;
  // One column per delivered field: a numeric or a string series.
  using Column = std::variant<PJ::PlotData*, PJ::StringSeries*>;
  std::vector<std::vector<Column>> _groups;
};

