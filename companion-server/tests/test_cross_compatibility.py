"""Cross-compatibility tests with C++ implementation.

These tests verify that Python can decode data encoded by C++ and vice versa.
They use canonical test vectors that both implementations must handle identically.
"""

import pytest
import msgpack
import binascii

from companion_server.protocol.messages import (
    MessageType,
    TrustOfferPayload,
    TrustAcceptPayload,
    NTPRequestPayload,
    NTPResponsePayload,
    SearchRequestPayload,
    SearchResponsePayload,
    SearchResult,
    TravelMode,
    MapRouteRequestPayload,
    MapRouteResponsePayload,
    MapRouteInstruction,
    MapGeocodeRequestPayload,
    MapGeocodeResponsePayload,
    MapGeocodeResult,
)
from companion_server.protocol.serialization import (
    _encode_payload,
    _decode_payload,
)


class TestCanonicalTestVectors:
    """Test vectors that both Python and C++ must handle identically.

    These test vectors define the "contract" between the two implementations.
    If a test fails here, the corresponding C++ test should also fail.
    """

    def test_trust_offer_vector_1(self):
        """Basic TRUST_OFFER with server name and two services."""
        # This is what C++ should produce and Python should decode
        expected_structure = {
            "server_name": "TestServer",
            "services": ["ntp", "search"],
        }

        # Encode using Python
        payload = TrustOfferPayload(
            server_name="TestServer",
            services=["ntp", "search"],
        )
        encoded = _encode_payload(MessageType.TRUST_OFFER, payload)

        # Verify structure
        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure

        # Verify roundtrip
        decoded = _decode_payload(MessageType.TRUST_OFFER, encoded)
        assert decoded.server_name == "TestServer"
        assert decoded.services == ["ntp", "search"]

    def test_trust_accept_vector_1(self):
        """Basic TRUST_ACCEPT with device name."""
        expected_structure = {
            "device_name": "rDeck",
        }

        payload = TrustAcceptPayload(device_name="rDeck")
        encoded = _encode_payload(MessageType.TRUST_ACCEPT, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure

    def test_ntp_request_vector_1(self):
        """NTP request with specific timestamp."""
        expected_structure = {
            "client_timestamp": 1234567890,
        }

        payload = NTPRequestPayload(client_timestamp=1234567890)
        encoded = _encode_payload(MessageType.NTP_REQUEST, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure

    def test_ntp_response_vector_1(self):
        """NTP response with server and client timestamps."""
        expected_structure = {
            "server_timestamp": 1706825600,
            "client_timestamp": 1000,
        }

        payload = NTPResponsePayload(
            server_timestamp=1706825600,
            client_timestamp=1000,
        )
        encoded = _encode_payload(MessageType.NTP_RESPONSE, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure

    def test_search_request_vector_1(self):
        """Search request with query and max_results."""
        expected_structure = {
            "query": "python programming",
            "max_results": 5,
        }

        payload = SearchRequestPayload(
            query="python programming",
            max_results=5,
        )
        encoded = _encode_payload(MessageType.SEARCH_REQUEST, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure

    def test_search_response_vector_1(self):
        """Search response with results."""
        expected_structure = {
            "query": "test",
            "results": [
                {"title": "Title1", "url": "http://example.com", "snippet": "Snippet1"},
            ],
            "error": None,
        }

        payload = SearchResponsePayload(
            query="test",
            results=[
                SearchResult(title="Title1", url="http://example.com", snippet="Snippet1"),
            ],
            error=None,
        )
        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure

    def test_search_response_with_error_vector(self):
        """Search response with error."""
        expected_structure = {
            "query": "failed query",
            "results": [],
            "error": "Network timeout",
        }

        payload = SearchResponsePayload(
            query="failed query",
            results=[],
            error="Network timeout",
        )
        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure

    def test_route_request_vector(self):
        """MAP_ROUTE_REQUEST with coordinates and travel mode."""
        expected_structure = {
            "start_lat": 478563210,
            "start_lon": -1224567890,
            "end_lat": 478600000,
            "end_lon": -1224500000,
            "mode": 0,  # WALK
        }

        payload = MapRouteRequestPayload(
            start_lat=478563210,
            start_lon=-1224567890,
            end_lat=478600000,
            end_lon=-1224500000,
            mode=TravelMode.WALK,
        )
        encoded = _encode_payload(MessageType.MAP_ROUTE_REQUEST, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure

        decoded = _decode_payload(MessageType.MAP_ROUTE_REQUEST, encoded)
        assert decoded.start_lat == 478563210
        assert decoded.start_lon == -1224567890
        assert decoded.mode == TravelMode.WALK

    def test_route_response_vector(self):
        """MAP_ROUTE_RESPONSE with points, instructions, and summary."""
        expected_structure = {
            "points": [478563210, -1224567890, 478600000, -1224500000],
            "instructions": [
                {"distance_m": 150, "maneuver": "straight", "street": "Main St", "bearing": 0},
                {"distance_m": 0, "maneuver": "arrive", "street": "", "bearing": 0},
            ],
            "total_distance_m": 150,
            "total_time_s": 120,
        }

        payload = MapRouteResponsePayload(
            points=[478563210, -1224567890, 478600000, -1224500000],
            instructions=[
                MapRouteInstruction(distance_m=150, maneuver="straight", street="Main St"),
                MapRouteInstruction(distance_m=0, maneuver="arrive", street=""),
            ],
            total_distance_m=150,
            total_time_s=120,
        )
        encoded = _encode_payload(MessageType.MAP_ROUTE_RESPONSE, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw == expected_structure

    def test_route_response_with_error_vector(self):
        """MAP_ROUTE_RESPONSE with error."""
        payload = MapRouteResponsePayload(
            points=[],
            instructions=[],
            total_distance_m=0,
            total_time_s=0,
            error="No route found",
        )
        encoded = _encode_payload(MessageType.MAP_ROUTE_RESPONSE, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw["error"] == "No route found"
        assert decoded_raw["points"] == []

    def test_geocode_request_vector(self):
        """MAP_GEOCODE_REQUEST with query and bias."""
        payload = MapGeocodeRequestPayload(
            query="Portland",
            bias_lat=455123456,
            bias_lon=-1226789012,
            max_results=3,
        )
        encoded = _encode_payload(MessageType.MAP_GEOCODE_REQUEST, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw["query"] == "Portland"
        assert decoded_raw["max_results"] == 3
        assert decoded_raw["bias_lat"] == 455123456
        assert decoded_raw["bias_lon"] == -1226789012

    def test_geocode_response_vector(self):
        """MAP_GEOCODE_RESPONSE with results."""
        expected_results = [
            {"display_name": "Portland, OR, USA", "lat": 455123456, "lon": -1226789012, "type": "city"},
            {"display_name": "Portland, ME, USA", "lat": 436568000, "lon": -702580000, "type": "city"},
        ]

        payload = MapGeocodeResponsePayload(
            query="Portland",
            results=[
                MapGeocodeResult(display_name="Portland, OR, USA", lat=455123456, lon=-1226789012, type="city"),
                MapGeocodeResult(display_name="Portland, ME, USA", lat=436568000, lon=-702580000, type="city"),
            ],
        )
        encoded = _encode_payload(MessageType.MAP_GEOCODE_RESPONSE, payload)

        decoded_raw = msgpack.unpackb(encoded, raw=False)
        assert decoded_raw["query"] == "Portland"
        assert decoded_raw["results"] == expected_results

        decoded = _decode_payload(MessageType.MAP_GEOCODE_RESPONSE, encoded)
        assert len(decoded.results) == 2
        assert decoded.results[0].display_name == "Portland, OR, USA"
        assert decoded.results[0].lat == 455123456


class TestMessageTypeCompatibility:
    """Verify message type enum values match protocol spec."""

    # These values are defined in the protocol and must never change
    PROTOCOL_VALUES = {
        "TRUST_OFFER": 0x01,
        "TRUST_ACCEPT": 0x02,
        "TRUST_REVOKE": 0x03,
        "NTP_REQUEST": 0x10,
        "NTP_RESPONSE": 0x11,
        "SEARCH_REQUEST": 0x20,
        "SEARCH_RESPONSE": 0x21,
        "MAP_TILE_REQUEST": 0x30,
        "MAP_TILE_RESPONSE": 0x31,
        "MAP_ROUTE_REQUEST": 0x33,
        "MAP_ROUTE_RESPONSE": 0x34,
        "MAP_GEOCODE_REQUEST": 0x35,
        "MAP_GEOCODE_RESPONSE": 0x36,
    }

    def test_all_message_types_match_protocol(self):
        """All message types must have the correct protocol-defined values."""
        assert MessageType.TRUST_OFFER == self.PROTOCOL_VALUES["TRUST_OFFER"]
        assert MessageType.TRUST_ACCEPT == self.PROTOCOL_VALUES["TRUST_ACCEPT"]
        assert MessageType.TRUST_REVOKE == self.PROTOCOL_VALUES["TRUST_REVOKE"]
        assert MessageType.NTP_REQUEST == self.PROTOCOL_VALUES["NTP_REQUEST"]
        assert MessageType.NTP_RESPONSE == self.PROTOCOL_VALUES["NTP_RESPONSE"]
        assert MessageType.SEARCH_REQUEST == self.PROTOCOL_VALUES["SEARCH_REQUEST"]
        assert MessageType.SEARCH_RESPONSE == self.PROTOCOL_VALUES["SEARCH_RESPONSE"]
        assert MessageType.MAP_TILE_REQUEST == self.PROTOCOL_VALUES["MAP_TILE_REQUEST"]
        assert MessageType.MAP_TILE_RESPONSE == self.PROTOCOL_VALUES["MAP_TILE_RESPONSE"]
        assert MessageType.MAP_ROUTE_REQUEST == self.PROTOCOL_VALUES["MAP_ROUTE_REQUEST"]
        assert MessageType.MAP_ROUTE_RESPONSE == self.PROTOCOL_VALUES["MAP_ROUTE_RESPONSE"]
        assert MessageType.MAP_GEOCODE_REQUEST == self.PROTOCOL_VALUES["MAP_GEOCODE_REQUEST"]
        assert MessageType.MAP_GEOCODE_RESPONSE == self.PROTOCOL_VALUES["MAP_GEOCODE_RESPONSE"]


class TestMsgpackFieldNames:
    """Verify msgpack field names match what C++ expects."""

    def test_trust_offer_field_names(self):
        """TRUST_OFFER must use exactly these field names."""
        payload = TrustOfferPayload(server_name="S", services=["a"])
        encoded = _encode_payload(MessageType.TRUST_OFFER, payload)
        data = msgpack.unpackb(encoded, raw=False)

        # C++ expects these exact keys
        assert "server_name" in data
        assert "services" in data

    def test_trust_accept_field_names(self):
        """TRUST_ACCEPT must use exactly these field names."""
        payload = TrustAcceptPayload(device_name="D")
        encoded = _encode_payload(MessageType.TRUST_ACCEPT, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "device_name" in data

    def test_ntp_request_field_names(self):
        """NTP_REQUEST must use exactly these field names."""
        payload = NTPRequestPayload(client_timestamp=0)
        encoded = _encode_payload(MessageType.NTP_REQUEST, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "client_timestamp" in data

    def test_ntp_response_field_names(self):
        """NTP_RESPONSE must use exactly these field names."""
        payload = NTPResponsePayload(server_timestamp=0, client_timestamp=0)
        encoded = _encode_payload(MessageType.NTP_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "server_timestamp" in data
        assert "client_timestamp" in data

    def test_search_request_field_names(self):
        """SEARCH_REQUEST must use exactly these field names."""
        payload = SearchRequestPayload(query="q", max_results=1)
        encoded = _encode_payload(MessageType.SEARCH_REQUEST, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "query" in data
        assert "max_results" in data

    def test_search_response_field_names(self):
        """SEARCH_RESPONSE must use exactly these field names."""
        payload = SearchResponsePayload(query="q", results=[], error=None)
        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "query" in data
        assert "results" in data
        assert "error" in data

    def test_search_result_field_names(self):
        """Search results must use exactly these field names."""
        payload = SearchResponsePayload(
            query="q",
            results=[SearchResult(title="T", url="U", snippet="S")],
        )
        encoded = _encode_payload(MessageType.SEARCH_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)

        result = data["results"][0]
        assert "title" in result
        assert "url" in result
        assert "snippet" in result

    def test_route_request_field_names(self):
        """MAP_ROUTE_REQUEST must use exactly these field names."""
        payload = MapRouteRequestPayload(
            start_lat=0, start_lon=0, end_lat=0, end_lon=0,
        )
        encoded = _encode_payload(MessageType.MAP_ROUTE_REQUEST, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "start_lat" in data
        assert "start_lon" in data
        assert "end_lat" in data
        assert "end_lon" in data
        assert "mode" in data

    def test_route_response_field_names(self):
        """MAP_ROUTE_RESPONSE must use exactly these field names."""
        payload = MapRouteResponsePayload(points=[])
        encoded = _encode_payload(MessageType.MAP_ROUTE_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "points" in data
        assert "instructions" in data
        assert "total_distance_m" in data
        assert "total_time_s" in data

    def test_route_instruction_field_names(self):
        """Route instructions must use exactly these field names."""
        payload = MapRouteResponsePayload(
            points=[],
            instructions=[MapRouteInstruction(distance_m=0, maneuver="straight", street="")],
        )
        encoded = _encode_payload(MessageType.MAP_ROUTE_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)

        instruction = data["instructions"][0]
        assert "distance_m" in instruction
        assert "maneuver" in instruction
        assert "street" in instruction

    def test_geocode_request_field_names(self):
        """MAP_GEOCODE_REQUEST must use exactly these field names."""
        payload = MapGeocodeRequestPayload(query="test")
        encoded = _encode_payload(MessageType.MAP_GEOCODE_REQUEST, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "query" in data
        assert "max_results" in data

    def test_geocode_response_field_names(self):
        """MAP_GEOCODE_RESPONSE must use exactly these field names."""
        payload = MapGeocodeResponsePayload(query="test", results=[])
        encoded = _encode_payload(MessageType.MAP_GEOCODE_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)

        assert "query" in data
        assert "results" in data

    def test_geocode_result_field_names(self):
        """Geocode results must use exactly these field names."""
        payload = MapGeocodeResponsePayload(
            query="test",
            results=[MapGeocodeResult(display_name="Place", lat=0, lon=0, type="city")],
        )
        encoded = _encode_payload(MessageType.MAP_GEOCODE_RESPONSE, payload)
        data = msgpack.unpackb(encoded, raw=False)

        result = data["results"][0]
        assert "display_name" in result
        assert "lat" in result
        assert "lon" in result
        assert "type" in result


class TestLXMFFieldsStructure:
    """Test the structure of LXMF fields dict."""

    def test_fields_dict_keys(self):
        """LXMF fields dict must have these exact keys."""
        from companion_server.protocol import encode_service_fields, ServiceMessage

        msg = ServiceMessage(
            msg_type=MessageType.TRUST_OFFER,
            service="trust",
            payload=TrustOfferPayload(server_name="S", services=[]),
            request_id=123,
        )

        fields = encode_service_fields(msg)

        # C++ expects these exact keys in the LXMF fields dict
        assert "msg_type" in fields
        assert "service" in fields
        assert "payload" in fields
        assert "request_id" in fields

    def test_msg_type_is_integer(self):
        """msg_type must be an integer, not enum."""
        from companion_server.protocol import encode_service_fields, ServiceMessage

        msg = ServiceMessage(
            msg_type=MessageType.NTP_REQUEST,
            service="ntp",
            payload=NTPRequestPayload(client_timestamp=0),
            request_id=0,
        )

        fields = encode_service_fields(msg)

        assert isinstance(fields["msg_type"], int)
        assert fields["msg_type"] == 0x10

    def test_payload_is_bytes(self):
        """payload must be bytes (msgpack binary)."""
        from companion_server.protocol import encode_service_fields, ServiceMessage

        msg = ServiceMessage(
            msg_type=MessageType.TRUST_ACCEPT,
            service="trust",
            payload=TrustAcceptPayload(device_name="D"),
            request_id=0,
        )

        fields = encode_service_fields(msg)

        assert isinstance(fields["payload"], bytes)


class TestDecodeFromCppVectors:
    """Test decoding data that C++ would produce.

    These tests use manually constructed msgpack data to simulate
    what the C++ implementation would produce.
    """

    def test_decode_cpp_trust_offer(self):
        """Decode TRUST_OFFER as C++ would encode it."""
        # Simulate C++ encoded data
        cpp_data = msgpack.packb({
            "server_name": "CppServer",
            "services": ["ntp", "search"],
        }, use_bin_type=True)

        decoded = _decode_payload(MessageType.TRUST_OFFER, cpp_data)

        assert decoded.server_name == "CppServer"
        assert decoded.services == ["ntp", "search"]

    def test_decode_cpp_ntp_response(self):
        """Decode NTP_RESPONSE as C++ would encode it."""
        cpp_data = msgpack.packb({
            "server_timestamp": 1706825600,
            "client_timestamp": 12345,
        }, use_bin_type=True)

        decoded = _decode_payload(MessageType.NTP_RESPONSE, cpp_data)

        assert decoded.server_timestamp == 1706825600
        assert decoded.client_timestamp == 12345

    def test_decode_cpp_search_response_with_results(self):
        """Decode SEARCH_RESPONSE with results as C++ would encode it."""
        cpp_data = msgpack.packb({
            "query": "cpp query",
            "results": [
                {"title": "CppResult", "url": "http://cpp.example", "snippet": "From C++"},
            ],
            "error": None,
        }, use_bin_type=True)

        decoded = _decode_payload(MessageType.SEARCH_RESPONSE, cpp_data)

        assert decoded.query == "cpp query"
        assert len(decoded.results) == 1
        assert decoded.results[0].title == "CppResult"

    def test_decode_cpp_route_response(self):
        """Decode MAP_ROUTE_RESPONSE as C++ would encode it."""
        cpp_data = msgpack.packb({
            "points": [478563210, -1224567890, 478600000, -1224500000],
            "instructions": [
                {"distance_m": 150, "maneuver": "straight", "street": "Main St"},
                {"distance_m": 0, "maneuver": "arrive", "street": ""},
            ],
            "total_distance_m": 150,
            "total_time_s": 120,
        }, use_bin_type=True)

        decoded = _decode_payload(MessageType.MAP_ROUTE_RESPONSE, cpp_data)

        assert decoded.points == [478563210, -1224567890, 478600000, -1224500000]
        assert len(decoded.instructions) == 2
        assert decoded.instructions[0].maneuver == "straight"
        assert decoded.instructions[0].street == "Main St"
        assert decoded.instructions[0].distance_m == 150
        assert decoded.instructions[1].maneuver == "arrive"
        assert decoded.total_distance_m == 150
        assert decoded.total_time_s == 120
        assert decoded.error is None

    def test_decode_cpp_route_response_with_error(self):
        """Decode MAP_ROUTE_RESPONSE with error as C++ would encode it."""
        cpp_data = msgpack.packb({
            "points": [],
            "instructions": [],
            "total_distance_m": 0,
            "total_time_s": 0,
            "error": "No route found",
        }, use_bin_type=True)

        decoded = _decode_payload(MessageType.MAP_ROUTE_RESPONSE, cpp_data)

        assert decoded.points == []
        assert len(decoded.instructions) == 0
        assert decoded.error == "No route found"

    def test_decode_cpp_geocode_response(self):
        """Decode MAP_GEOCODE_RESPONSE as C++ would encode it."""
        cpp_data = msgpack.packb({
            "query": "Portland",
            "results": [
                {"display_name": "Portland, OR, USA", "lat": 455123456, "lon": -1226789012, "type": "city"},
            ],
        }, use_bin_type=True)

        decoded = _decode_payload(MessageType.MAP_GEOCODE_RESPONSE, cpp_data)

        assert decoded.query == "Portland"
        assert len(decoded.results) == 1
        assert decoded.results[0].display_name == "Portland, OR, USA"
        assert decoded.results[0].lat == 455123456
        assert decoded.results[0].lon == -1226789012
        assert decoded.results[0].type == "city"

    def test_decode_cpp_route_request(self):
        """Decode MAP_ROUTE_REQUEST as C++ would encode it."""
        cpp_data = msgpack.packb({
            "start_lat": 478563210,
            "start_lon": -1224567890,
            "end_lat": 478600000,
            "end_lon": -1224500000,
            "mode": 1,  # BIKE
        }, use_bin_type=True)

        decoded = _decode_payload(MessageType.MAP_ROUTE_REQUEST, cpp_data)

        assert decoded.start_lat == 478563210
        assert decoded.start_lon == -1224567890
        assert decoded.end_lat == 478600000
        assert decoded.end_lon == -1224500000
        assert decoded.mode == TravelMode.BIKE


class TestNTPWorkflowCompatibility:
    """Tests specifically for NTP request/response workflow compatibility.

    These tests verify that the full NTP workflow works correctly between
    Python (companion server) and C++ (rDeck) implementations.
    """

    def test_ntp_request_payload_from_cpp_style(self):
        """Test decoding NTP request as C++ would send it (millis-based timestamp)."""
        # C++ sends client_timestamp as millis() value (32-bit, wraps around)
        cpp_client_timestamp = 123456789  # Typical millis() value

        cpp_data = msgpack.packb({
            "client_timestamp": cpp_client_timestamp,
        }, use_bin_type=True)

        decoded = _decode_payload(MessageType.NTP_REQUEST, cpp_data)

        assert decoded.client_timestamp == cpp_client_timestamp

    def test_ntp_response_payload_for_cpp(self):
        """Test encoding NTP response as C++ expects to receive it."""
        import time

        # Server creates response with epoch timestamp
        server_time = int(time.time())
        client_time = 123456789  # Echo back client's millis value

        payload = NTPResponsePayload(
            server_timestamp=server_time,
            client_timestamp=client_time,
        )
        encoded = _encode_payload(MessageType.NTP_RESPONSE, payload)

        # Verify C++ can decode this structure
        decoded_raw = msgpack.unpackb(encoded, raw=False)

        assert "server_timestamp" in decoded_raw
        assert "client_timestamp" in decoded_raw
        assert decoded_raw["server_timestamp"] == server_time
        assert decoded_raw["client_timestamp"] == client_time

    def test_ntp_round_trip_simulation(self):
        """Simulate full NTP request/response round trip."""
        # 1. C++ sends NTP_REQUEST
        client_millis = 50000  # 50 seconds since boot

        request_payload = NTPRequestPayload(client_timestamp=client_millis)
        request_encoded = _encode_payload(MessageType.NTP_REQUEST, request_payload)

        # 2. Python receives and decodes request
        request_decoded = _decode_payload(MessageType.NTP_REQUEST, request_encoded)
        assert request_decoded.client_timestamp == client_millis

        # 3. Python creates response
        import time
        server_time = int(time.time())
        response_payload = NTPResponsePayload(
            server_timestamp=server_time,
            client_timestamp=request_decoded.client_timestamp,  # Echo back
        )
        response_encoded = _encode_payload(MessageType.NTP_RESPONSE, response_payload)

        # 4. C++ would decode response
        response_decoded = _decode_payload(MessageType.NTP_RESPONSE, response_encoded)

        assert response_decoded.server_timestamp == server_time
        assert response_decoded.client_timestamp == client_millis

    def test_ntp_full_lxmf_message_format(self):
        """Test full LXMF message format for NTP response as Python sends it."""
        from companion_server.protocol import encode_service_fields, ServiceMessage

        # Create NTP response
        server_time = 1706825600  # Fixed timestamp for testing
        client_time = 12345

        payload = NTPResponsePayload(
            server_timestamp=server_time,
            client_timestamp=client_time,
        )
        msg = ServiceMessage(
            msg_type=MessageType.NTP_RESPONSE,
            service="ntp",
            payload=payload,
            request_id=100,
        )

        # Encode to LXMF fields
        fields = encode_service_fields(msg)

        # Create full LXMF packed payload: [timestamp, title, content, fields]
        lxmf_payload = [
            1706825600,  # LXMF timestamp
            b"",         # empty title
            b"",         # empty content
            fields,      # service fields
        ]

        # Pack as msgpack (this is what goes over the wire)
        packed = msgpack.packb(lxmf_payload, use_bin_type=True)

        # Verify C++ can parse this:
        unpacked = msgpack.unpackb(packed, raw=False)

        # Check structure
        assert isinstance(unpacked, list)
        assert len(unpacked) >= 4

        extracted_fields = unpacked[3]
        assert isinstance(extracted_fields, dict)

        # Verify service message markers
        assert extracted_fields["msg_type"] == MessageType.NTP_RESPONSE.value
        assert extracted_fields["service"] == "ntp"
        assert extracted_fields["request_id"] == 100

        # Verify inner payload
        inner_data = msgpack.unpackb(extracted_fields["payload"], raw=False)
        assert inner_data["server_timestamp"] == server_time
        assert inner_data["client_timestamp"] == client_time


class TestLXMFMessageFormat:
    """Test the full LXMF message format as it would be transmitted over the wire.

    LXMF packed payload format: [timestamp, title_bytes, content_bytes, fields_dict]
    This matches what the C++ onLinkPacket receives.
    """

    def test_lxmf_trust_offer_format(self):
        """Create LXMF message with TRUST_OFFER fields as Python would send."""
        from companion_server.protocol import encode_service_fields, ServiceMessage

        # Create the service message
        payload = TrustOfferPayload(
            server_name="TestCompanionServer",
            services=["ntp", "search"],
        )
        msg = ServiceMessage(
            msg_type=MessageType.TRUST_OFFER,
            service="trust",
            payload=payload,
            request_id=12345,
        )

        # Encode to LXMF fields
        fields = encode_service_fields(msg)

        # Create full LXMF packed payload: [timestamp, title, content, fields]
        lxmf_payload = [
            1706825600,  # timestamp
            b"",         # empty title
            b"",         # empty content
            fields,      # service fields
        ]

        # Pack as msgpack (this is what goes over the wire)
        packed = msgpack.packb(lxmf_payload, use_bin_type=True)

        # Now verify C++ would be able to parse this:
        # 1. Unpack the outer array
        unpacked = msgpack.unpackb(packed, raw=False)
        assert isinstance(unpacked, list)
        assert len(unpacked) >= 4

        # 2. Extract fields (index 3)
        extracted_fields = unpacked[3]
        assert isinstance(extracted_fields, dict)

        # 3. Check service message markers
        assert "msg_type" in extracted_fields
        assert "service" in extracted_fields
        assert extracted_fields["msg_type"] == 0x01
        assert extracted_fields["service"] == "trust"

        # 4. Extract and decode inner payload
        inner_payload = extracted_fields["payload"]
        assert isinstance(inner_payload, bytes)

        inner_data = msgpack.unpackb(inner_payload, raw=False)
        assert inner_data["server_name"] == "TestCompanionServer"
        assert inner_data["services"] == ["ntp", "search"]

    def test_lxmf_ntp_response_format(self):
        """Create LXMF message with NTP_RESPONSE fields."""
        from companion_server.protocol import encode_service_fields, ServiceMessage

        payload = NTPResponsePayload(
            server_timestamp=1706825600,
            client_timestamp=5000,
        )
        msg = ServiceMessage(
            msg_type=MessageType.NTP_RESPONSE,
            service="ntp",
            payload=payload,
            request_id=42,
        )

        fields = encode_service_fields(msg)
        lxmf_payload = [1706825600, b"", b"", fields]
        packed = msgpack.packb(lxmf_payload, use_bin_type=True)

        # Verify structure
        unpacked = msgpack.unpackb(packed, raw=False)
        extracted_fields = unpacked[3]

        assert extracted_fields["msg_type"] == 0x11
        assert extracted_fields["service"] == "ntp"
        assert extracted_fields["request_id"] == 42

        inner_data = msgpack.unpackb(extracted_fields["payload"], raw=False)
        assert inner_data["server_timestamp"] == 1706825600
        assert inner_data["client_timestamp"] == 5000

    def test_lxmf_regular_message_no_service_fields(self):
        """Verify regular LXMF messages (not service messages) have null fields."""
        # Regular chat message format
        lxmf_payload = [
            1706825600,           # timestamp
            b"Hello",             # title
            b"Message content",   # content
            None,                 # no fields
        ]

        packed = msgpack.packb(lxmf_payload, use_bin_type=True)
        unpacked = msgpack.unpackb(packed, raw=False)

        assert unpacked[3] is None

    def test_lxmf_search_response_format(self):
        """Create LXMF message with SEARCH_RESPONSE fields."""
        from companion_server.protocol import encode_service_fields, ServiceMessage

        payload = SearchResponsePayload(
            query="test query",
            results=[
                SearchResult(title="Result 1", url="https://example.com/1", snippet="First result"),
                SearchResult(title="Result 2", url="https://example.com/2", snippet="Second result"),
            ],
            error=None,
        )
        msg = ServiceMessage(
            msg_type=MessageType.SEARCH_RESPONSE,
            service="search",
            payload=payload,
            request_id=999,
        )

        fields = encode_service_fields(msg)
        lxmf_payload = [1706825600, b"", b"", fields]
        packed = msgpack.packb(lxmf_payload, use_bin_type=True)

        # Verify structure
        unpacked = msgpack.unpackb(packed, raw=False)
        extracted_fields = unpacked[3]

        assert extracted_fields["msg_type"] == 0x21
        assert extracted_fields["service"] == "search"

        inner_data = msgpack.unpackb(extracted_fields["payload"], raw=False)
        assert inner_data["query"] == "test query"
        assert len(inner_data["results"]) == 2
        assert inner_data["results"][0]["title"] == "Result 1"

    def test_lxmf_route_response_format(self):
        """Create LXMF message with MAP_ROUTE_RESPONSE fields."""
        from companion_server.protocol import encode_service_fields, ServiceMessage

        payload = MapRouteResponsePayload(
            points=[478563210, -1224567890, 478580000, -1224530000, 478600000, -1224500000],
            instructions=[
                MapRouteInstruction(distance_m=150, maneuver="straight", street="Main St"),
                MapRouteInstruction(distance_m=200, maneuver="turn-left", street="Oak Ave"),
                MapRouteInstruction(distance_m=0, maneuver="arrive", street=""),
            ],
            total_distance_m=350,
            total_time_s=240,
        )
        msg = ServiceMessage(
            msg_type=MessageType.MAP_ROUTE_RESPONSE,
            service="maps",
            payload=payload,
            request_id=500,
        )

        fields = encode_service_fields(msg)
        lxmf_payload = [1706825600, b"", b"", fields]
        packed = msgpack.packb(lxmf_payload, use_bin_type=True)

        # Verify structure
        unpacked = msgpack.unpackb(packed, raw=False)
        extracted_fields = unpacked[3]

        assert extracted_fields["msg_type"] == 0x34
        assert extracted_fields["service"] == "maps"
        assert extracted_fields["request_id"] == 500

        inner_data = msgpack.unpackb(extracted_fields["payload"], raw=False)
        assert len(inner_data["points"]) == 6
        assert inner_data["points"][0] == 478563210
        assert len(inner_data["instructions"]) == 3
        assert inner_data["instructions"][0]["maneuver"] == "straight"
        assert inner_data["instructions"][0]["street"] == "Main St"
        assert inner_data["total_distance_m"] == 350
        assert inner_data["total_time_s"] == 240
