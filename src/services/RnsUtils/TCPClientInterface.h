#pragma once

#include <Interface.h>
#include <Bytes.h>
#include <Type.h>
#include <stdint.h>

#ifdef ESP_PLATFORM
#include <WiFiClient.h>
#elif defined(RET_PLATFORM_EMU)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#endif

class RnsService;

namespace RNS { namespace Interfaces {

/**
 * TCP Client Interface for Reticulum
 *
 * Implements a TCP client interface that connects to a Reticulum TCP server
 * (like rnsd). Uses HDLC-like framing with KISS escaping for stream transport.
 *
 * Frame format:
 * - FLAG (0x7E) marks frame boundaries
 * - ESC (0x7D) escapes special bytes
 * - FLAG in data becomes ESC + 0x5E
 * - ESC in data becomes ESC + 0x5D
 */
class TCPClientInterface : public InterfaceImpl {

public:
    TCPClientInterface(RnsService* service);
    virtual ~TCPClientInterface();

    // Connection management
    bool start(const char* host, uint16_t port);
    void stop();
    void tick(RNS::Interface& interface);

    bool isConnected() const { return _connected; }
    bool hasHost() const { return strlen(_host) > 0; }

    virtual inline std::string toString() const { return "TCPClientInterface[" + _name + "]"; }

protected:
    virtual void send_outgoing(const Bytes& data);

private:
    // HDLC framing constants
    static const uint8_t FLAG = 0x7E;
    static const uint8_t ESC = 0x7D;
    static const uint8_t ESC_MASK = 0x20;

    // Encode data with HDLC-like escaping
    size_t hdlcEncode(const uint8_t* data, size_t len, uint8_t* out, size_t outMaxLen);

    // Decode HDLC frame, returns true if complete frame received
    bool hdlcDecode(uint8_t byte);

    // Process a complete received frame
    void processFrame();

    RnsService* _rns_service;

#ifdef ESP_PLATFORM
    WiFiClient _client;
#elif defined(RET_PLATFORM_EMU)
    int _sockfd = -1;
#endif

    char _host[64] = {0};
    uint16_t _port = 4242;
    bool _connected = false;

    // Receive buffer for HDLC framing
    static const size_t RX_BUFFER_SIZE = 1024;
    uint8_t _rxBuffer[RX_BUFFER_SIZE];
    size_t _rxBufferLen = 0;
    bool _inEscape = false;
    bool _inFrame = false;

    // Reconnection handling with capped exponential backoff
    unsigned long _lastConnectAttempt = 0;
    unsigned long _reconnectInterval = RECONNECT_INTERVAL_MIN;
    static constexpr unsigned long RECONNECT_INTERVAL_MIN = 5000;    // Start at 5s
    static constexpr unsigned long RECONNECT_INTERVAL_MAX = 300000;  // Cap at 5 min
};

}}  // namespace RNS::Interfaces
