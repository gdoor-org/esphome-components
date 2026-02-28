#include "gdoor_component.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace gdoor_esphome {

static const char *TAG = "gdoor_esphome.gdoor_component";

void GdoorComponent::set_tx_pin(GPIOPin *tx_pin) {
  this->tx_pin_ = tx_pin;
}

void GdoorComponent::set_tx_en_pin(GPIOPin *tx_en_pin) {
  this->tx_en_pin_ = tx_en_pin;
}

void GdoorComponent::set_rx_pin(GPIOPin *rx_pin) {
  this->rx_pin_ = rx_pin;
}

void GdoorComponent::set_rx_thresh_pin(GPIOPin *rx_thresh_pin) {
  this->rx_thresh_pin_ = rx_thresh_pin;
}

void GdoorComponent::set_rx_sens(float rx_sens) {
  this->rx_sens_ = rx_sens;
}

void GdoorComponent::set_last_rx_data(GDOOR_DATA *data) {
  this->last_rx_data_ = data;
}

void GdoorComponent::send_bus_message(const std::string &payload) {
  ESP_LOGVV(TAG, "Writing bus data: %s", payload.c_str());
  GDOOR::send(payload.c_str());
}

void GdoorComponent::setup() {
    if (this->tx_pin_ == nullptr || this->tx_en_pin_ == nullptr || this->rx_pin_ == nullptr) {
        ESP_LOGE(TAG, "One or more pins are not configured properly!");
        return;
    }

    // Convert GPIOPin to InternalGPIOPin and extract pin numbers
    auto *tx_internal_pin = static_cast<esphome::InternalGPIOPin *>(this->tx_pin_);
    auto *tx_en_internal_pin = static_cast<esphome::InternalGPIOPin *>(this->tx_en_pin_);
    auto *rx_internal_pin = static_cast<esphome::InternalGPIOPin *>(this->rx_pin_);
    auto *rx_thresh_internal_pin = static_cast<esphome::InternalGPIOPin *>(this->rx_thresh_pin_);

    uint8_t tx_pin_number = tx_internal_pin->get_pin();
    uint8_t tx_en_pin_number = tx_en_internal_pin->get_pin();
    uint8_t rx_pin_number = rx_internal_pin->get_pin();
    uint8_t rx_thresh_pin_number = rx_thresh_internal_pin != nullptr ? rx_thresh_internal_pin->get_pin() : 0;

    GDOOR::setup(tx_pin_number, tx_en_pin_number, rx_pin_number);

    // Configure RX threshold if conditions are met
    if (rx_pin_number == 22 && this->rx_sens_ != 1.65) {
        GDOOR::setRxThreshold(rx_thresh_pin_number, this->rx_sens_);
    }
}


// Build uppercase hex string from raw frame bytes — O(n), called once per frame
static std::string build_busdata_hex(const GDOOR_DATA *data) {
  static const char HC[] = "0123456789ABCDEF";
  std::string s;
  s.reserve((size_t)data->len * 2);
  for (uint16_t i = 0; i < data->len; i++) {
    s += HC[(data->data[i] >> 4) & 0xF];
    s += HC[ data->data[i]       & 0xF];
  }
  return s;
}

void GdoorComponent::push_bus_data(const std::string &busdata_hex) {
  for (auto *l : bus_listeners_) l->on_bus_message(busdata_hex);
}

void GdoorComponent::loop() {
  GDOOR::loop();
  GDOOR_DATA* rx_data = GDOOR::read();
  if (rx_data != nullptr) {
    GDOOR_DATA_PROTOCOL busmessage = GDOOR_DATA_PROTOCOL(rx_data);
    std::string action = busmessage.action;
    this->set_last_rx_data(rx_data);
    char buffer[256];
    PrintToBuffer ptb(buffer, sizeof(buffer));
    busmessage.printTo(ptb);
    this->last_rx_str_ = "{" + std::string(buffer) + "}";
    this->set_last_bus_update( millis() );
    ESP_LOGD(TAG, "Received data from GDoor bus: %s", buffer);

    // Push busdata_hex to all registered sensors and events (valid frames only)
    if (rx_data->valid) {
      push_bus_data(build_busdata_hex(rx_data));
    }
  }
}

void GdoorComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "GDoor Component:");

  if (this->tx_pin_ != nullptr) {
    auto *internal_pin = static_cast<InternalGPIOPin *>(this->tx_pin_);
    ESP_LOGCONFIG(TAG, "  TX Pin: GPIO %d", internal_pin->get_pin());
  } else {
    ESP_LOGCONFIG(TAG, "  TX Pin: Not set");
  }

  if (this->tx_en_pin_ != nullptr) {
    auto *internal_pin = static_cast<InternalGPIOPin *>(this->tx_en_pin_);
    ESP_LOGCONFIG(TAG, "  TX Enable Pin: GPIO %d", internal_pin->get_pin());
  } else {
    ESP_LOGCONFIG(TAG, "  TX Enable Pin: Not set");
  }

  if (this->rx_pin_ != nullptr) {
    auto *internal_pin = static_cast<InternalGPIOPin *>(this->rx_pin_);
    ESP_LOGCONFIG(TAG, "  RX Pin: GPIO %d", internal_pin->get_pin());
  } else {
    ESP_LOGCONFIG(TAG, "  RX Pin: Not set");
  }

  if (this->rx_thresh_pin_ != nullptr) {
    auto *internal_pin = static_cast<InternalGPIOPin *>(this->rx_thresh_pin_);
    ESP_LOGCONFIG(TAG, "  RX_THRESH Pin: GPIO %d", internal_pin->get_pin());
  } else {
    ESP_LOGCONFIG(TAG, "  RX_THRESH Pin: Not set");
  }

  ESP_LOGCONFIG(TAG, "  RX Sensitivity: %f", this->rx_sens());
}

}  // namespace gdoor_esphome
}  // namespace esphome
