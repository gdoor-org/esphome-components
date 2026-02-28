/*
 * This file is part of the GDoor distribution (https://github.com/gdoor-org).
 * Copyright (c) 2024 GDoor authors.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gdoor_bus_event.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gdoor_esphome {

static const char *TAG = "gdoor_esphome.bus_event";

void GDoorBusEvent::setup() {
  // Registration with parent is done via Python-generated code (register_bus_event)
}

void GDoorBusEvent::dump_config() {
  ESP_LOGCONFIG(TAG, "GDoor Bus Event '%s':", this->get_name().c_str());
  const auto &dc = this->get_device_class_ref();
  ESP_LOGCONFIG(TAG, "  Device class: %s", dc.empty() ? "(none)" : dc.c_str());
  if (busdata_.empty()) {
    ESP_LOGCONFIG(TAG, "  Busdata filters: none (TX-only event)");
  } else {
    for (const auto &entry : busdata_) {
      ESP_LOGCONFIG(TAG, "  Busdata '%s' → event_type '%s'",
                    entry.first.c_str(), entry.second.c_str());
    }
  }
}

}  // namespace gdoor_esphome
}  // namespace esphome
