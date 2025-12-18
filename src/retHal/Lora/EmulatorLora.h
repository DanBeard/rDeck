#pragma once
#ifdef RET_PLATFORM_EMU

#include "BaseLora.h"
#include <queue>
#include <vector>
#include <mutex>

/**
 * EmulatorLora - Stub LoRa implementation for emulator.
 *
 * In stub mode: Packets are logged and discarded.
 * For testing: Provides injectPacket() to simulate incoming packets.
 *
 * Future: Can be extended to use UDP/TCP for real network communication.
 */
class EmulatorLora : public BaseLora {
public:
    virtual void initLora() override;
    virtual bool startLora(const LoraConfig& config) override;
    virtual bool changeConfig(const LoraConfig& config) override;
    virtual bool hasPacket() override;
    virtual size_t read(uint8_t* data, uint32_t len) override;
    virtual bool transmit(uint8_t* data, uint32_t len) override;
    virtual float getRSSI() override;

    // Test helpers - inject packets for testing
    void injectPacket(const uint8_t* data, size_t len);
    void injectPacket(const std::vector<uint8_t>& packet);

    // Get number of transmitted packets (for testing)
    size_t getTransmitCount() const { return _txCount; }

    // Enable/disable logging
    void setLogging(bool enabled) { _logging = enabled; }

private:
    std::queue<std::vector<uint8_t>> _recvQueue;
    std::mutex _mutex;
    bool _online = false;
    bool _logging = true;
    size_t _txCount = 0;
    float _rssi = -50.0f;  // Fake RSSI
};

#endif
