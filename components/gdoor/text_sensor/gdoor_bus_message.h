#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "../gdoor_component.h"

namespace esphome {
namespace gdoor_esphome {

class GDoorBusMessage : public text_sensor::TextSensor, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_parent(GdoorComponent *parent) { this->parent_ = parent; }

 protected:
  GdoorComponent *parent_{nullptr};
  uint32_t last_bus_update_{0};
  uint32_t last_publish_time_{0};
  bool pending_idle_ = false;
};

}  // namespace gdoor_esphome
}  // namespace esphome