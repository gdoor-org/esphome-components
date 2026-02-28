#pragma once
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include <string>
#include <vector>
#include "gdoor.h"
#include "gdoor_bus_listener.h"

namespace esphome {
namespace gdoor_esphome {

class GdoorComponent : public Component {
 public:
  // Methods for setting the pins and sensitivity.
  void set_tx_pin(GPIOPin *tx_pin);
  void set_tx_en_pin(GPIOPin *tx_en_pin);
  void set_rx_pin(GPIOPin *rx_pin);
  void set_rx_thresh_pin(GPIOPin *rx_thresh_pin);
  void set_rx_sens(float rx_sens);
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  void setup() override;
  void loop() override;
  void dump_config() override;

  void send_bus_message(const std::string &payload);

  // Push-model registration — called from each sub-component's setup() or Python codegen
  void register_bus_listener(GDoorBusListener *l) { bus_listeners_.push_back(l); }

  // Push busdata_hex to all registered listeners (binary sensors and event entities)
  void push_bus_data(const std::string &busdata_hex);

  void set_last_rx_data(GDOOR_DATA *data);

  GDOOR_DATA* get_last_rx_data() { return this->last_rx_data_; }
  std::string get_last_rx_data_str() const { return this->last_rx_str_; }

  void set_last_bus_update(uint32_t timestamp) { this->last_bus_update_ = timestamp; }
  uint32_t get_last_bus_update() const { return this->last_bus_update_; }

  GPIOPin* tx_pin() const { return tx_pin_; }
  GPIOPin* tx_en_pin() const { return tx_en_pin_; }
  GPIOPin* rx_pin() const { return rx_pin_; }
  GPIOPin* rx_thresh_pin() const { return rx_thresh_pin_; }
  float rx_sens() const { return rx_sens_; };

 protected:
  GPIOPin *tx_pin_{nullptr};
  GPIOPin *tx_en_pin_{nullptr};
  GPIOPin *rx_pin_{nullptr};
  GPIOPin *rx_thresh_pin_{nullptr};
  float rx_sens_{-1};
  GDOOR_DATA* last_rx_data_{nullptr};
  std::string last_rx_str_;
  uint32_t last_bus_update_{0};
  std::vector<GDoorBusListener *> bus_listeners_;
};

class PrintToBuffer : public Print {
 public:
  PrintToBuffer(char *buffer, size_t buffer_size)
      : buffer_(buffer), buffer_size_(buffer_size), index_(0) {
    if (buffer_size_ > 0) {
      buffer_[0] = '\0';
    }
  }
  virtual size_t write(uint8_t c) override {
    if (index_ < buffer_size_ - 1) {
      buffer_[index_++] = c;
      buffer_[index_] = '\0';
      return 1;
    }
    return 0;  // Buffer full.
  }
  size_t size() const { return index_; }
 private:
  char *buffer_;
  size_t buffer_size_;
  size_t index_;
};

}  // namespace gdoor_esphome
}  // namespace esphome
