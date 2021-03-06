/*
 * Copyright (C) 2016  Marco Gulino <marco@gulinux.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "asicontrol.h"
#include "zwoexception.h"
#include "c++/stringbuilder.h"
#include <set>
#include <QHash>
#include <QObject>

using namespace std;
using namespace GuLinux;

namespace {
  ASI_BOOL bool2asi(bool value) {
    return value ? ASI_TRUE : ASI_FALSE;
  }
  
  bool asi2bool(ASI_BOOL asi_bool) {
    return asi_bool == ASI_TRUE;
  }
}

ASIControl::ASIControl(int index, int camera_id) : index{index}, camera_id{camera_id}
{
  ASI_CHECK << ASIGetControlCaps(camera_id, index, &caps) << "Get control caps";
  reload();
}

Imager::Control ASIControl::control() const
{
  return *this;
}

ASIControl &ASIControl::reload()
{
  ASI_BOOL read_is_auto;
  ASI_CHECK << ASIGetControlValue(camera_id, caps.ControlType, &value, &read_is_auto)
            << (stringbuilder() << "Get control value: " << caps.Name);
  qDebug() << "Got raw control value for " << caps.Name << "(" << caps.ControlType << ")" << ": value=" << value << ", is_auto=" << read_is_auto;
  this->is_auto = asi2bool(read_is_auto);
  return *this;
}

ASIControl &ASIControl::set(qlonglong new_value, bool write_is_auto)
{
  if(write_is_auto) {
    new_value = reload().value;
  }
  qDebug() << "Setting control " << caps.Name << "(" << caps.ControlType << ") to value " << new_value << ", auto=" << write_is_auto;
  ASI_CHECK << ASISetControlValue(camera_id, caps.ControlType, new_value, bool2asi(write_is_auto) )
            << (stringbuilder() << "Set new control value: " << caps.Name << " to " << new_value << " (auto: " << write_is_auto << ")");
  reload();
  return *this;
}

ASIControl::operator Imager::Control() const
{
  static QHash<ASI_CONTROL_TYPE, QString> labels{
    { ASI_GAIN, QObject::tr("Gain") },
    { ASI_EXPOSURE, QObject::tr("Exposure`") },
    { ASI_GAMMA, QObject::tr("Gamma") },
    { ASI_WB_R, QObject::tr("White Balance (R)") },
    { ASI_WB_B, QObject::tr("White Balance (B)") },
    { ASI_BRIGHTNESS, QObject::tr("Brightness") },
    { ASI_BANDWIDTHOVERLOAD, QObject::tr("Bandwidth") },
    { ASI_OVERCLOCK, QObject::tr("Overclock") },
    { ASI_TEMPERATURE, QObject::tr("Temperature") },
    { ASI_FLIP, QObject::tr("Flip") },
    { ASI_AUTO_MAX_GAIN, QObject::tr("Max gain") },
    { ASI_AUTO_MAX_EXP, QObject::tr("Max exposure") },
    { ASI_AUTO_MAX_BRIGHTNESS, QObject::tr("Max brightness") },
    { ASI_HARDWARE_BIN, QObject::tr("Hardware bin") },
    { ASI_HIGH_SPEED_MODE, QObject::tr("High speed") },
    { ASI_COOLER_POWER_PERC, QObject::tr("Cooler power (%)") },
    { ASI_TARGET_TEMP, QObject::tr("Target temp") },
    { ASI_COOLER_ON, QObject::tr("Cooler on") },
    { ASI_MONO_BIN, QObject::tr("Mono bin") },
    { ASI_FAN_ON, QObject::tr("Fan on") },
    { ASI_PATTERN_ADJUST, QObject::tr("Pattern adjust") },
    { ASI_ANTI_DEW_HEATER, QObject::tr("Anti dew heater") },
  };
  auto control = Imager::Control{ static_cast<qlonglong>(caps.ControlType), labels.value(caps.ControlType, caps.Description) }
      .set_range(caps.MinValue, caps.MaxValue, 1l)
      .set_value(value)
      .set_default_value(caps.DefaultValue)
      .set_decimals(0)
      ;

  static std::set<ASI_CONTROL_TYPE> boolean_caps {ASI_HIGH_SPEED_MODE, ASI_HARDWARE_BIN};
  if(boolean_caps.count(caps.ControlType))
    control.type = Imager::Control::Bool;

  if(caps.ControlType == ASI_EXPOSURE) {
    control.is_duration = true;
    control.duration_unit = 1us;
  }
  if(caps.ControlType == ASI_TEMPERATURE) {
    qDebug() << "Converting temperature from " << value;
    control.decimals = 1;
    control.value = static_cast<double>(value) / 10.;
  }
  if(caps.ControlType == ASI_FLIP) {
    control.type = Imager::Control::Combo;
    control.choices = {
      {"no", static_cast<int>(ASI_FLIP_NONE)},
      {"horizontal", static_cast<int>(ASI_FLIP_HORIZ)},
      {"vertical", static_cast<int>(ASI_FLIP_VERT)},
      {"horizontal and vertical", static_cast<int>(ASI_FLIP_BOTH)},
    };
  }
  control.readonly = !caps.IsWritable;
  control.value_auto = caps.IsAutoSupported && is_auto;
  control.supports_auto = caps.IsAutoSupported;
  control.is_exposure = caps.ControlType == ASI_EXPOSURE;
  return control;
}
