#include "prosense.h"
#include "esphome/core/log.h"

namespace esphome {
namespace prosense {

static const char *const TAG = "prosense";

void ProsenseComponent::set_co_sensor(sensor::Sensor *co_sensor) { co_sensor_ = co_sensor; }
void ProsenseComponent::set_temperature_sensor(sensor::Sensor *temperature_sensor) {
  temperature_sensor_ = temperature_sensor;
}
void ProsenseComponent::set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
void ProsenseComponent::set_tvoc_sensor(sensor::Sensor *tvoc_sensor) { tvoc_sensor_ = tvoc_sensor; }
void ProsenseComponent::set_formaldehyde_sensor(sensor::Sensor *formaldehyde_sensor) {
  formaldehyde_sensor_ = formaldehyde_sensor;
}

void ProsenseComponent::loop() {
  const uint32_t now = millis();

  if (now - this->last_transmission_ >= 500) {
    // last transmission too long ago. Reset RX index.
    this->data_index_ = 0;
  }

  if (this->available() == 0)
    return;

  this->last_transmission_ = now;
  while (this->available() != 0) {
    if (this->data_index_ >= sizeof(this->data_)) {
      // Buffer overflow, reset index
      this->data_index_ = 0;
      return;
    }
    this->read_byte(&this->data_[this->data_index_]);
    auto check = this->check_byte_();
    if (!check.has_value()) {
      // finished
      this->parse_data_();
      this->data_index_ = 0;
      // Clear RX buffer
      while (this->available()) {
        this->read();
      }
    } else if (!*check) {
      // wrong data
      this->data_index_ = 0;
    } else {
      // next byte
      this->data_index_++;
    }
  }
}

float ProsenseComponent::get_setup_priority() const { return setup_priority::DATA; }

optional<bool> ProsenseComponent::check_byte_() {
  uint8_t index = this->data_index_;
  uint8_t byte = this->data_[index];

  if (this->type_ == PROSENSE_TYPE_CO100) {
    if (index == 0)
      return byte == CO100_START_BYTE;

    if (index == 1)
      return byte == CO100_GAS_CO;

    if (index == 2)
      return byte == CO100_UNIT_PPM;

    if (index < CO100_PACKET_LENGTH - 1)
      return true;

    // Verify checksum before completing packet
    if (index == CO100_PACKET_LENGTH - 2) {
      uint8_t checksum = this->data_[index];
      uint8_t calculated_checksum = this->calculate_checksum_(this->data_, index);
      if (checksum != calculated_checksum) {
        ESP_LOGW(TAG, "Checksum mismatch! Expected: 0x%02X, Got: 0x%02X", calculated_checksum, checksum);
        return false;
      }
      return true;
    }

    if (index == CO100_PACKET_LENGTH - 1)
      return {};  // Packet complete
  } else if (this->type_ == PROSENSE_TYPE_DSRF) {
    if (index == 0)
      return byte == DSRF_START_BYTE;

    if (index == 1)
      return byte == DSRF_RESERVED_BYTE;

    if (index < DSRF_PACKET_LENGTH - 1)
      return true;

    // Verify checksum before completing packet
    if (index == DSRF_PACKET_LENGTH - 2) {
      uint8_t checksum = this->data_[index];
      uint8_t calculated_checksum = this->calculate_checksum_(this->data_, index);
      if (checksum != calculated_checksum) {
        ESP_LOGW(TAG, "Checksum mismatch! Expected: 0x%02X, Got: 0x%02X", calculated_checksum, checksum);
        return false;
      }
      return true;
    }

    if (index == DSRF_PACKET_LENGTH - 1)
      // Return std::nullopt to indicate packet is complete and valid.
      // This matches the return type of optional<bool> and signals to the caller
      // that processing should stop and parse_data_() should be called.
      return {};  // Packet complete
  }

  // For unknown type, just return true to continue reading
  return true;
}

uint8_t ProsenseComponent::calculate_checksum_(const uint8_t *data, uint8_t length) {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < length; i++) {
    sum += data[i];
  }
  return (~sum) + 1;
}

void ProsenseComponent::parse_data_() {
  switch (this->type_) {
    case PROSENSE_TYPE_DSRF: {
      if (this->data_index_ != DSRF_PACKET_LENGTH - 1) {
        ESP_LOGW(TAG, "Invalid DSRF packet size: %u", this->data_index_ + 1);
        return;
      }

      // Parse formaldehyde (HCHO)
      uint16_t hcho = this->get_16_bit_uint_(2);
      // Parse VOC and TVOC
      uint16_t voc = this->get_16_bit_uint_(4);
      uint16_t tvoc = this->get_16_bit_uint_(6);

      // Parse temperature with sign and decimal part
      int8_t temp_sign = (this->data_[8] == DSRF_TEMP_POSITIVE) ? 1 : -1;
      float temperature = temp_sign * (this->data_[9] + (this->data_[10] / 100.0f));

      // Parse humidity with decimal part
      float humidity = this->data_[11] + (this->data_[12] / 100.0f);

      ESP_LOGD(TAG, "Got DSRF - HCHO: %u ppb, VOC: %u ppb, TVOC: %u ppb, Temperature: %.2f°C, Humidity: %.2f%%", hcho,
               voc, tvoc, temperature, humidity);

      if (this->formaldehyde_sensor_ != nullptr)
        this->formaldehyde_sensor_->publish_state(hcho);
      if (this->tvoc_sensor_ != nullptr)
        this->tvoc_sensor_->publish_state(tvoc);
      if (this->temperature_sensor_ != nullptr)
        this->temperature_sensor_->publish_state(temperature);
      if (this->humidity_sensor_ != nullptr)
        this->humidity_sensor_->publish_state(humidity);

      break;
    }
    case PROSENSE_TYPE_CO100: {
      if (this->data_index_ != CO100_PACKET_LENGTH - 1) {
        ESP_LOGW(TAG, "Invalid CO100 packet size: %u", this->data_index_ + 1);
        return;
      }

      uint8_t digits_after_decimal = this->data_[3];
      uint16_t raw_value = this->get_16_bit_uint_(4);
      uint16_t full_scale = this->get_16_bit_uint_(6);

      // Convert raw value to float with proper decimal places
      // For example: if raw_value=1234 and digits_after_decimal=2, result should be 12.34
      float co_value = raw_value;
      if (digits_after_decimal > 0) {
        co_value = co_value / powf(10.0f, digits_after_decimal);
      }

      ESP_LOGD(TAG, "Got CO: %.3f ppm (full scale: %u, raw: %u, decimal places: %u)", co_value, full_scale, raw_value,
               digits_after_decimal);

      if (this->co_sensor_ != nullptr) {
        this->co_sensor_->publish_state(co_value);
      }
      break;
    }
  }

  this->status_clear_warning();
}

uint16_t ProsenseComponent::get_16_bit_uint_(uint8_t start_index) {
  return (uint16_t(this->data_[start_index]) << 8) | uint16_t(this->data_[start_index + 1]);
}

void ProsenseComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Prosense:");
  switch (this->type_) {
    case PROSENSE_TYPE_DSRF:
      ESP_LOGCONFIG(TAG, "  Type: DS-RF");
      break;
    case PROSENSE_TYPE_CO100:
      ESP_LOGCONFIG(TAG, "  Type: CO-100");
      break;
    default:
      ESP_LOGCONFIG(TAG, "  Type: Unknown");
      break;
  }
  LOG_SENSOR("  ", "CO", this->co_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "TVOC", this->tvoc_sensor_);
  LOG_SENSOR("  ", "Formaldehyde", this->formaldehyde_sensor_);
  this->check_uart_settings(9600);
}

}  // namespace prosense
}  // namespace esphome