// SPDX-License-Identifier: MIT

#pragma once

#include <PlotJuggler/plotdata.h>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "pj_drone_core/ardupilot/parser.hpp"

// Writes the parser's groups straight into the PlotJuggler 3 data map: one series per numeric field.
class PlotMapSink : public pj_drone::ardupilot::SampleSink
{
public:
  PlotMapSink(PJ::PlotDataMapRef& dest, bool show_units, bool official_compat,
              std::function<bool()> keep_going)
    : _dest(dest), _show_units(show_units), _official_compat(official_compat),
      _keep_going(std::move(keep_going))
  {
  }

  size_t declareGroup(const std::string& name, const std::vector<pj_drone::ardupilot::FieldSpec>& fields) override
  {
    std::vector<PJ::PlotData*> series;
    series.reserve(fields.size());
    for (const auto& field : fields)
    {
      series.push_back(&_dest.getOrCreateNumeric(displayName(name, field)));
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
      const double v = std::visit([](auto x) { return static_cast<double>(x); }, values[i]);
      series[i]->pushBack({ t, v });
    }
    return _keep_going();
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
          key += "∕";  // U+2215 DIVISION SLASH, so a unit never looks like a path separator
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
  std::function<bool()> _keep_going;
  std::vector<std::vector<PJ::PlotData*>> _groups;
};

