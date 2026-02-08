# Companion Server

A Reticulum companion server for rDeck that provides infrastructure services (NTP, web search) to trusted devices over the mesh network.

## Features

- **Trust Management**: Mutual trust workflow - server sends trust offers, device accepts
- **NTP Service**: Provides time synchronization over Reticulum
- **Web Search Service**: Proxies search queries to DuckDuckGo
- **Textual TUI**: Interactive terminal interface for monitoring and management

## Installation

```bash
cd companion-server
pip install -e .
```

## Usage

```bash
# Run the companion server
python -m companion_server

# Or use the installed script
companion-server
```

## Trust Workflow

1. rDeck announces on the network (existing behavior)
2. Server TUI shows announce in stream
3. Server user clicks "Trust" on rDeck's announce
4. Server sends TRUST_OFFER message (name, services list)
5. rDeck receives, stores as "pending"
6. rDeck user opens Settings -> Trusted Servers -> sees pending offer
7. rDeck user clicks "Accept"
8. rDeck sends TRUST_ACCEPT message
9. Server receives, marks mutual trust
10. Services now available!

## Configuration

Configuration is stored in `~/.companion-server/`:

- `config.json` - Server settings
- `trust.json` - Trusted device state
- `identity` - Reticulum identity files

## Development

```bash
# Install with dev dependencies
pip install -e ".[dev]"

# Run tests
pytest
```

## Protocol

Messages use LXMF's `fields` field with msgpack encoding:

```python
fields = {
    "msg_type": <uint8>,      # Message type enum
    "service": <string>,      # "ntp", "search", etc.
    "payload": <msgpack>,     # Service-specific data
    "request_id": <uint32>    # For request/response correlation
}
```

### Message Types

| Type | Value | Direction | Purpose |
|------|-------|-----------|---------|
| TRUST_OFFER | 0x01 | Server->Device | "I want to serve you" |
| TRUST_ACCEPT | 0x02 | Device->Server | "I accept your services" |
| TRUST_REVOKE | 0x03 | Either | Revoke trust |
| NTP_REQUEST | 0x10 | Device->Server | Request time sync |
| NTP_RESPONSE | 0x11 | Server->Device | Time response |
| SEARCH_REQUEST | 0x20 | Device->Server | Search query |
| SEARCH_RESPONSE | 0x21 | Server->Device | Search results |
