#ifdef RET_PLATFORM_EMU

#include "EmulatorLora.h"
#include <cstdio>
#include <cstring>

void EmulatorLora::initLora() {
    // Initialize config with default values to avoid crashes when displaying settings
    // (startLora may not be called if using TCP mode instead of LoRa)
    config.frequency = 915.0f;
    config.bandwidth = 125.0f;
    config.sf = 7;
    config.cr = 5;
    config.power = 17;
    config.preamble_len = 18;
    config.crc = 1;
    config.explicitHeader = true;

    printf("[EmulatorLora] Initialized (stub mode)\n");
}

bool EmulatorLora::startLora(const LoraConfig& newConfig) {
    config = newConfig;
    _online = true;

    if (_logging) {
        printf("[EmulatorLora] Started - freq=%.3f MHz, SF=%d, BW=%.0f kHz, power=%d dBm\n",
               config.frequency, config.sf, config.bandwidth, config.power);
    }

    return false;  // false = no error
}

bool EmulatorLora::changeConfig(const LoraConfig& newConfig) {
    config = newConfig;

    if (_logging) {
        printf("[EmulatorLora] Config changed - freq=%.3f MHz, SF=%d\n",
               config.frequency, config.sf);
    }

    return false;  // false = no error
}

bool EmulatorLora::hasPacket() {
    std::lock_guard<std::mutex> lock(_mutex);
    return !_recvQueue.empty();
}

size_t EmulatorLora::read(uint8_t* data, uint32_t len) {
    std::lock_guard<std::mutex> lock(_mutex);

    if (_recvQueue.empty()) {
        return 0;
    }

    const auto& packet = _recvQueue.front();
    size_t copyLen = std::min((size_t)len, packet.size());
    memcpy(data, packet.data(), copyLen);
    _recvQueue.pop();

    if (_logging) {
        printf("[EmulatorLora] Read %zu bytes\n", copyLen);
    }

    return copyLen;
}

bool EmulatorLora::transmit(uint8_t* data, uint32_t len) {
    _txCount++;

    if (_logging) {
        printf("[EmulatorLora] TX %u bytes (packet #%zu, discarded in stub mode)\n",
               len, _txCount);

        // Print first few bytes for debugging
        printf("[EmulatorLora] Data: ");
        for (uint32_t i = 0; i < std::min(len, 16u); i++) {
            printf("%02X ", data[i]);
        }
        if (len > 16) printf("...");
        printf("\n");
    }

    return false;  // false = no error (success)
}

float EmulatorLora::getRSSI() {
    return _rssi;
}

void EmulatorLora::injectPacket(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(_mutex);
    _recvQueue.emplace(data, data + len);

    if (_logging) {
        printf("[EmulatorLora] Injected packet (%zu bytes)\n", len);
    }
}

void EmulatorLora::injectPacket(const std::vector<uint8_t>& packet) {
    std::lock_guard<std::mutex> lock(_mutex);
    _recvQueue.push(packet);

    if (_logging) {
        printf("[EmulatorLora] Injected packet (%zu bytes)\n", packet.size());
    }
}

#endif
