# Debug Plan: LXMF Messages Not Working

## User Context
- **Symptom:** Both sending AND receiving fail
- **Regression:** Messages used to work, then stopped
- **Test setup:** Testing with Sideband (Python Reticulum/LXMF)
- **Announces work:** RF layer confirmed working

## ROOT CAUSE FOUND: Wrong Destination in Packet Constructor

**File:** `src/services/RnsService.cpp` line 310

```cpp
// CURRENT (WRONG):
_sending_packet = new RNS::Packet(lxmf_delivery_src, fullMsg);

// SHOULD BE:
_sending_packet = new RNS::Packet(their_dest, fullMsg);
```

### Why This Breaks Everything:

The `Packet::pack()` method (Packet.cpp:322) encrypts data using `_destination.encrypt()`:
- Current code passes `lxmf_delivery_src` (the **sender's** IN destination)
- This encrypts with the **sender's** public key (only sender can decrypt!)
- The packet's destination hash becomes the **sender's** hash (routes back to sender!)

**Result:** Messages are encrypted so only the sender can read them, AND they're addressed to the sender, not the recipient. Complete failure for both send and receive.

### The Fix:

Change line 310 in `src/services/RnsService.cpp`:
```cpp
// Before (line 304):
RNS::Destination their_dest(their_ident, RNS::Type::Destination::OUT,
                            RNS::Type::Destination::SINGLE, "lxmf", "delivery");
msg->pack(lxmf_delivery_src, their_dest);

// Line 310 - change this:
_sending_packet = new RNS::Packet(their_dest, fullMsg);  // Use recipient's destination!
```

**Note:** `their_dest` is currently a local variable that goes out of scope. Either:
1. Make it a member variable, OR
2. Store it in the Message object, OR
3. Recreate it when needed (current approach but use it for the packet)

---

## Secondary Issues (UI bugs - fix after messaging works)

1. **`renderMessageInConversation()` is NEVER CALLED** - Messages stored but not rendered
2. **`sendMessageUpdateEvent()` is NEVER CALLED** - Status updates don't reach UI
3. **`_queued_msgs` set never populated**

---

## Implementation Plan

### Step 1: Fix the Packet Destination Bug (PRIMARY FIX)

**File:** `src/services/RnsService.cpp`

The `their_dest` variable goes out of scope before the packet is sent. Need to either:

**Option A (Recommended):** Keep destination alive until packet is sent
```cpp
// Add member variable to RnsService class (in .h):
RNS::Destination _current_recipient_dest;

// In transmitMsg():
_current_recipient_dest = RNS::Destination(their_ident, RNS::Type::Destination::OUT,
                                           RNS::Type::Destination::SINGLE, "lxmf", "delivery");
msg->pack(lxmf_delivery_src, _current_recipient_dest);
// ...
_sending_packet = new RNS::Packet(_current_recipient_dest, fullMsg);
```

**Option B:** Recreate destination inline (simpler but duplicates code)
```cpp
void RnsService::transmitMsg(shared_ptr<Retcon::LXMF::Message>& msg) {
    // ... existing code ...

    RNS::Identity their_ident = RNS::Identity::recall(msg->dest);
    RNS::Destination their_dest(their_ident, RNS::Type::Destination::OUT,
                                RNS::Type::Destination::SINGLE, "lxmf", "delivery");
    msg->pack(lxmf_delivery_src, their_dest);

    // ...

    _sending_packet = new RNS::Packet(their_dest, fullMsg);  // FIX: Use their_dest!
    _sending_packet->send();
    // ...
}
```

### Step 2: Test with Sideband

After the fix:
```bash
cd ~/rDeck
pio run -e T-Deck-Pro --target upload
```

Test:
1. Send message from rDeck -> Sideband should receive
2. Send message from Sideband -> rDeck should receive

### Step 3: Fix UI Bugs (after messaging works)

**File:** `src/apps/UChat.cpp`

1. In `drawCurrentConversation()`, add loop to render messages:
```cpp
// After setting up the conversation UI, render existing messages:
for (auto& msg : current_conv.messages) {
    renderMessageInConversation(msg);
}
```

2. Call `sendMessageUpdateEvent()` in RnsService callbacks:
   - Add call in `transmit_delivery_cb()` after status change
   - Add call in `transmit_timeout_cb()` after status change

---

## Files to Modify

| File | Change |
|------|--------|
| `src/services/RnsService.h` | Add `_current_recipient_dest` member (if using Option A) |
| `src/services/RnsService.cpp` | Fix line 310: use `their_dest` instead of `lxmf_delivery_src` |
| `src/apps/UChat.cpp` | Call `renderMessageInConversation()` in `drawCurrentConversation()` |
| `src/services/RnsService.cpp` | Call `sendMessageUpdateEvent()` in callbacks |
