# gdoor ESPHome Component
An esphome component for the [gdoor](https://gdoor-org.github.io/) GIRA TKS-Bus-Adapter.
Based on @nholloh's [gdoor esphome-component](https://github.com/nholloh/gdoor-esphome) who was the first to make the [gdoor firmware](https://github.com/gdoor-org/gdoor) work in ESPHome
(see more details in : [gdoor issue #25](https://github.com/gdoor-org/gdoor/issues/25)).

Supported Features: read/write to GIRA bus

Tested hardware combination: Gira Wohnungsstation AP (1250 015) + gdoor Adapter 3.1-1 + ESP32 D1 Mini.

[Example YAML](../../example_gdoor.yaml) configuration:
```yaml
esp32:
  board: esp32dev

external_components:
  - source:
      type: git
      url: https://github.com/dtill/esphome-components
    components: [gdoor]
    refresh: 0s

gdoor:
  id: my_gdoor      # optional set your own id here
  tx_pin: 25        # optional (default 25)
  tx_en_pin: 27     # optional (default 27)
  rx_pin: 22        # optional (default 22)
  rx_thresh_pin: 26 # optional (default 26)
  rx_sens: 'med'  # optional if rx_pin is 22: 'low', 'med' or 'high' (default 'high')

text_sensor:      # atm returns gdoor formatted strings like: {"action": "BUTTON_RING", "parameters": "0360", "source": "A286FD", "destination": "000000", "type": "OUTDOOR", "busdata": "011011A286FD0360A04A"}
 -  platform: gdoor
    id: gdoor_bus_message
    icon: "mdi:console-network-outline"
    name: "GDoor Bus Message"
    gdoor_id: my_gdoor

binary_sensor:
  - platform: gdoor
    id: gdoor_outdoor_button_ring
    icon: "mdi:bell-ring-outline"
    name: "GDoor Button Ring"
    gdoor_id: my_gdoor
    busdata:
      - "011011A286FD0360A04A" # example filter a short BUTTON_RING on OUTDOOR station
      - "011011A286FD03A0A08A" # example filter a long BUTTON_RING on OUTDOOR station

  - platform: gdoor
    id: gdoor_indoor_button_light
    icon: "mdi:lightbulb-on"
    name: "GDoor Button Light"
    gdoor_id: my_gdoor
    busdata: "011041A286FD0000A18FA7" # example filter a BUTTON_LIGHT from INDOOR station

output:
  - platform: gdoor
    id: gdoor_outdoor_opener
    name: "GDoor Outdoor Opener"
    gdoor_id: my_gdoor
    # Attention: CRC check will be performed on hex-string during config validation. Only valid payloads are allowed.
    payload: "0200311234560000A165432139"    # example of DOOR_OPEN to open a OUTDOOR .

button:
  - platform: output
    name: Outdoor Opener
    id: my_opener_utton
    icon: "mdi:door-open"
    output: gdoor_outdoor_opener
    duration: 50ms
```