#include "TCPClientInterface.h"
#include <memory>
#include <Log.h>
#include <algorithm>
#include "../RnsService.h"

#ifdef ESP_PLATFORM
#include <WiFi.h>
#elif defined(RET_PLATFORM_EMU)
#include <errno.h>
#include <cstring>
#endif

using namespace RNS;
using namespace RNS::Interfaces;

TCPClientInterface::TCPClientInterface(RnsService* service)
    : _rns_service(service), InterfaceImpl("TCP") {
    _IN = true;
    _OUT = true;
    // TCP over WiFi has much higher bandwidth than LoRa
    // Typical WiFi: 10-100+ Mbps, but we'll be conservative
    _bitrate = 1000000;  // 1 Mbps nominal
}

TCPClientInterface::~TCPClientInterface() {
    stop();
}

bool TCPClientInterface::start(const char* host, uint16_t port) {
    strncpy(_host, host, sizeof(_host) - 1);
    _host[sizeof(_host) - 1] = '\0';
    _port = port;

    Serial.printf("[TCP] Connecting to %s:%d...\n", _host, _port);

#ifdef ESP_PLATFORM
    // Check WiFi connection first
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[TCP] WiFi not connected, cannot start TCP interface");
        _online = false;
        return false;
    }

    // Attempt connection
    if (_client.connect(_host, _port)) {
        _connected = true;
        _online = true;
        Serial.printf("[TCP] Connected to %s:%d\n", _host, _port);
        return true;
    } else {
        _connected = false;
        _online = false;
        Serial.printf("[TCP] Failed to connect to %s:%d\n", _host, _port);
        return false;
    }

#elif defined(RET_PLATFORM_EMU)
    // Create socket
    _sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_sockfd < 0) {
        Serial.printf("[TCP] Failed to create socket: %s\n", strerror(errno));
        _online = false;
        return false;
    }

    // Resolve hostname
    struct hostent* server = gethostbyname(_host);
    if (server == nullptr) {
        Serial.printf("[TCP] Failed to resolve hostname: %s\n", _host);
        close(_sockfd);
        _sockfd = -1;
        _online = false;
        return false;
    }

    // Setup server address
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(_port);

    // Connect
    if (connect(_sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        Serial.printf("[TCP] Failed to connect to %s:%d: %s\n", _host, _port, strerror(errno));
        close(_sockfd);
        _sockfd = -1;
        _connected = false;
        _online = false;
        return false;
    }

    // Set socket to non-blocking mode for tick() polling
    int flags = fcntl(_sockfd, F_GETFL, 0);
    fcntl(_sockfd, F_SETFL, flags | O_NONBLOCK);

    _connected = true;
    _online = true;
    Serial.printf("[TCP] Connected to %s:%d\n", _host, _port);
    return true;

#else
    Serial.println("[TCP] TCP interface not available on this platform");
    return false;
#endif
}

void TCPClientInterface::stop() {
#ifdef ESP_PLATFORM
    if (_connected) {
        _client.stop();
        _connected = false;
        _online = false;
        Serial.println("[TCP] Disconnected");
    }
#elif defined(RET_PLATFORM_EMU)
    if (_sockfd >= 0) {
        close(_sockfd);
        _sockfd = -1;
        _connected = false;
        _online = false;
        Serial.println("[TCP] Disconnected");
    }
#endif
}

void TCPClientInterface::tick(RNS::Interface& interface) {
#ifdef ESP_PLATFORM
    // Check connection status
    if (_connected && !_client.connected()) {
        Serial.println("[TCP] Connection lost");
        _connected = false;
        _online = false;
    }

    // Attempt reconnection if needed (exponential backoff)
    if (!_connected && strlen(_host) > 0) {
        unsigned long now = millis();
        if (now - _lastConnectAttempt > _reconnectInterval || now < _lastConnectAttempt) {
            _lastConnectAttempt = now;
            Serial.printf("[TCP] Attempting reconnection to %s:%d (next retry in %lus)\n",
                _host, _port, _reconnectInterval * 2 / 1000);
            if (_client.connect(_host, _port)) {
                _connected = true;
                _online = true;
                _reconnectInterval = RECONNECT_INTERVAL_MIN;
                Serial.println("[TCP] Reconnected");
            } else {
                _reconnectInterval = std::min(_reconnectInterval * 2, RECONNECT_INTERVAL_MAX);
            }
        }
        return;
    }

    // Read incoming data
    if (_connected && _client.available()) {
        while (_client.available()) {
            uint8_t byte = _client.read();
            if (hdlcDecode(byte)) {
                // Complete frame received
                processFrame();
            }
        }
    }

#elif defined(RET_PLATFORM_EMU)
    // Check connection status by attempting a zero-byte read
    if (_connected && _sockfd >= 0) {
        char testBuf;
        ssize_t result = recv(_sockfd, &testBuf, 1, MSG_PEEK | MSG_DONTWAIT);
        if (result == 0) {
            // Connection closed by peer
            Serial.println("[TCP] Connection closed by peer");
            close(_sockfd);
            _sockfd = -1;
            _connected = false;
            _online = false;
        } else if (result < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // Error
            Serial.printf("[TCP] Connection error: %s\n", strerror(errno));
            close(_sockfd);
            _sockfd = -1;
            _connected = false;
            _online = false;
        }
    }

    // Attempt reconnection if needed (exponential backoff)
    if (!_connected && strlen(_host) > 0) {
        unsigned long now = millis();
        if (now - _lastConnectAttempt > _reconnectInterval || now < _lastConnectAttempt) {
            _lastConnectAttempt = now;
            Serial.printf("[TCP] Attempting reconnection to %s:%d (next retry in %lus)\n",
                _host, _port, _reconnectInterval * 2 / 1000);
            if (start(_host, _port)) {
                _reconnectInterval = RECONNECT_INTERVAL_MIN;
            } else {
                _reconnectInterval = std::min(_reconnectInterval * 2, RECONNECT_INTERVAL_MAX);
            }
        }
        return;
    }

    // Read incoming data (non-blocking)
    if (_connected && _sockfd >= 0) {
        uint8_t buf[256];
        ssize_t bytesRead;
        while ((bytesRead = recv(_sockfd, buf, sizeof(buf), MSG_DONTWAIT)) > 0) {
            for (ssize_t i = 0; i < bytesRead; i++) {
                if (hdlcDecode(buf[i])) {
                    // Complete frame received
                    processFrame();
                }
            }
        }
        // EAGAIN/EWOULDBLOCK is expected for non-blocking socket with no data
        if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            Serial.printf("[TCP] Read error: %s\n", strerror(errno));
        }
    }
#endif
}

size_t TCPClientInterface::hdlcEncode(const uint8_t* data, size_t len, uint8_t* out, size_t outMaxLen) {
    size_t outLen = 0;

    // Start with FLAG
    if (outLen < outMaxLen) out[outLen++] = FLAG;

    // Encode data with escaping
    for (size_t i = 0; i < len && outLen < outMaxLen - 2; i++) {
        uint8_t byte = data[i];
        if (byte == FLAG || byte == ESC) {
            if (outLen < outMaxLen - 1) {
                out[outLen++] = ESC;
                out[outLen++] = byte ^ ESC_MASK;
            }
        } else {
            out[outLen++] = byte;
        }
    }

    // End with FLAG
    if (outLen < outMaxLen) out[outLen++] = FLAG;

    return outLen;
}

bool TCPClientInterface::hdlcDecode(uint8_t byte) {
    if (byte == FLAG) {
        if (_inFrame && _rxBufferLen > 0) {
            // End of frame
            _inFrame = false;
            return true;
        } else {
            // Start of frame
            _inFrame = true;
            _rxBufferLen = 0;
            _inEscape = false;
            return false;
        }
    }

    if (!_inFrame) {
        // Data outside of frame, ignore
        return false;
    }

    if (_inEscape) {
        // Previous byte was ESC, un-escape this byte
        byte = byte ^ ESC_MASK;
        _inEscape = false;
    } else if (byte == ESC) {
        // Next byte is escaped
        _inEscape = true;
        return false;
    }

    // Add byte to buffer
    if (_rxBufferLen < RX_BUFFER_SIZE) {
        _rxBuffer[_rxBufferLen++] = byte;
    } else {
        // Buffer overflow, reset
        Serial.println("[TCP] RX buffer overflow, resetting frame");
        _rxBufferLen = 0;
        _inFrame = false;
    }

    return false;
}

void TCPClientInterface::processFrame() {
    if (_rxBufferLen == 0) {
        return;
    }

    Serial.printf("[TCP] Received frame: %d bytes\n", _rxBufferLen);

    // Notify service of activity
    if (_rns_service) {
        _rns_service->actionHappened();
    }

    // Pass to Reticulum transport
    Bytes data(_rxBuffer, _rxBufferLen);
    handle_incoming(data);

    // Reset buffer for next frame
    _rxBufferLen = 0;
}

void TCPClientInterface::send_outgoing(const Bytes& data) {
    if (!_connected) {
        Serial.println("[TCP] Cannot send: not connected");
        return;
    }

    // Notify service of activity
    if (_rns_service) {
        _rns_service->actionHappened();
    }

    DEBUG(toString() + ".send_outgoing: data: " + data.toHex());
    Serial.printf("[TCP TX] Transmitting %d bytes\n", data.size());

    // Encode with HDLC framing
    // Worst case: each byte needs escaping (2x) + 2 flags
    size_t maxEncodedLen = data.size() * 2 + 2;
    uint8_t* encoded = new uint8_t[maxEncodedLen];

    size_t encodedLen = hdlcEncode(data.data(), data.size(), encoded, maxEncodedLen);

#ifdef ESP_PLATFORM
    if (!_client.connected()) {
        Serial.println("[TCP] Cannot send: connection lost");
        delete[] encoded;
        return;
    }

    // Send over TCP
    size_t written = _client.write(encoded, encodedLen);
    if (written != encodedLen) {
        Serial.printf("[TCP] Write error: wrote %d of %d bytes\n", written, encodedLen);
    } else {
        Serial.printf("[TCP TX] Sent %d bytes (encoded from %d)\n", encodedLen, data.size());
    }

#elif defined(RET_PLATFORM_EMU)
    if (_sockfd < 0) {
        Serial.println("[TCP] Cannot send: socket not open");
        delete[] encoded;
        return;
    }

    // Send over TCP
    ssize_t written = send(_sockfd, encoded, encodedLen, 0);
    if (written < 0) {
        Serial.printf("[TCP] Write error: %s\n", strerror(errno));
    } else if ((size_t)written != encodedLen) {
        Serial.printf("[TCP] Partial write: wrote %zd of %zu bytes\n", written, encodedLen);
    } else {
        Serial.printf("[TCP TX] Sent %zu bytes (encoded from %zu)\n", encodedLen, data.size());
    }
#endif

    delete[] encoded;

    // Notify transport of outgoing data
    handle_outgoing(data);
}
