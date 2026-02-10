#pragma once
#include "BaseApp.h"
#include "services/RnsUtils/ServiceProtocol.h"
#include <vector>
#include <map>
#include <deque>

/**
 * Maps App - Offline maps via companion server
 *
 * Displays map tiles from MBTiles served by a companion server.
 * Features:
 * - Map tile display with pan/zoom
 * - GPS position tracking
 * - Two-tier tile cache (RAM + disk)
 * - Keyboard controls for navigation
 * - Address search (geocoding)
 * - Route planning
 */
class Maps : public BaseApp {
public:
    using BaseApp::BaseApp;

    virtual void start(RetOS* retos) override;
    virtual void tick(const unsigned long tickMillis) override;
    virtual void stop() override;
    virtual EventStatus onEvent(const Event& event) override;

    // Public methods for keyboard callbacks
    void pan(int dx, int dy);
    void zoom(int delta);
    void centerOnGps();
    void toggleFollowGps();
    void startSearch();
    void performSearch(const std::string& query);
    void goToLocation(int32_t lat, int32_t lon);

    // Public access to members needed by callbacks
    lv_obj_t* getSearchOverlay() { return _search_overlay; }
    lv_obj_t* getSearchInput() { return _search_input; }
    std::vector<Retcon::Service::MapGeocodeResult>& getSearchResults() { return _searchResults; }
    bool& searchMode() { return _searchMode; }

protected:
    // Drawing
    void drawUI();
    void renderMap();
    void drawTile(int screenX, int screenY, const std::vector<uint8_t>& tileData);
    void drawGpsMarker();
    void drawStatusBar();
    void clearCanvas();

    // Tile management
    struct TileKey {
        uint8_t z;
        uint32_t x;
        uint32_t y;
        bool operator<(const TileKey& other) const;
        bool operator==(const TileKey& other) const;
    };

    struct CachedTile {
        std::vector<uint8_t> data;  // Decoded 1-bit tile (2KB for 128x128)
        unsigned long lastAccess;
    };

    void requestTile(uint8_t z, uint32_t x, uint32_t y);
    void requestVisibleTiles();
    bool getTileFromCache(const TileKey& key, CachedTile& tile);
    void addTileToCache(const TileKey& key, const std::vector<uint8_t>& data);
    std::vector<uint8_t> decompressRle(const std::vector<uint8_t>& rleData);
    void evictOldestTile();
    std::string getTileCachePath(const TileKey& key);
    bool loadTileFromDisk(const TileKey& key, CachedTile& tile);
    void saveTileToDisk(const TileKey& key, const std::vector<uint8_t>& data);

    // Coordinate conversion
    struct LatLon {
        double lat;
        double lon;
    };
    static void latLonToTile(double lat, double lon, uint8_t z, uint32_t& tileX, uint32_t& tileY);
    static void tileToLatLon(uint32_t tileX, uint32_t tileY, uint8_t z, double& lat, double& lon);
    static void latLonToPixel(double lat, double lon, uint8_t z, int& pixelX, int& pixelY);

    // Search
    void displaySearchResults(const Retcon::Service::MapGeocodeResponsePayload& results);

    // Route
    void startRouting();
    void calculateRoute();
    void displayRoute(const Retcon::Service::MapRouteResponsePayload& route);
    void clearRoute();

    // UI elements
    lv_obj_t* _map_canvas = nullptr;
    lv_obj_t* _status_bar = nullptr;
    lv_obj_t* _zoom_label = nullptr;
    lv_obj_t* _coords_label = nullptr;
    lv_obj_t* _search_overlay = nullptr;
    lv_obj_t* _search_input = nullptr;
    lv_obj_t* _search_results_list = nullptr;
    lv_obj_t* _loading_spinner = nullptr;
    lv_obj_t* _error_label = nullptr;

    // Canvas buffer (240x280 = 67200 pixels, but we use 1-bit = 8400 bytes for mono)
    static const int CANVAS_WIDTH = 240;
    static const int CANVAS_HEIGHT = 280;
    static const int TILE_SIZE = 128;  // 128x128 tiles from server

    // Map state
    uint8_t _zoom = 14;  // Current zoom level (10-18)
    double _centerLat = 41.8781;  // Default: Chicago
    double _centerLon = -87.6298;
    int _pixelOffsetX = 0;  // Sub-tile pixel offset for smooth panning
    int _pixelOffsetY = 0;

    // GPS state
    bool _hasGps = false;
    double _gpsLat = 0;
    double _gpsLon = 0;
    bool _followGps = false;  // Auto-center on GPS

    // Tile cache (RAM)
    std::map<TileKey, CachedTile> _tileCache;
    std::deque<TileKey> _tileLruOrder;
    static const size_t MAX_CACHED_TILES = 16;

    // Pending tile requests
    std::map<TileKey, uint32_t> _pendingTileRequests;  // key -> request_id
    static const size_t MAX_PENDING_REQUESTS = 9;

    // Route data
    std::vector<int32_t> _routePoints;  // lat/lon pairs * 1e7
    bool _hasRoute = false;

    // Search results
    std::vector<Retcon::Service::MapGeocodeResult> _searchResults;
    bool _searchMode = false;

    // Routing mode
    bool _routeMode = false;
    LatLon _routeStart;
    LatLon _routeEnd;
    bool _hasRouteStart = false;

    // Server hash for requests
    RNS::Bytes _mapsServerHash;
    bool findMapsServer();

    // Error display
    void showError(const char* msg);
    void clearError();

    // Timing
    unsigned long _lastTileRequest = 0;
    static const unsigned long TILE_REQUEST_THROTTLE_MS = 100;
    static const uint32_t TILE_REQUEST_TIMEOUT_MS = 15000;  // 15s timeout for pending requests

    // Zoom limits
    static const uint8_t MIN_ZOOM = 10;
    static const uint8_t MAX_ZOOM = 18;
};
