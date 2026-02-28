#pragma once
#include <string>

namespace esphome {
namespace gdoor_esphome {

/// Common interface for components that receive Gira bus frame notifications.
/// Implemented by GDoorActionSensor (binary_sensor) and GDoorBusEvent (event).
class GDoorBusListener {
 public:
  virtual void on_bus_message(const std::string &busdata_hex) = 0;
  virtual ~GDoorBusListener() = default;
};

/// Interface for event entities that can be triggered from the TX (output) side.
/// Implemented by GDoorBusEvent (event). Used by GDoorBusWrite (output) to fire
/// a linked event when a payload is sent, without a direct dependency on the event header.
class GDoorTxTarget {
 public:
  virtual void handle_tx(const std::string &event_type) = 0;
  virtual ~GDoorTxTarget() = default;
};

}  // namespace gdoor_esphome
}  // namespace esphome
