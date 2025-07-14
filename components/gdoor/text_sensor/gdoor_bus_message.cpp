#include "esphome/core/log.h"
#include "gdoor_bus_message.h"

namespace esphome {
namespace gdoor_esphome {

static const char *TAG = "gdoor_esphome.bus_message";

void GDoorBusMessage::setup() {
  ESP_LOGI(TAG, "Setting up GDoorBusMessage text_sensor");
  if (this->parent_ != nullptr) {
    this->parent_->setup();
  }
  publish_state("BUS_IDLE");
}

void GDoorBusMessage::loop() {
  const uint32_t now = millis();
  if (this->parent_ != nullptr) {
    uint32_t parent_timestamp = this->parent_->get_last_bus_update();
    if (parent_timestamp != this->last_bus_update_) {
      std::string current_message = this->parent_->get_last_rx_data_str();
      publish_state(current_message.c_str());
      ESP_LOGVV("GDoorBusMessage", "Published bus message: %s", current_message.c_str());
      // Schedule BUS_IDLE state after 500ms
      this->last_publish_time_ = now;
      this->pending_idle_ = true;
      this->last_bus_update_ = parent_timestamp;
    }
  } else {
    ESP_LOGW("GDoorBusMessage", "Parent component is null!");
  }
  // Handle delayed BUS_IDLE publish
  if (this->pending_idle_ && now - this->last_publish_time_ >= 500) {
    publish_state("BUS_IDLE");
    ESP_LOGVV("GDoorBusMessage", "Switched to BUS_IDLE.");
    this->pending_idle_ = false;
  }
}

void GDoorBusMessage::dump_config() {
  ESP_LOGCONFIG(TAG, "GDoor Bus Message text sensor");
}

}  // namespace gdoor_esphome
}  // namespace esphome
