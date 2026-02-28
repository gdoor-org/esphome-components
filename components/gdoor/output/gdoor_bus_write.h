#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/binary_output.h"
#include "../gdoor_component.h"
#include "../gdoor_bus_listener.h"

namespace esphome {
namespace gdoor_esphome {

class GDoorBusWrite : public output::BinaryOutput, public Component {
 public:
  void dump_config() override;
  void loop() override {};
  void write_state(bool state) override;

  void set_parent(GdoorComponent *parent) { this->parent_ = parent; }
  void set_payload(const std::string &payload) { this->payload_ = payload; }
  void set_require_response(bool require_response) { this->require_response_ = require_response; }
  void set_tx_event(GDoorTxTarget *event) { this->tx_event_ = event; }
  void set_tx_event_type(const std::string &event_type) { this->tx_event_type_ = event_type; }

 protected:
  GdoorComponent *parent_{nullptr};
  std::string payload_;
  bool require_response_{false};
  GDoorTxTarget *tx_event_{nullptr};
  std::string tx_event_type_;
};

}  // namespace gdoor_esphome
}  // namespace esphome