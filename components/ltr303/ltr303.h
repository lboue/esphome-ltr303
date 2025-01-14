#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/optional.h"

namespace esphome {
namespace ltr303 {

// https://www.mouser.com/datasheet/2/239/Lite-On_LTR-303ALS-01_DS_ver%201.1-1175269.pdf

enum class CommandRegisters : uint8_t {
  ALS_CTRL = 0x80,   // ALS operation mode control SW reset
  MEAS_RATE = 0x85,  // ALS measurement rate in active mode
  PART_ID = 0x86,    // Part Number ID and Revision ID
  MANU_ID = 0x87,    // Manufacturer ID
  CH1_0 = 0x88,      // ALS measurement CH1 data, lower byte - infrared only
  CH1_1 = 0x89,      // ALS measurement CH1 data, upper byte - infrared only
  CH0_0 = 0x8A,      // ALS measurement CH0 data, lower byte - visible + infrared
  CH0_1 = 0x8B,      // ALS measurement CH0 data, upper byte - visible + infrared
  ALS_STATUS = 0x8c  // ALS new data status
};

// Sensor gain levels
enum Gain : uint8_t {
  GAIN_1 = 0,  // default
  GAIN_2 = 1,
  GAIN_4 = 2,
  GAIN_8 = 3,
  GAIN_48 = 6,
  GAIN_96 = 7,
};
static const uint8_t GAINS_COUNT = 6;

enum IntegrationTime : uint8_t {
  INTEGRATION_TIME_100MS = 0,  // default
  INTEGRATION_TIME_50MS = 1,
  INTEGRATION_TIME_200MS = 2,
  INTEGRATION_TIME_400MS = 3,
  INTEGRATION_TIME_150MS = 4,
  INTEGRATION_TIME_250MS = 5,
  INTEGRATION_TIME_300MS = 6,
  INTEGRATION_TIME_350MS = 7
};
static const uint8_t TIMES_COUNT = 8;

enum MeasurementRepeatRate {
  REPEAT_RATE_50MS = 0,
  REPEAT_RATE_100MS = 1,
  REPEAT_RATE_200MS = 2,
  REPEAT_RATE_500MS = 3,  // default
  REPEAT_RATE_1000MS = 4,
  REPEAT_RATE_2000MS = 5
};

//
// ALS_CONTR Register (0x80)
//
union ControlRegister {
  uint8_t raw;
  struct {
    bool active_mode : 1;
    bool sw_reset : 1;
    Gain gain : 3;
    uint8_t reserved : 3;
  } __attribute__((packed));
};

//
// ALS_MEAS_RATE Register (0x85)
//
union MeasurementRateRegister {
  uint8_t raw;
  struct {
    MeasurementRepeatRate measurement_repeat_rate : 3;
    IntegrationTime integration_time : 3;
    bool reserved_6 : 1;
    bool reserved_7 : 1;
  } __attribute__((packed));
};

//
// ALS_STATUS Register (0x8C) (Read Only)
//
union StatusRegister {
  uint8_t raw;
  struct {
    bool reserved_0 : 1;
    bool reserved_1 : 1;
    bool new_data : 1;
    bool reserved_3 : 1;
    Gain gain : 3;
    bool data_invalid : 1;
  } __attribute__((packed));
};

enum DataAvail : uint8_t { NO_DATA, BAD_DATA, DATA_OK };

class LTR303Component : public PollingComponent, public i2c::I2CDevice {
 public:
  //
  // EspHome framework functions
  //
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  //
  // Configuration setters
  //
  void set_gain(Gain gain) { this->gain_ = gain; }
  void set_integration_time(IntegrationTime time) { this->integration_time_ = time; }
  void set_repeat_rate(MeasurementRepeatRate rate) { this->repeat_rate_ = rate; }
  void set_glass_attenuation_factor(float factor) { this->glass_attenuation_factor_ = factor; }
  void set_enable_automatic_mode(bool enable) { this->automatic_mode_enabled_ = enable; }

  void set_ambient_light_sensor(sensor::Sensor *sensor) { this->ambient_light_sensor_ = sensor; }
  void set_full_spectrum_counts_sensor(sensor::Sensor *sensor) { this->full_spectrum_counts_sensor_ = sensor; }
  void set_infrared_counts_sensor(sensor::Sensor *sensor) { this->infrared_counts_sensor_ = sensor; }
  void set_actual_gain_sensor(sensor::Sensor *sensor) { this->actual_gain_sensor_ = sensor; }
  void set_actual_integration_time_sensor(sensor::Sensor *sensor) { this->actual_integration_time_sensor_ = sensor; }

 protected:
  //
  // Internal state machine, used to split all the actions into
  // small steps in loop() to make sure we are not blocking execution
  //
  enum class State : uint8_t {
    NOT_INITIALIZED,
    DELAYED_SETUP,
    IDLE,
    WAITING_FOR_DATA,
    COLLECTING_DATA_AUTO,
    DATA_COLLECTED,
    ADJUSTMENT_IN_PROGRESS,
    READY_TO_PUBLISH,
    KEEP_PUBLISHING
  } state_{State::NOT_INITIALIZED};

  //
  // Current measurements data
  //
  struct Readings {
    uint16_t ch0{0};
    uint16_t ch1{0};
    Gain actual_gain{Gain::GAIN_1};
    IntegrationTime integration_time{IntegrationTime::INTEGRATION_TIME_100MS};
    float lux{0.0f};
  } readings_;

  //
  // Device interaction and data manipulation
  //
  void configure_reset_and_activate_();
  void configure_integration_time_(IntegrationTime time);
  void configure_gain_(Gain gain);
  DataAvail is_data_ready_(Readings &data);
  void read_sensor_data_(Readings &data);
  bool are_adjustments_required_(Readings &data);
  void apply_lux_calculation_(Readings &data);
  void publish_data_part_1_(Readings &data);
  void publish_data_part_2_(Readings &data);

  //
  // Component configuration
  //
  bool automatic_mode_enabled_{true};
  Gain gain_{Gain::GAIN_1};
  IntegrationTime integration_time_{IntegrationTime::INTEGRATION_TIME_100MS};
  MeasurementRepeatRate repeat_rate_{MeasurementRepeatRate::REPEAT_RATE_500MS};
  float glass_attenuation_factor_{1.0};

  //
  //   Sensors for publishing data
  //
  sensor::Sensor *infrared_counts_sensor_{nullptr};          // direct reading CH1, infrared only
  sensor::Sensor *full_spectrum_counts_sensor_{nullptr};     // direct reading CH0, infrared + visible light
  sensor::Sensor *ambient_light_sensor_{nullptr};            // calculated lux
  sensor::Sensor *actual_gain_sensor_{nullptr};              // actual gain of reading
  sensor::Sensor *actual_integration_time_sensor_{nullptr};  // actual integration time
};

}  // namespace ltr303
}  // namespace esphome
