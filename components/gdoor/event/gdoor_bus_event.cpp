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
#include "esphome/core/macros.h"
#include "esphome/core/version.h"

namespace esphome {
namespace gdoor_esphome {

static const char *TAG = "gdoor_esphome.bus_event";

void GDoorBusEvent::setup() {
  // Registration with parent is done via Python-generated code (register_bus_event)
}

void GDoorBusEvent::dump_config() {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 11, 0)
  // get_name() deprecated in 2026.11, removed in 2027.5
  ESP_LOGCONFIG(TAG, "GDoor Bus Event '%s':", LOG_STR_ARG(this->get_log_name()));
#else
  ESP_LOGCONFIG(TAG, "GDoor Bus Event '%s':", this->get_name().c_str());
#endif
  char dc_buf[MAX_DEVICE_CLASS_LENGTH];
  const char *dc = this->get_device_class_to(dc_buf);
  ESP_LOGCONFIG(TAG, "  Device class: %s", dc[0] == '\0' ? "(none)" : dc);
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
