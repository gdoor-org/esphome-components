#pragma once

#include "esphome/core/component.h"
#include "esphome/components/event/event.h"
#include "../gdoor_component.h"
#include "../gdoor_bus_listener.h"
#include <string>
#include <vector>
#include <utility>   // std::pair

namespace esphome {
namespace gdoor_esphome {

class GDoorBusEvent : public event::Event, public Component, public GDoorBusListener, public GDoorTxTarget {
 public:
  void set_parent(GdoorComponent *parent) { this->parent_ = parent; }

  // Called once per configured busdata entry from Python-generated setup code
  void add_busdata(const std::string &hex, const std::string &event_type) {
    busdata_.emplace_back(hex, event_type);
  }

  // Called by GdoorComponent::push_bus_data() for every valid received frame
  void on_bus_message(const std::string &busdata_hex) {
    for (const auto &entry : busdata_) {
      if (entry.first == busdata_hex) {
        this->trigger(entry.second);
        return;   // first match wins
      }
    }
  }

  // Called by GDoorBusWrite::write_state() when a TX-linked output fires
  void handle_tx(const std::string &event_type) { this->trigger(event_type); }

  void setup() override;
  void dump_config() override;

 protected:
  GdoorComponent *parent_{nullptr};
  // Flat vector of (busdata_hex, event_type) pairs — small N, linear scan is fast
  std::vector<std::pair<std::string, std::string>> busdata_;
};

}  // namespace gdoor_esphome
}  // namespace esphome
