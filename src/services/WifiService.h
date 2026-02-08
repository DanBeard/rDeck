#pragma once
#include "BaseService.h"

#ifdef ESP_PLATFORM
#include <WiFi.h>
#endif

class Settings;

enum class WifiStatus : uint8_t {
    DISCONNECTED = 0,
    CONNECTING,
    CONNECTED,
    CONNECTION_FAILED
};

class WifiService : public BaseService {
    using BaseService::BaseService;

public:
    static constexpr const char* settingsSection = "network";

    virtual void start(RetOS* retos) override;
    virtual void tick(const unsigned long tmillis) override;

    // Connection management
    bool connect(const char* ssid, const char* password);
    void disconnect();
    bool isConnected() const;
    WifiStatus getStatus() const { return _wifiStatus; }

    // Network info
    const char* getSSID() const { return _currentSSID; }
#ifdef ESP_PLATFORM
    IPAddress localIP() const;
#endif

    // Settings
    static bool drawSettings(lv_obj_t* column, Settings* settings);
    static void applySettings();

    // TCP server configuration
    const char* getTcpHost() const { return _tcpHost; }
    uint16_t getTcpPort() const { return _tcpPort; }
    bool isWifiModeEnabled() const { return _wifiModeEnabled; }

protected:
    void updateIcon();
    void loadSettings();

    WifiStatus _wifiStatus = WifiStatus::DISCONNECTED;
    char _currentSSID[64] = {0};
    char _currentPassword[64] = {0};
    char _tcpHost[64] = {0};
    uint16_t _tcpPort = 4242;
    bool _wifiModeEnabled = false;

    unsigned long _lastConnectionAttempt = 0;
    static const unsigned long CONNECTION_RETRY_INTERVAL = 30000;  // 30 seconds
    static const unsigned long CONNECTION_TIMEOUT = 15000;  // 15 seconds

    unsigned long _connectionStartTime = 0;
};
