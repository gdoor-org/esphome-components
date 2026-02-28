#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "gdoor_action_sensor.h"

namespace esphome {
namespace gdoor_esphome {

static const char *TAG = "gdoor_esphome.action_sensor";

void GDoorActionSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up GDoorActionSensor...");
  this->publish_state(false);
  if (this->parent_ != nullptr) {
    this->parent_->register_bus_listener(this);
  } else {
    ESP_LOGW(TAG, "Parent component not set!");
  }
}

void GDoorActionSensor::on_bus_message(const std::string &busdata_hex) {
  // Direct string equality — no JSON search
  for (const auto &busdata : this->busdata_list_) {
    if (busdata == busdata_hex) {
      ESP_LOGVV(TAG, "Matched busdata: %s", busdata.c_str());
      this->publish_state(true);
      this->last_trigger_time_ = millis();
      this->pending_false_ = true;
      return;
    }
  }
}

void GDoorActionSensor::loop() {
  // Matching moved to on_bus_message(); loop() only handles the reset timer
  if (this->pending_false_ && millis() - this->last_trigger_time_ >= 500) {
    this->publish_state(false);
    this->pending_false_ = false;
  }
}

void GDoorActionSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "GDoor Action Sensor binary_sensor");
  for (const auto &busdata : this->busdata_list_) {
    ESP_LOGCONFIG(TAG, "  Busdata filter: %s", busdata.c_str());
  }
}

}  // namespace gdoor_esphome
}  // namespace esphome
