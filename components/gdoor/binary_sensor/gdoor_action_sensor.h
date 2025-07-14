#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../gdoor_component.h"

namespace esphome {
namespace gdoor_esphome {

class GDoorActionSensor : public binary_sensor::BinarySensor, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_parent(GdoorComponent *parent) { this->parent_ = parent; }
  void add_busdata(const std::string &busdata) { this->busdata_list_.push_back(busdata); }
  void set_busdata_list(const std::vector<std::string> &busdata) { this->busdata_list_ = busdata; }

 protected:
  GdoorComponent *parent_{nullptr};
  std::vector<std::string> busdata_list_;
  uint32_t last_bus_update_{0};
  uint32_t last_trigger_time_ = 0;
  bool pending_false_ = false;
};

}  // namespace gdoor_esphome
}  // namespace esphome