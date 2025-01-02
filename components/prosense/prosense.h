#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace prosense {

static const uint8_t CO100_START_BYTE = 0xFF;
static const uint8_t CO100_GAS_CO = 0x19;
static const uint8_t CO100_UNIT_PPM = 0x02;
static const uint8_t CO100_PACKET_LENGTH = 8;

static const uint8_t DSRF_START_BYTE = 0xFF;
static const uint8_t DSRF_RESERVED_BYTE = 0x05;
static const uint8_t DSRF_TEMP_POSITIVE = 0x00;
static const uint8_t DSRF_PACKET_LENGTH = 14;  // Including checksum

enum ProsenseType {
  PROSENSE_TYPE_DSRF = 0,
  PROSENSE_TYPE_CO100,
};

class ProsenseComponent : public uart::UARTDevice, public Component {
 public:
  ProsenseComponent() = default;
  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void set_type(ProsenseType type) { type_ = type; }
  void set_update_interval(uint32_t val) { update_interval_ = val; };

  void set_co_sensor(sensor::Sensor *co_sensor);
  void set_temperature_sensor(sensor::Sensor *temperature_sensor);
  void set_humidity_sensor(sensor::Sensor *humidity_sensor);
  void set_tvoc_sensor(sensor::Sensor *tvoc_sensor);
  void set_formaldehyde_sensor(sensor::Sensor *formaldehyde_sensor);

 protected:
  optional<bool> check_byte_();
  void parse_data_();
  uint16_t get_16_bit_uint_(uint8_t start_index);
  uint8_t calculate_checksum_(const uint8_t *data, uint8_t length);

  uint8_t data_[64];
  uint8_t data_index_{0};
  uint32_t last_transmission_{0};
  uint32_t update_interval_{0};
  ProsenseType type_;

  sensor::Sensor *co_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *tvoc_sensor_{nullptr};
  sensor::Sensor *formaldehyde_sensor_{nullptr};
};

}  // namespace prosense
}  // namespace esphome