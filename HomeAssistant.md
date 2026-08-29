# Home Assistant Integration Guide

DigiFrame integrates beautifully with Home Assistant (HA) via MQTT and REST API. This guide shows you how to automate calendar events and control the newly added Night Mode.

## 1. Calendar Automation
DigiFrame can display upcoming calendar events on the bottom of the clock face. To avoid memory issues, Home Assistant fetches the calendar and pushes only the necessary text to the frame.

### `configuration.yaml`
Add this to your `configuration.yaml` file to tell HA how to send the data:
```yaml
rest_command:
  update_digiframe_calendar:
    url: "http://digiframe.local/api/calendar" # Replace with the frame's IP if DNS fails
    method: POST
    headers:
      content-type: "application/json"
    payload: >
      [
        {% for event in events %}
          {
            "d": "{{ as_timestamp(event.start) | timestamp_custom('%Y-%m-%d') }}",
            "e": "{{ as_timestamp(event.end) | timestamp_custom('%Y-%m-%d') }}",
            "t": "{{ as_timestamp(event.start) | timestamp_custom('%H:%M') }}",
            "m": "{{ event.summary }}"
          }
          {% if not loop.last %},{% endif %}
        {% endfor %}
      ]
```

### Automations
Create a new automation in HA, edit in **YAML mode**, and paste this snippet. You can list as many calendar entities as you want under `entity_id`.

```yaml
alias: "Update DigiFrame Calendar"
description: "Pushes upcoming calendar events to DigiFrame every hour"
trigger:
  - platform: time_pattern
    hours: "/1"   # Runs every hour
action:
  - service: calendar.get_events
    target:
      entity_id:
        - calendar.personal     # <-- CHANGE THIS
        - calendar.work         # <-- CHANGE THIS
        - calendar.family       # <-- ADD MORE IF NEEDED
    data:
      duration:
        days: 3   # Fetch events for the next 3 days
    response_variable: agenda
  - service: rest_command.update_digiframe_calendar
    data:
      # This template merges the events from all queried calendars into one list
      events: >
        {% set ns = namespace(all_events=[]) %}
        {% for cal in agenda.values() %}
          {% set ns.all_events = ns.all_events + cal.events %}
        {% endfor %}
        {{ ns.all_events | tojson }}
```

## 2. Night Mode Control
DigiFrame has a built-in schedule for Night Mode (configurable in its web dashboard).
However, you can completely override the schedule using Home Assistant!

If you enabled MQTT in the DigiFrame web dashboard, the frame will automatically announce a **"Night Mode"** `select` entity to HA via MQTT Discovery.

You can use this entity in your dashboards or automations to force Night Mode on or off based on your room's occupancy or smart light status.

**Example Automation:** Turn on Night Mode when the bedroom lights turn off:
```yaml
alias: "DigiFrame: Sync Night Mode with Lights"
trigger:
  - platform: state
    entity_id: light.bedroom_lights
    to: "off"
action:
  - service: select.select_option
    target:
      entity_id: select.digiframe_nightmode
    data:
      option: "On"
```

## 3. Play GIFs via MQTT
DigiFrame also exposes a **"Play GIF"** `select` entity to Home Assistant via MQTT Discovery.

You can use this to trigger animations from your Home Assistant dashboards or automations by selecting the filename of the GIF (e.g. `cake.gif`). The GIF must already be uploaded to the frame's memory via the web dashboard. The options in this dropdown are automatically updated whenever you add or delete a GIF! Selecting `None` will stop the GIF and return to the clock.

**Example Automation:** Play a GIF when someone arrives home:
```yaml
alias: "DigiFrame: Play Welcome GIF"
trigger:
  - platform: state
    entity_id: person.john
    to: "home"
action:
  - service: select.select_option
    target:
      entity_id: select.digiframe_play_gif
    data:
      option: "welcome.gif"
```

## 4. Overlay Text on a GIF
It is absolutely possible to merge a scrolling text message and a GIF simultaneously! If you trigger a GIF and a message at the same time, DigiFrame will automatically overlay the text on top of the GIF with a solid black backing so it remains perfectly legible.

Here is how you can welcome someone home with an animation and a message for 5 minutes, then return to the clock:

**Example Automation:** Welcome Home Overlay (Handles multiple people)
```yaml
alias: "DigiFrame: Welcome Home (GIF + Text)"
triggers:
  - entity_id:
      - person.john
      - person.jane
    to:
      - home
    trigger: state
    from: null
actions:
  - target:
      entity_id: select.digiframe_play_gif
    data:
      option: welcome.gif
    action: select.select_option
  - target:
      entity_id: text.digiframe_message
    data:
      value: >
        Hallo!\n{{ expand('person.john', 'person.jane') |
        selectattr('state', 'eq', 'home') | map(attribute='name') | join(' & ')
        }}
    action: text.set_value
  - delay: "00:03:00"
  - target:
      entity_id: button.digiframe_back_to_clock
    action: button.press
mode: restart
```

## 5. Live Location Tracking Map
You can overlay live travel stats (ETA, distance, street) on top of an automatically downloaded JPEG map by publishing a JSON payload to the clock's MQTT tracker topic. This is perfect for "Where is mustermann?" automations!

The clock listens to the `digiframe/<your-clock-id>/tracker/set` MQTT topic.
You can find your clock's exact MQTT ID by looking at your MQTT broker or HA devices list.

> **Important**: The `map_url` provided by Home Assistant **must** be a **JPEG** image (not a PNG), as the firmware uses a hardware-accelerated JPEG decoder to keep memory usage low.

**Example Automation:** Send tracking data to the clock
```yaml
alias: "DigiFrame: Show mustermann's Location"
trigger:
  - platform: state
    entity_id: input_boolean.trigger_tracking # E.g., triggered via an Alexa Intent
    to: "on"
action:
  - service: mqtt.publish
    data:
      topic: digiframe/digiframe_a1b2/tracker/set
      payload: >
        {
          "person": "mustermann",
          "dist": "{{ states('sensor.mustermann_distance_from_home') }} km",
          "eta": "{{ states('sensor.mustermann_time_to_home') }} min",
          "street": "{{ states('sensor.mustermann_current_street') }}",
          "map_url": "https://api.mapbox.com/styles/v1/mapbox/streets-v11/static/{{ state_attr('device_tracker.mustermann', 'longitude') }},{{ state_attr('device_tracker.mustermann', 'latitude') }},14/64x64?access_token=YOUR_TOKEN&format=jpg"
        }
```

Whenever you want the clock to return to the normal clock face, Home Assistant can simply publish an empty payload to the `digiframe/<your-clock-id>/stop/set` topic.
