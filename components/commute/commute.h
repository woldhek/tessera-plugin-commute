#pragma once
#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::commute {

// The plugin: a tile for one person ("commute"), a tile for several ("commutes"), the map of a trip ("trip") and an
// item for the top bar ("progress"). What it shows comes from the Home Assistant integration Commute, which follows
// each person; the screen only draws.
class Commute : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // The commute sensor the top bar reads (plugin.yaml): its state and three of its attributes.
  void set_mode(text_sensor::TextSensor *s) { mode_ = s; }
  void set_phase(text_sensor::TextSensor *s) { phase_ = s; }
  void set_progress(text_sensor::TextSensor *s) { progress_ = s; }
  void set_person(text_sensor::TextSensor *s) { person_ = s; }

 protected:
  tessera::BarItem bar() const;
  text_sensor::TextSensor *mode_{}, *phase_{}, *progress_{}, *person_{};
};

}  // namespace esphome::commute
