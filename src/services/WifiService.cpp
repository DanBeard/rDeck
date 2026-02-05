#include "WifiService.h"
#include "lvgl.h"
#include "apps/Settings.h"

#ifdef ESP_PLATFORM
#include <WiFi.h>
#endif

// Forward declaration of functor_callback from Settings.cpp
extern void functor_callback(lv_event_t* e);

void WifiService::start(RetOS* retos) {
    loadSettings();
    updateIcon();

#ifdef ESP_PLATFORM
    // Set WiFi mode to station
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);  // Disconnect from any previous connections

    // If WiFi mode is enabled and we have credentials, auto-connect
    if (_wifiModeEnabled && strlen(_currentSSID) > 0) {
        Serial.printf("[WiFi] Auto-connecting to %s\n", _currentSSID);
        connect(_currentSSID, _currentPassword);
    }
#elif defined(RET_PLATFORM_EMU)
    // On emulator, we use host networking directly - no WiFi management needed
    // If WiFi/TCP mode is enabled, just mark as "connected" since host has network
    if (_wifiModeEnabled) {
        Serial.println("[WiFi] Emulator mode: using host networking");
        _wifiStatus = WifiStatus::CONNECTED;
        updateIcon();
    }
#endif

    _status = RUNNING;
}

void WifiService::tick(const unsigned long tMillis) {
#ifdef ESP_PLATFORM
    // Monitor connection status
    if (_wifiStatus == WifiStatus::CONNECTING) {
        wl_status_t status = WiFi.status();

        if (status == WL_CONNECTED) {
            _wifiStatus = WifiStatus::CONNECTED;
            Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            updateIcon();
            actionHappened();
        } else if (status == WL_CONNECT_FAILED ||
                   status == WL_NO_SSID_AVAIL ||
                   (tMillis - _connectionStartTime > CONNECTION_TIMEOUT)) {
            _wifiStatus = WifiStatus::CONNECTION_FAILED;
            Serial.println("[WiFi] Connection failed");
            updateIcon();
        }
    }

    // Handle disconnections
    if (_wifiStatus == WifiStatus::CONNECTED && WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Lost connection");
        _wifiStatus = WifiStatus::DISCONNECTED;
        updateIcon();
    }

    // Auto-reconnect if enabled and disconnected
    if (_wifiModeEnabled &&
        (_wifiStatus == WifiStatus::DISCONNECTED || _wifiStatus == WifiStatus::CONNECTION_FAILED) &&
        strlen(_currentSSID) > 0 &&
        (tMillis - _lastConnectionAttempt > CONNECTION_RETRY_INTERVAL || tMillis < _lastConnectionAttempt)) {
        Serial.println("[WiFi] Attempting reconnection...");
        connect(_currentSSID, _currentPassword);
    }
#elif defined(RET_PLATFORM_EMU)
    // On emulator, no WiFi management needed - host networking is always available
    // Just ensure status matches enabled state
    if (_wifiModeEnabled && _wifiStatus != WifiStatus::CONNECTED) {
        _wifiStatus = WifiStatus::CONNECTED;
        updateIcon();
    } else if (!_wifiModeEnabled && _wifiStatus == WifiStatus::CONNECTED) {
        _wifiStatus = WifiStatus::DISCONNECTED;
        updateIcon();
    }
#endif
}

bool WifiService::connect(const char* ssid, const char* password) {
    strncpy(_currentSSID, ssid, sizeof(_currentSSID) - 1);
    _currentSSID[sizeof(_currentSSID) - 1] = '\0';

    if (password) {
        strncpy(_currentPassword, password, sizeof(_currentPassword) - 1);
        _currentPassword[sizeof(_currentPassword) - 1] = '\0';
    } else {
        _currentPassword[0] = '\0';
    }

#ifdef ESP_PLATFORM
    if (_wifiStatus == WifiStatus::CONNECTING) {
        return false;  // Already attempting connection
    }

    Serial.printf("[WiFi] Connecting to %s...\n", ssid);
    WiFi.begin(ssid, password);

    _wifiStatus = WifiStatus::CONNECTING;
    _connectionStartTime = millis();
    _lastConnectionAttempt = millis();
    updateIcon();

    return true;
#elif defined(RET_PLATFORM_EMU)
    // On emulator, we use host networking - just mark as connected
    Serial.printf("[WiFi] Emulator: using host networking (SSID '%s' ignored)\n", ssid);
    _wifiStatus = WifiStatus::CONNECTED;
    updateIcon();
    return true;
#else
    Serial.println("[WiFi] WiFi not available on this platform");
    return false;
#endif
}

void WifiService::disconnect() {
#ifdef ESP_PLATFORM
    WiFi.disconnect(true);
#endif
    _wifiStatus = WifiStatus::DISCONNECTED;
    Serial.println("[WiFi] Disconnected");
    updateIcon();
}

bool WifiService::isConnected() const {
#ifdef ESP_PLATFORM
    return _wifiStatus == WifiStatus::CONNECTED && WiFi.status() == WL_CONNECTED;
#elif defined(RET_PLATFORM_EMU)
    // On emulator, we're "connected" when WiFi mode is enabled (using host networking)
    return _wifiModeEnabled;
#else
    return false;
#endif
}

#ifdef ESP_PLATFORM
IPAddress WifiService::localIP() const {
    return WiFi.localIP();
}
#endif

void WifiService::loadSettings() {
    JsonObject settings = Settings::getSettings(settingsSection);

    const char* ssid = settings["wifi_ssid"];
    if (ssid) {
        strncpy(_currentSSID, ssid, sizeof(_currentSSID) - 1);
        _currentSSID[sizeof(_currentSSID) - 1] = '\0';
    }

    const char* password = settings["wifi_password"];
    if (password) {
        strncpy(_currentPassword, password, sizeof(_currentPassword) - 1);
        _currentPassword[sizeof(_currentPassword) - 1] = '\0';
    }

    const char* host = settings["tcp_host"];
    if (host) {
        strncpy(_tcpHost, host, sizeof(_tcpHost) - 1);
        _tcpHost[sizeof(_tcpHost) - 1] = '\0';
    }

    _tcpPort = settings["tcp_port"] | 4242;
    _wifiModeEnabled = settings["enabled"] | false;

    Serial.printf("[WiFi] Settings loaded: SSID=%s, host=%s:%d, enabled=%s\n",
                  _currentSSID, _tcpHost, _tcpPort, _wifiModeEnabled ? "yes" : "no");
}

void WifiService::updateIcon() {
    const char* icon;
    lv_opa_t opacity = LV_OPA_100;

    switch (_wifiStatus) {
        case WifiStatus::CONNECTED:
            icon = LV_SYMBOL_WIFI;
            break;
        case WifiStatus::CONNECTING:
            icon = "W?";
            break;
        case WifiStatus::CONNECTION_FAILED:
            icon = "W!";
            break;
        case WifiStatus::DISCONNECTED:
        default:
            // Only show icon if WiFi mode is enabled
            if (_wifiModeEnabled) {
                icon = "W-";
            } else {
                icon = "";  // Hide icon when WiFi mode disabled
            }
            break;
    }

    ServiceIcon iconInfo = {
        .serviceID = this->_id,
        .icon = icon,
        .opacity = opacity,
    };
    _retos->ui()->setServiceIcon(iconInfo);
}

// Static settings variables
static FunctorCallback wifiSettingsCallback;
static char tempSSID[64] = {0};
static char tempPassword[64] = {0};
static char tempHost[64] = {0};
static uint16_t tempPort = 4242;
static bool tempEnabled = false;

bool WifiService::drawSettings(lv_obj_t* container, Settings* settings) {
    settings->drawSettingsSectionHeader(container, "Network (WiFi/TCP)");

    JsonObject netSettings = Settings::getSettings(settingsSection);

    // Load current values
    const char* ssid = netSettings["wifi_ssid"] | "";
    const char* password = netSettings["wifi_password"] | "";
    const char* host = netSettings["tcp_host"] | "";
    tempPort = netSettings["tcp_port"] | 4242;
    tempEnabled = netSettings["enabled"] | false;

    strncpy(tempSSID, ssid, sizeof(tempSSID) - 1);
    strncpy(tempPassword, password, sizeof(tempPassword) - 1);
    strncpy(tempHost, host, sizeof(tempHost) - 1);

    lv_obj_t *ssidInput, *passwordInput, *hostInput, *portInput;

    ssidInput = settings->drawSettingsTextInputRow(container, "WiFi SSID", tempSSID, &wifiSettingsCallback);
    passwordInput = settings->drawSettingsTextInputRow(container, "Password", tempPassword, &wifiSettingsCallback);
    // Set password mode for the password field
    lv_textarea_set_password_mode(passwordInput, true);

    hostInput = settings->drawSettingsTextInputRow(container, "TCP Host", tempHost, &wifiSettingsCallback);

    char portStr[8];
    snprintf(portStr, sizeof(portStr), "%d", tempPort);
    portInput = settings->drawSettingsTextInputRow(container, "TCP Port", portStr, &wifiSettingsCallback);

    // Enable/disable toggle - create as a button that toggles
    lv_obj_t* enableLabel = lv_label_create(container);
#ifdef RET_PLATFORM_EMU
    lv_label_set_text(enableLabel, "Use TCP (not LoRa)");
#else
    lv_label_set_text(enableLabel, "Use WiFi (not LoRa)");
#endif
    lv_obj_set_size(enableLabel, LV_PCT(55), LV_SIZE_CONTENT);
    lv_obj_add_flag(enableLabel, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    lv_obj_set_style_pad_top(enableLabel, 8, LV_PART_MAIN);

    lv_obj_t* enableBtn = lv_btn_create(container);
    lv_obj_set_size(enableBtn, LV_PCT(40), 35);
    lv_obj_set_style_border_width(enableBtn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(enableBtn, retOsGlobalPtr->ui()->fg_color(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(enableBtn, retOsGlobalPtr->ui()->bg_color(), LV_PART_MAIN);

    lv_obj_t* btnLabel = lv_label_create(enableBtn);
    lv_label_set_text(btnLabel, tempEnabled ? LV_SYMBOL_OK " Enabled" : LV_SYMBOL_CLOSE " Disabled");
    lv_obj_set_style_text_color(btnLabel, retOsGlobalPtr->ui()->fg_color(), LV_PART_MAIN);
    lv_obj_center(btnLabel);

    // Status display
    WifiService* wifiSvc = retOsGlobalPtr->fetchService<WifiService>();
    if (wifiSvc) {
        lv_obj_t* statusLabel = lv_label_create(container);
        lv_obj_set_size(statusLabel, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_add_flag(statusLabel, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
        lv_obj_set_style_pad_top(statusLabel, 5, LV_PART_MAIN);

        const char* statusText;
        switch (wifiSvc->getStatus()) {
            case WifiStatus::CONNECTED:
#ifdef ESP_PLATFORM
                {
                    static char statusBuf[64];
                    snprintf(statusBuf, sizeof(statusBuf), "Status: Connected (%s)",
                             WiFi.localIP().toString().c_str());
                    statusText = statusBuf;
                }
#elif defined(RET_PLATFORM_EMU)
                statusText = "Status: Using host network";
#else
                statusText = "Status: Connected";
#endif
                break;
            case WifiStatus::CONNECTING:
                statusText = "Status: Connecting...";
                break;
            case WifiStatus::CONNECTION_FAILED:
                statusText = "Status: Connection failed";
                break;
            default:
                statusText = "Status: Disconnected";
                break;
        }
        lv_label_set_text(statusLabel, statusText);
    }

    // Callback to handle text input changes
    wifiSettingsCallback = [ssidInput, passwordInput, hostInput, portInput](lv_event_t* e) {
        lv_obj_t* ta = lv_event_get_target(e);
        const char* value = lv_textarea_get_text(ta);

        if (ta == ssidInput) {
            strncpy(tempSSID, value, sizeof(tempSSID) - 1);
        } else if (ta == passwordInput) {
            strncpy(tempPassword, value, sizeof(tempPassword) - 1);
        } else if (ta == hostInput) {
            strncpy(tempHost, value, sizeof(tempHost) - 1);
        } else if (ta == portInput) {
            tempPort = atoi(value);
            if (tempPort == 0) tempPort = 4242;
        }
    };

    // Toggle button callback
    static FunctorCallback toggleCallback;
    toggleCallback = [btnLabel](lv_event_t* e) {
        tempEnabled = !tempEnabled;
        lv_label_set_text(btnLabel, tempEnabled ? LV_SYMBOL_OK " Enabled" : LV_SYMBOL_CLOSE " Disabled");
    };
    lv_obj_add_event_cb(enableBtn, functor_callback, LV_EVENT_CLICKED, &toggleCallback);

    return true;
}

void WifiService::applySettings() {
    // Save to JSON settings
    JsonObject settings = Settings::getSettings(settingsSection);
    settings["wifi_ssid"] = tempSSID;
    settings["wifi_password"] = tempPassword;
    settings["tcp_host"] = tempHost;
    settings["tcp_port"] = tempPort;
    settings["enabled"] = tempEnabled;

    Serial.printf("[WiFi] Settings saved: SSID=%s, host=%s:%d, enabled=%s\n",
                  tempSSID, tempHost, tempPort, tempEnabled ? "yes" : "no");

    // Apply to running service
    WifiService* wifiSvc = retOsGlobalPtr->fetchService<WifiService>();
    if (wifiSvc) {
        bool wasEnabled = wifiSvc->_wifiModeEnabled;
        bool wasConnected = wifiSvc->isConnected();

        wifiSvc->_wifiModeEnabled = tempEnabled;
        strncpy(wifiSvc->_currentSSID, tempSSID, sizeof(wifiSvc->_currentSSID) - 1);
        strncpy(wifiSvc->_currentPassword, tempPassword, sizeof(wifiSvc->_currentPassword) - 1);
        strncpy(wifiSvc->_tcpHost, tempHost, sizeof(wifiSvc->_tcpHost) - 1);
        wifiSvc->_tcpPort = tempPort;

        // Handle enable/disable changes
        if (tempEnabled && !wasEnabled && strlen(tempSSID) > 0) {
            // WiFi was just enabled, connect
            wifiSvc->connect(tempSSID, tempPassword);
        } else if (!tempEnabled && wasConnected) {
            // WiFi was just disabled, disconnect
            wifiSvc->disconnect();
        } else if (tempEnabled && wasConnected &&
                   (strcmp(tempSSID, wifiSvc->_currentSSID) != 0 ||
                    strcmp(tempPassword, wifiSvc->_currentPassword) != 0)) {
            // Credentials changed while connected, reconnect
            wifiSvc->disconnect();
            wifiSvc->connect(tempSSID, tempPassword);
        }

        wifiSvc->updateIcon();
    }
}
