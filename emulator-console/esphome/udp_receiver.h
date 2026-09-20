#pragma once

#include "esphome.h"
#include "resistor_ladder.h"
#include "custom_mcpwm.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cinttypes>
#include <cstring>
#include <cmath>

namespace esphome {
namespace vehicle_emulator {

static const char *TAG_UDP = "VEHICLE_UDP";

// Binary packet structure (14 bytes total, little-endian)
// [0xAA, 0x55] [telltale_mask (2B)] [speed_freq_x10 (2B)] [rpm_freq_x10 (2B)] [coolant_duty_x100 (2B)] [fuel_ohm_x10 (2B)] [checksum (2B)]
#pragma pack(push, 1)
struct VehicleUdpPacket {
    uint8_t magic[2];          // 0xAA, 0x55
    uint16_t telltales_mask;   // Bitmask for Expander 0 (0x20)
    uint16_t speed_freq_x10;   // Speed frequency in 0.1 Hz (e.g. 1000 = 100.0 Hz)
    uint16_t rpm_freq_x10;     // RPM frequency in 0.1 Hz (e.g. 2000 = 200.0 Hz)
    uint16_t coolant_duty_x100;// Coolant duty cycle in 0.01% (e.g. 3570 = 35.70%)
    uint16_t fuel_ohm_x10;     // Target fuel resistance in 0.1 Ohm (e.g. 1200 = 120.0 Ohm)
    uint16_t checksum;         // Sum of first 12 bytes
};
#pragma pack(pop)

class UdpReceiver : public Component {
public:
    UdpReceiver(i2c::I2CBus *i2c_bus, uint8_t exp0_addr, uint8_t exp1_addr, uint16_t port = 8888)
        : i2c_bus_(i2c_bus), exp0_addr_(exp0_addr), exp1_addr_(exp1_addr), port_(port) {}

    void set_telemetry_sensors(
        sensor::Sensor *speed_s,
        sensor::Sensor *rpm_s,
        sensor::Sensor *coolant_s,
        sensor::Sensor *fuel_s,
        text_sensor::TextSensor *exp0_s,
        text_sensor::TextSensor *exp1_s) {
        speed_sensor_ = speed_s;
        rpm_sensor_ = rpm_s;
        coolant_sensor_ = coolant_s;
        fuel_sensor_ = fuel_s;
        exp0_sensor_ = exp0_s;
        exp1_sensor_ = exp1_s;
    }

    void setup() override {
        ESP_LOGI(TAG_UDP, "UDP Receiver component registered for port %u (waiting for network)", (unsigned int)port_);
    }

    void on_wifi_connect() {
        network_ready_ = true;
        if (sock_ < 0) {
            ESP_LOGI(TAG_UDP, "Wi-Fi connected, initializing UDP socket...");
            init_socket();
        }
    }

    void on_wifi_disconnect() {
        network_ready_ = false;
        if (sock_ >= 0) {
            ESP_LOGI(TAG_UDP, "Wi-Fi disconnected, closing UDP socket...");
            close(sock_);
            sock_ = -1;
        }
    }

    void loop() override {
        if (!network_ready_) {
            return;
        }

        if (sock_ < 0) {
            static uint32_t last_retry = 0;
            uint32_t now = millis();
            if (now - last_retry > 2000) {
                last_retry = now;
                init_socket();
            }
            return;
        }

        // Drain pending UDP datagrams
        uint8_t rx_buffer[256];
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);

        while (true) {
            int len = recvfrom(sock_, rx_buffer, sizeof(rx_buffer) - 1, 0,
                               (struct sockaddr *)&source_addr, &socklen);
            if (len < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    break;
                }
                ESP_LOGW(TAG_UDP, "recvfrom failed with errno: %d", errno);
                close(sock_);
                sock_ = -1;
                break;
            }

            if (len > 0) {
                process_packet(rx_buffer, len);
            }
        }
    }

    void init_socket() {
        if (sock_ >= 0) {
            return;
        }

        sock_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock_ < 0) {
            ESP_LOGW(TAG_UDP, "Unable to create socket: errno %d", errno);
            return;
        }

        int flags = fcntl(sock_, F_GETFL, 0);
        if (flags != -1) {
            fcntl(sock_, F_SETFL, flags | O_NONBLOCK);
        }

        int opt = 1;
        setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port_);

        int err = bind(sock_, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGW(TAG_UDP, "Socket unable to bind to port %u: errno %d", (unsigned int)port_, errno);
            close(sock_);
            sock_ = -1;
            return;
        }

        ESP_LOGI(TAG_UDP, "Socket bound successfully to port %u (non-blocking)", (unsigned int)port_);
    }

protected:
    void process_packet(const uint8_t *buf, int len) {
        if (len == sizeof(VehicleUdpPacket) && buf[0] == 0xAA && buf[1] == 0x55) {
            const VehicleUdpPacket *pkt = reinterpret_cast<const VehicleUdpPacket *>(buf);
            
            uint16_t calc_sum = 0;
            const uint8_t *p = buf;
            for (size_t i = 0; i < sizeof(VehicleUdpPacket) - 2; i++) {
                calc_sum += p[i];
            }
            if (calc_sum != pkt->checksum) {
                ESP_LOGW(TAG_UDP, "Checksum mismatch: calc=0x%04X, pkt=0x%04X", calc_sum, pkt->checksum);
                return;
            }

            apply_controls(
                pkt->telltales_mask,
                static_cast<float>(pkt->speed_freq_x10) / 10.0f,
                static_cast<float>(pkt->rpm_freq_x10) / 10.0f,
                static_cast<float>(pkt->coolant_duty_x100) / 100.0f,
                static_cast<float>(pkt->fuel_ohm_x10) / 10.0f
            );
            return;
        }

        parse_text_packet(reinterpret_cast<const char *>(buf), len);
    }

    void parse_text_packet(const char *text, int len) {
        char buf[256];
        if (len >= (int)sizeof(buf)) len = sizeof(buf) - 1;
        memcpy(buf, text, len);
        buf[len] = 0;

        uint16_t mask = last_telltales_mask_;
        float speed = last_speed_freq_ < 0.0f ? 0.0f : last_speed_freq_;
        float rpm = last_rpm_freq_ < 0.0f ? 0.0f : last_rpm_freq_;
        float coolant = last_coolant_duty_ < 0.0f ? 0.0f : last_coolant_duty_;
        float fuel = last_fuel_ohm_ < 0.0f ? 120.0f : last_fuel_ohm_;

        char *p_t = strstr(buf, "\"telltales\":");
        if (!p_t) p_t = strstr(buf, "telltales=");
        if (p_t) {
            p_t = strchr(p_t, p_t[9] == ':' ? ':' : '=');
            if (p_t) {
                p_t++;
                while (*p_t == ' ') p_t++;
                if (p_t[0] == '0' && (p_t[1] == 'x' || p_t[1] == 'X')) {
                    mask = strtoul(p_t, nullptr, 16);
                } else {
                    mask = strtoul(p_t, nullptr, 10);
                }
            }
        }

        char *p_s = strstr(buf, "\"speed_freq\":");
        if (!p_s) p_s = strstr(buf, "speed_freq=");
        if (p_s) {
            p_s = strchr(p_s, p_s[10] == ':' ? ':' : '=');
            if (p_s) speed = strtof(p_s + 1, nullptr);
        }

        char *p_r = strstr(buf, "\"rpm_freq\":");
        if (!p_r) p_r = strstr(buf, "rpm_freq=");
        if (p_r) {
            p_r = strchr(p_r, p_r[8] == ':' ? ':' : '=');
            if (p_r) rpm = strtof(p_r + 1, nullptr);
        }

        char *p_c = strstr(buf, "\"coolant_duty\":");
        if (!p_c) p_c = strstr(buf, "coolant_duty=");
        if (p_c) {
            p_c = strchr(p_c, p_c[12] == ':' ? ':' : '=');
            if (p_c) coolant = strtof(p_c + 1, nullptr);
        }

        char *p_f = strstr(buf, "\"fuel_ohm\":");
        if (!p_f) p_f = strstr(buf, "fuel_ohm=");
        if (p_f) {
            p_f = strchr(p_f, p_f[8] == ':' ? ':' : '=');
            if (p_f) fuel = strtof(p_f + 1, nullptr);
        }

        apply_controls(mask, speed, rpm, coolant, fuel);
    }

    void apply_controls(uint16_t telltales_mask, float speed_freq, float rpm_freq, float coolant_duty, float fuel_ohm) {
        // Bit 15 (0x8000) controls Ignition on TCA9555 expander 0.
        // Board power depends on Ignition being active unless supplied by secondary 3V3.
        if ((telltales_mask & 0x8000) == 0) {
            ESP_LOGW(TAG_UDP, "Warning: Inbound mask 0x%04X has Ignition (bit 15) OFF - board power may shut down!", telltales_mask);
        }

        // Apply telltales
        if (telltales_mask != last_telltales_mask_ || !has_received_packet_) {
            write_resistor_mask(i2c_bus_, exp0_addr_, telltales_mask);
            last_telltales_mask_ = telltales_mask;
        }

        // Apply Speed MCPWM
        if (std::abs(speed_freq - last_speed_freq_) > 0.05f || !has_received_packet_) {
            McpwmManager::set_frequency(MCPWM_CHAN_SPEED, speed_freq);
            last_speed_freq_ = speed_freq;
        }

        // Apply RPM MCPWM
        if (std::abs(rpm_freq - last_rpm_freq_) > 0.05f || !has_received_packet_) {
            McpwmManager::set_frequency(MCPWM_CHAN_RPM, rpm_freq);
            last_rpm_freq_ = rpm_freq;
        }

        // Apply Coolant MCPWM
        if (std::abs(coolant_duty - last_coolant_duty_) > 0.05f || !has_received_packet_) {
            McpwmManager::set_duty(MCPWM_CHAN_COOLANT, coolant_duty);
            last_coolant_duty_ = coolant_duty;
        }

        // Apply Fuel resistance step
        int step = find_closest_fuel_step(fuel_ohm);
        if (std::abs(fuel_ohm - last_fuel_ohm_) > 0.5f || !has_received_packet_) {
            if (step != last_fuel_step_ || !has_received_packet_) {
                set_fuel_step(i2c_bus_, exp1_addr_, step);
                last_fuel_step_ = step;
            }
            last_fuel_ohm_ = fuel_ohm;
        }

        has_received_packet_ = true;
        packets_received_++;

        uint32_t now = millis();
        // Log first packet immediately, then rate-limit to once every 2 seconds
        if (packets_received_ == 1 || now - last_log_time_ > 2000) {
            ESP_LOGI(TAG_UDP, "UDP Rx #%" PRIu32 ": Tell=0x%04X Spd=%.1fHz Rpm=%.1fHz Cool=%.1f%% Fuel=%.1fOhm (Step %d)",
                     packets_received_, last_telltales_mask_, last_speed_freq_, last_rpm_freq_,
                     last_coolant_duty_, last_fuel_ohm_, last_fuel_step_);
            last_log_time_ = now;
        }

        // Rate-limited telemetry sync to Home Assistant / Web UI sensors (every 500 ms)
        if (now - last_telemetry_publish_ > 500) {
            last_telemetry_publish_ = now;
            if (speed_sensor_) speed_sensor_->publish_state(last_speed_freq_);
            if (rpm_sensor_) rpm_sensor_->publish_state(last_rpm_freq_);
            if (coolant_sensor_) coolant_sensor_->publish_state(last_coolant_duty_);
            if (fuel_sensor_) {
                float actual_r = (last_fuel_step_ >= 1 && last_fuel_step_ <= 19)
                                     ? FUEL_RES_VALUES[last_fuel_step_ - 1]
                                     : last_fuel_ohm_;
                fuel_sensor_->publish_state(actual_r);
            }
            if (exp0_sensor_) {
                char buf[10];
                snprintf(buf, sizeof(buf), "0x%04X", last_telltales_mask_);
                exp0_sensor_->publish_state(buf);
            }
            if (exp1_sensor_ && last_fuel_step_ >= 1 && last_fuel_step_ <= 19) {
                char buf[10];
                snprintf(buf, sizeof(buf), "0x%04X", FUEL_RES_MASKS[last_fuel_step_ - 1]);
                exp1_sensor_->publish_state(buf);
            }
        }
    }

private:
    i2c::I2CBus *i2c_bus_{nullptr};
    uint8_t exp0_addr_{0x20};
    uint8_t exp1_addr_{0x21};
    uint16_t port_{8888};
    int sock_{-1};
    bool network_ready_{false};

    bool has_received_packet_{false};
    uint32_t packets_received_{0};
    uint32_t last_log_time_{0};
    uint32_t last_telemetry_publish_{0};

    uint16_t last_telltales_mask_{0};
    float last_speed_freq_{-1.0f};
    float last_rpm_freq_{-1.0f};
    float last_coolant_duty_{-1.0f};
    float last_fuel_ohm_{-1.0f};
    int last_fuel_step_{-1};

    sensor::Sensor *speed_sensor_{nullptr};
    sensor::Sensor *rpm_sensor_{nullptr};
    sensor::Sensor *coolant_sensor_{nullptr};
    sensor::Sensor *fuel_sensor_{nullptr};
    text_sensor::TextSensor *exp0_sensor_{nullptr};
    text_sensor::TextSensor *exp1_sensor_{nullptr};
};

static UdpReceiver *global_udp_receiver = nullptr;

} // namespace vehicle_emulator
} // namespace esphome
