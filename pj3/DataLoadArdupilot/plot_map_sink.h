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
      Column col;
      if (field.type == pj_drone::ardupilot::ValueType::kString)
        col.text = &_dest.getOrCreateStringSeries(displayName(name, field));
      else
        col.number = &_dest.getOrCreateNumeric(displayName(name, field));
      series.push_back(col);
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
      if (series[i].text != nullptr)
      {
        // Empty strings are dropped by StringSeries::pushBack.
        series[i].text->pushBack(std::make_pair(t, PJ::StringRef(std::get<std::string_view>(values[i]))));
        continue;
      }
      const double v = std::visit(
          [](auto x) {
            if constexpr (std::is_arithmetic_v<decltype(x)>)
              return static_cast<double>(x);
            else
              return 0.0;  // not reachable: string fields go to a StringSeries above
          },
          values[i]);
      series[i].number->pushBack({ t, v });
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
  // One column per delivered field: exactly one of the two pointers is set.
  struct Column
  {
    PJ::PlotData* number = nullptr;
    PJ::StringSeries* text = nullptr;
  };
  std::vector<std::vector<Column>> _groups;
};

