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

# Enable Home Assistant API
api:
  reboot_timeout: 0s
encryption:
    key: !secret my_own_secret_api_key # api-key generator found here: https://esphome.io/components/api.html#configuration-variables

gdoor:
  id: my_gdoor      # optional set your own id here
  tx_pin: 25        # optional (default 25)
  tx_en_pin: 27     # optional (default 27)
  rx_pin: 22        # optional (default 22)
  rx_thresh_pin: 26 # optional (default 26)
  rx_sens: 'med'    # optional if rx_pin is 22: 'low', 'med' or 'high' (default 'high')

text_sensor:        # atm returns gdoor formatted strings like: {"action": "BUTTON_RING", "parameters": "0360", "source": "A286FD", "destination": "000000", "type": "OUTDOOR", "busdata": "011011A286FD0360A04A"}
 -  platform: gdoor
    id: gdoor_bus_message
    icon: "mdi:console-network-outline"
    name: "GDoor Bus Message"
    gdoor_id: my_gdoor
    on_value:
      then:
        - light.turn_on: blue_status_light
        - delay: 500ms
        - light.turn_off: blue_status_light

binary_sensor:
  - platform: gdoor
    id: gdoor_outdoor_button_ring
    icon: "mdi:bell-ring-outline"
    name: "GDoor Button Ring"
    gdoor_id: my_gdoor
    busdata:
      - "011011A286FD0360A04A"                # example filter a short BUTTON_RING on OUTDOOR station
      - "011011A286FD03A0A08A"                # example filter a long BUTTON_RING on OUTDOOR station

  - platform: gdoor
    id: gdoor_indoor_button_light
    icon: "mdi:lightbulb-on"
    name: "GDoor Button Light"
    gdoor_id: my_gdoor
    busdata: "011041A286FD0000A18FA7"         # example filter a BUTTON_LIGHT from INDOOR station

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

light:
  - platform: status_led
    name: "Blue Status LED"
    pin:
      number: GPIO2
      ignore_strapping_warning: true        # https://github.com/esphome/feature-requests/issues/2168
    id: blue_status_light
    internal: true
```

## Event Entities

In addition to `binary_sensor`, this component supports the ESPHome [`event`](https://esphome.io/components/event/index.html) platform. Events are stateless triggers that appear in Home Assistant as **event entities**. Unlike a binary sensor (which has an ON/OFF state that resets after 500 ms), an event entity fires once and carries an `event_type` string that automations can use to distinguish between different bus messages.

### Why use events instead of (or alongside) binary_sensor?

| Feature | `binary_sensor` | `event` |
|---|---|---|
| HA state (on/off) | Yes — auto-resets after 500 ms | No — stateless trigger |
| `event_type` field | No | Yes — distinguish short/long ring, etc. |
| HA Blueprints / doorbell intent | Limited | Full support (`device_class: doorbell`) |
| Mobile push notifications | Manual | Native doorbell notification in HA app |
| Voice assistant integration | No | Yes (doorbell device class) |

**Recommendation:** use both — a `binary_sensor` for simple `on_value` automations and an `event` entity for HA dashboards, blueprints, and mobile notifications.

### `device_class`

- `doorbell` — use for door-ring button events. Enables native HA doorbell behaviour (push notifications, blueprints, voice assistants).
- `button` — use for all other bus-triggered button events (indoor light button, opener confirmation, etc.).

### YAML Configuration

The `busdata` key is a **dict** where each key is an `event_type` name (you choose the name freely), and the value is a list of one or more validated hex frame strings. When a matching frame arrives on the bus, the event fires with the corresponding `event_type`.

```yaml
event:
  # Doorbell ring event — distinguishes short and long ring
  - platform: gdoor
    id: gdoor_ring_event
    name: "GDoor Ring"
    device_class: doorbell
    gdoor_id: my_gdoor
    busdata:
      ring_short:
        - "011011A286FD0360A04A"   # short BUTTON_RING on OUTDOOR station
      ring_long:
        - "011011A286FD03A0A08A"   # long BUTTON_RING on OUTDOOR station

  # Indoor light button event
  - platform: gdoor
    id: gdoor_light_event
    name: "GDoor Light Button"
    device_class: button
    gdoor_id: my_gdoor
    busdata:
      press:
        - "011041A286FD0000A18FA7" # BUTTON_LIGHT from INDOOR station

  # TX-linked event: fires when the output sends the DOOR_OPEN payload.
  # No busdata needed — RX is disabled while transmitting, so we fire from TX side.
  - platform: gdoor
    id: gdoor_opener_event
    name: "GDoor Opener"
    device_class: button
    gdoor_id: my_gdoor
    event_types:
      - press                       # explicit list required when no busdata is provided

output:
  - platform: gdoor
    id: gdoor_outdoor_opener
    name: "GDoor Outdoor Opener"
    gdoor_id: my_gdoor
    payload: "0200311234560000A165432139"   # example DOOR_OPEN to OUTDOOR station
    tx_event_id: gdoor_opener_event         # optional: fire this event when output triggers
    tx_event_type: press                    # optional: event_type to fire (default: "press")
```

### Using events in ESPHome automations

```yaml
event:
  - platform: gdoor
    id: gdoor_ring_event
    name: "GDoor Ring"
    device_class: doorbell
    gdoor_id: my_gdoor
    busdata:
      ring_short:
        - "011011A286FD0360A04A"
      ring_long:
        - "011011A286FD03A0A08A"
    on_event:
      then:
        - if:
            condition:
              lambda: 'return x == "ring_short";'
            then:
              - light.turn_on: blue_status_light
              - delay: 500ms
              - light.turn_off: blue_status_light
```

In Home Assistant, the event entity appears under **Settings → Devices & Services** and can be used as a trigger in automations:

```yaml
# Home Assistant automation trigger
trigger:
  - platform: state
    entity_id: event.gdoor_ring
    attribute: event_type
    to: ring_short
```