#include "Maps.h"
#include "services/RnsService.h"
#include "services/PositionService.h"
#include "services/RnsUtils/TrustedServers.h"
#include "lvgl.h"
#include <cmath>
#include <algorithm>

// Keyboard input handling
static void mapKeyCallback(lv_event_t* e);
static void searchInputCallback(lv_event_t* e);
static void searchResultCallback(lv_event_t* e);
static void zoomInCallback(lv_event_t* e);
static void zoomOutCallback(lv_event_t* e);
static void searchBtnCallback(lv_event_t* e);
static void searchRetryCallback(lv_event_t* e);

// TileKey comparison operators
bool Maps::TileKey::operator<(const TileKey& other) const {
    if (z != other.z) return z < other.z;
    if (x != other.x) return x < other.x;
    return y < other.y;
}

bool Maps::TileKey::operator==(const TileKey& other) const {
    return z == other.z && x == other.x && y == other.y;
}

void Maps::start(RetOS* retos) {
    _keep_awake = true;
    drawUI();

    // Query PositionService for initial center
    PositionService* pos = _retos->fetchService<PositionService>();
    if (pos && pos->hasValidPosition()) {
        _centerLat = pos->getLatitude();
        _centerLon = pos->getLongitude();
        _gpsLat = _centerLat;
        _gpsLon = _centerLon;
        _hasGps = true;
        if (pos->hasValidHeading()) {
            _gpsHeading = pos->getHeading();
            _hasHeading = true;
        }
    }

    // Check if RnsService is available and fully initialized
    RnsService* rns = _retos->fetchService<RnsService>();
    if (!rns || rns->status() != RUNNING) {
        Serial.println("[Maps] RnsService not ready yet");
        showError("Waiting for network service...");
        return;
    }

    // Find a trusted server with maps capability
    if (!findMapsServer()) {
        showError("No trusted maps server found.\nAdd one in Settings > Trusted Servers.");
    }

    // Render any cached tiles immediately
    renderMap();

    // Request any missing tiles from server
    requestVisibleTiles();
}

void Maps::tick(const unsigned long tickMillis) {
    // Retry finding server if we don't have one yet
    if (_mapsServerHash.size() == 0) {
        // Retry every 2 seconds instead of every tick
        if (tickMillis - _lastTileRequest > 2000) {
            if (findMapsServer()) {
                clearError();
                renderMap();
            }
            _lastTileRequest = tickMillis;
        }
        return;
    }

    // Expire stale pending requests so failed sends don't permanently block new ones
    if (!_pendingTileRequests.empty()) {
        std::vector<TileKey> expired;
        for (const auto& [key, requestId] : _pendingTileRequests) {
            // requestId is millis() & 0xFFFFFFFF at queue time
            uint32_t age = (uint32_t)(tickMillis & 0xFFFFFFFF) - requestId;
            if (age > TILE_REQUEST_TIMEOUT_MS) {
                expired.push_back(key);
            }
        }
        for (const auto& key : expired) {
            _pendingTileRequests.erase(key);
        }
        if (!expired.empty() && _pendingTileRequests.empty()) {
            lv_obj_add_flag(_loading_spinner, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Request tiles if we have pending area to fill
    if (tickMillis - _lastTileRequest > TILE_REQUEST_THROTTLE_MS) {
        requestVisibleTiles();
        _lastTileRequest = tickMillis;
    }

    // Check search timeout
    if (_searchPending) {
        unsigned long elapsed = tickMillis - _searchStartTime;
        if (elapsed >= SEARCH_TIMEOUT_MS) {
            displaySearchTimeout();
        } else if (elapsed >= 5000) {
            // Show elapsed time after 5 seconds
            lv_label_set_text_fmt(_search_status_label, "Searching... (%lus)", elapsed / 1000);
            lv_obj_clear_flag(_search_status_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void Maps::stop() {
    _tileCache.clear();
    _tileLruOrder.clear();
    _pendingTileRequests.clear();
    _routePoints.clear();
    _searchResults.clear();
    _hasSearchPin = false;
    _searchPinName.clear();
    _searchPending = false;
}

void Maps::drawUI() {
    // Main container
    lv_obj_t* main = lv_obj_create(screen);
    lv_obj_set_size(main, LV_PCT(100), LV_PCT(100));
    lv_obj_align(main, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_pad_all(main, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(main, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(main, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(main, LV_OPA_100, LV_PART_MAIN);
    lv_obj_clear_flag(main, LV_OBJ_FLAG_SCROLLABLE);

    // Map canvas - monochrome buffer
    _map_canvas = lv_canvas_create(main);
    static lv_color_t cbuf[LV_CANVAS_BUF_SIZE_INDEXED_1BIT(CANVAS_WIDTH, CANVAS_HEIGHT)];
    lv_canvas_set_buffer(_map_canvas, cbuf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_INDEXED_1BIT);
    lv_obj_align(_map_canvas, LV_ALIGN_TOP_LEFT, 0, 0);

    // Set palette for 1-bit canvas
    // LVGL indexed 1-bit uses c.full & 0x1 as the palette index:
    //   lv_color_black().full = 0 → bit 0 → palette[0]
    //   lv_color_white().full != 0 → bit 1 → palette[1]
    lv_canvas_set_palette(_map_canvas, 0, lv_color_black());
    lv_canvas_set_palette(_map_canvas, 1, lv_color_white());

    // Clear canvas to white
    clearCanvas();

    // Status bar at bottom
    _status_bar = lv_obj_create(main);
    lv_obj_set_size(_status_bar, CANVAS_WIDTH, 20);
    lv_obj_align(_status_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_pad_all(_status_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_status_bar, lv_color_hex(0xEEEEEE), LV_PART_MAIN);
    lv_obj_set_flex_flow(_status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_status_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    _zoom_label = lv_label_create(_status_bar);
    lv_label_set_text_fmt(_zoom_label, "Z%d", _zoom);
    lv_obj_set_style_text_font(_zoom_label, &lv_font_montserrat_14, LV_PART_MAIN);

    _search_hint_label = lv_label_create(_status_bar);
    lv_label_set_text(_search_hint_label, "/ Srch");
    lv_obj_set_style_text_font(_search_hint_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_search_hint_label, lv_color_hex(0x555555), LV_PART_MAIN);

    _coords_label = lv_label_create(_status_bar);
    lv_label_set_text_fmt(_coords_label, "%.4f, %.4f", _centerLat, _centerLon);
    lv_obj_set_style_text_font(_coords_label, &lv_font_montserrat_14, LV_PART_MAIN);

    // Loading spinner (hidden by default)
    _loading_spinner = lv_spinner_create(main, 1000, 60);
    lv_obj_set_size(_loading_spinner, 30, 30);
    lv_obj_align(_loading_spinner, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_add_flag(_loading_spinner, LV_OBJ_FLAG_HIDDEN);

    // Error label (hidden by default)
    _error_label = lv_label_create(main);
    lv_obj_set_width(_error_label, CANVAS_WIDTH - 20);
    lv_obj_align(_error_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(_error_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(_error_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(_error_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_error_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_add_flag(_error_label, LV_OBJ_FLAG_HIDDEN);

    // Search overlay (hidden by default)
    _search_overlay = lv_obj_create(main);
    lv_obj_set_size(_search_overlay, LV_PCT(90), LV_PCT(80));
    lv_obj_align(_search_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(_search_overlay, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_search_overlay, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_flex_flow(_search_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(_search_overlay, 5, LV_PART_MAIN);
    lv_obj_add_flag(_search_overlay, LV_OBJ_FLAG_HIDDEN);

    // Search input
    _search_input = lv_textarea_create(_search_overlay);
    lv_obj_set_size(_search_input, LV_PCT(100), LV_SIZE_CONTENT);
    lv_textarea_set_one_line(_search_input, true);
    lv_textarea_set_placeholder_text(_search_input, "Search location...");
    lv_obj_add_event_cb(_search_input, searchInputCallback, LV_EVENT_READY, this);

    // Search results list
    _search_results_list = lv_list_create(_search_overlay);
    lv_obj_set_size(_search_results_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(_search_results_list, 1);

    // Search status label (below results list)
    _search_status_label = lv_label_create(_search_overlay);
    lv_obj_set_width(_search_status_label, LV_PCT(100));
    lv_obj_set_style_text_font(_search_status_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_search_status_label, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_label_set_text(_search_status_label, "");
    lv_obj_add_flag(_search_status_label, LV_OBJ_FLAG_HIDDEN);

    // Search retry button (hidden by default)
    _search_retry_btn = lv_btn_create(_search_overlay);
    lv_obj_set_size(_search_retry_btn, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(_search_retry_btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_color(_search_retry_btn, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(_search_retry_btn, 1, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(_search_retry_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(_search_retry_btn, searchRetryCallback, LV_EVENT_CLICKED, this);
    lv_obj_t* retry_label = lv_label_create(_search_retry_btn);
    lv_label_set_text(retry_label, LV_SYMBOL_REFRESH " Retry");
    lv_obj_set_style_text_color(retry_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_center(retry_label);
    lv_obj_add_flag(_search_retry_btn, LV_OBJ_FLAG_HIDDEN);

    // Zoom buttons (overlaid on map, not on canvas)
    _zoom_in_btn = lv_btn_create(main);
    lv_obj_set_size(_zoom_in_btn, 36, 36);
    lv_obj_align(_zoom_in_btn, LV_ALIGN_BOTTOM_RIGHT, -6, -66);  // Above zoom-out and status bar
    lv_obj_set_style_bg_color(_zoom_in_btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_zoom_in_btn, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_border_color(_zoom_in_btn, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(_zoom_in_btn, 2, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(_zoom_in_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_zoom_in_btn, 4, LV_PART_MAIN);
    lv_obj_add_event_cb(_zoom_in_btn, zoomInCallback, LV_EVENT_CLICKED, this);
    lv_obj_t* plus_label = lv_label_create(_zoom_in_btn);
    lv_label_set_text(plus_label, "+");
    lv_obj_set_style_text_font(plus_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(plus_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_center(plus_label);

    _zoom_out_btn = lv_btn_create(main);
    lv_obj_set_size(_zoom_out_btn, 36, 36);
    lv_obj_align_to(_zoom_out_btn, _zoom_in_btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
    lv_obj_set_style_bg_color(_zoom_out_btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_zoom_out_btn, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_border_color(_zoom_out_btn, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(_zoom_out_btn, 2, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(_zoom_out_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_zoom_out_btn, 4, LV_PART_MAIN);
    lv_obj_add_event_cb(_zoom_out_btn, zoomOutCallback, LV_EVENT_CLICKED, this);
    lv_obj_t* minus_label = lv_label_create(_zoom_out_btn);
    lv_label_set_text(minus_label, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(minus_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(minus_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_center(minus_label);

    // Search button (above zoom buttons)
    _search_btn = lv_btn_create(main);
    lv_obj_set_size(_search_btn, 36, 36);
    lv_obj_align_to(_search_btn, _zoom_in_btn, LV_ALIGN_OUT_TOP_MID, 0, -4);
    lv_obj_set_style_bg_color(_search_btn, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_search_btn, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_border_color(_search_btn, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(_search_btn, 2, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(_search_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_search_btn, 4, LV_PART_MAIN);
    lv_obj_add_event_cb(_search_btn, searchBtnCallback, LV_EVENT_CLICKED, this);
    lv_obj_t* search_icon = lv_label_create(_search_btn);
    lv_label_set_text(search_icon, LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_font(search_icon, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(search_icon, lv_color_black(), LV_PART_MAIN);
    lv_obj_center(search_icon);

    // Register keyboard handler for the main screen
    lv_obj_add_event_cb(main, mapKeyCallback, LV_EVENT_KEY, this);
    lv_group_add_obj(_retos->ui()->default_input_group(), main);
    lv_group_focus_obj(main);
}

static void mapKeyCallback(lv_event_t* e) {
    Maps* app = (Maps*)lv_event_get_user_data(e);
    if (!app) return;

    uint32_t key = lv_event_get_key(e);
    const int PAN_STEP = 32;  // Pixels to pan per keypress

    switch (key) {
        case 'w':
        case 'W':
        case LV_KEY_UP:
            app->pan(0, -PAN_STEP);
            break;
        case 's':
        case 'S':
        case LV_KEY_DOWN:
            app->pan(0, PAN_STEP);
            break;
        case 'a':
        case 'A':
        case LV_KEY_LEFT:
            app->pan(-PAN_STEP, 0);
            break;
        case 'd':
        case 'D':
        case LV_KEY_RIGHT:
            app->pan(PAN_STEP, 0);
            break;
        case '+':
        case '=':
            app->zoom(1);
            break;
        case '-':
        case '_':
            app->zoom(-1);
            break;
        case 'c':
        case 'C':
            app->centerOnGps();
            break;
        case 'f':
        case 'F':
            app->toggleFollowGps();
            break;
        case '/':
            app->startSearch();
            break;
        case LV_KEY_ESC:
            if (app->searchMode()) {
                lv_obj_add_flag(app->getSearchOverlay(), LV_OBJ_FLAG_HIDDEN);
                app->searchMode() = false;
            }
            break;
    }
}

static void searchInputCallback(lv_event_t* e) {
    Maps* app = (Maps*)lv_event_get_user_data(e);
    if (!app) return;

    const char* query = lv_textarea_get_text(app->getSearchInput());
    if (query && strlen(query) > 0) {
        app->performSearch(query);
    }
}

static void searchResultCallback(lv_event_t* e) {
    Maps* app = (Maps*)lv_event_get_user_data(e);
    if (!app) return;

    lv_obj_t* btn = lv_event_get_target(e);
    uint32_t idx = (uint32_t)(uintptr_t)lv_obj_get_user_data(btn);

    auto& results = app->getSearchResults();
    if (idx < results.size()) {
        app->setSearchPinName(results[idx].display_name);
        app->goToLocation(results[idx].lat, results[idx].lon);
        lv_obj_add_flag(app->getSearchOverlay(), LV_OBJ_FLAG_HIDDEN);
        app->searchMode() = false;
    }
}

static void zoomInCallback(lv_event_t* e) {
    Maps* app = (Maps*)lv_event_get_user_data(e);
    if (app) app->zoom(1);
}

static void zoomOutCallback(lv_event_t* e) {
    Maps* app = (Maps*)lv_event_get_user_data(e);
    if (app) app->zoom(-1);
}

static void searchBtnCallback(lv_event_t* e) {
    Maps* app = (Maps*)lv_event_get_user_data(e);
    if (app) app->startSearch();
}

static void searchRetryCallback(lv_event_t* e) {
    Maps* app = (Maps*)lv_event_get_user_data(e);
    if (app) app->retrySearch();
}

void Maps::showError(const char* msg) {
    if (!_error_label) return;
    lv_label_set_text(_error_label, msg);
    lv_obj_clear_flag(_error_label, LV_OBJ_FLAG_HIDDEN);
    if (_map_canvas) lv_obj_add_flag(_map_canvas, LV_OBJ_FLAG_HIDDEN);
}

void Maps::clearError() {
    if (!_error_label) return;
    lv_obj_add_flag(_error_label, LV_OBJ_FLAG_HIDDEN);
    if (_map_canvas) lv_obj_clear_flag(_map_canvas, LV_OBJ_FLAG_HIDDEN);
}

bool Maps::findMapsServer() {
    // Safety: ensure RnsService is fully initialized (it owns TrustedServers state)
    RnsService* rns = _retos->fetchService<RnsService>();
    if (!rns || rns->status() != RUNNING) {
        Serial.println("[Maps] RnsService not ready");
        return false;
    }

    auto& trustedServers = Retcon::Service::getTrustedServers();
    auto servers = trustedServers.getTrustedServers();

    for (const auto& server : servers) {
        for (const auto& svc : server.services) {
            if (svc == "maps") {
                _mapsServerHash = server.hash;
                Serial.printf("[Maps] Found maps server: %s\n", server.name.c_str());
                return true;
            }
        }
    }

    Serial.println("[Maps] No trusted maps server found");
    return false;
}

void Maps::clearCanvas() {
    lv_canvas_fill_bg(_map_canvas, lv_color_white(), LV_OPA_100);
}

void Maps::renderMap() {
    clearCanvas();

    // Calculate which tiles are visible
    uint32_t centerTileX, centerTileY;
    latLonToTile(_centerLat, _centerLon, _zoom, centerTileX, centerTileY);

    // Calculate pixel position within center tile
    int centerPixelX, centerPixelY;
    latLonToPixel(_centerLat, _centerLon, _zoom, centerPixelX, centerPixelY);
    int tilePixelX = centerPixelX % TILE_SIZE;
    int tilePixelY = centerPixelY % TILE_SIZE;

    // Draw tiles in a 3x3 grid around center (enough to cover 240x280 screen)
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            TileKey key = {_zoom, centerTileX + dx, centerTileY + dy};

            // Calculate screen position for this tile
            int screenX = (CANVAS_WIDTH / 2) - tilePixelX + (dx * TILE_SIZE);
            int screenY = (CANVAS_HEIGHT / 2) - tilePixelY + (dy * TILE_SIZE);

            // Check cache
            CachedTile cached;
            if (getTileFromCache(key, cached)) {
                drawTile(screenX, screenY, cached.data);
            }
            // else: tile not cached, will be requested in requestVisibleTiles()
        }
    }

    // Draw GPS marker if we have position
    if (_hasGps) {
        drawGpsMarker();
    }

    // Draw search pin marker if we have one
    if (_hasSearchPin) {
        drawSearchPin();
    }

    // Draw route if we have one
    if (_hasRoute && _routePoints.size() >= 4) {
        // TODO: Draw route polyline
    }

    // Update status bar
    drawStatusBar();

    // Force canvas refresh
    lv_obj_invalidate(_map_canvas);
}

void Maps::drawTile(int screenX, int screenY, const std::vector<uint8_t>& tileData) {
    // tileData is 2048 bytes (128x128 pixels, 1 bit per pixel, 8 pixels per byte)
    if (tileData.size() != (TILE_SIZE * TILE_SIZE / 8)) {
        Serial.printf("[Maps] Invalid tile data size: %zu (expected %d)\n", tileData.size(), TILE_SIZE * TILE_SIZE / 8);
        return;
    }

    // Draw each pixel of the tile to the canvas
    for (int ty = 0; ty < TILE_SIZE; ty++) {
        int cy = screenY + ty;
        if (cy < 0 || cy >= CANVAS_HEIGHT - 20) continue;  // -20 for status bar

        for (int tx = 0; tx < TILE_SIZE; tx++) {
            int cx = screenX + tx;
            if (cx < 0 || cx >= CANVAS_WIDTH) continue;

            // Get pixel from packed bit data (1=white, 0=black per Python convention)
            int bitIndex = ty * TILE_SIZE + tx;
            int byteIndex = bitIndex / 8;
            int bitOffset = 7 - (bitIndex % 8);  // MSB first
            bool isWhite = (tileData[byteIndex] >> bitOffset) & 1;

            // Set pixel on canvas
            lv_color_t color = isWhite ? lv_color_white() : lv_color_black();
            lv_canvas_set_px_color(_map_canvas, cx, cy, color);
        }
    }
}

void Maps::drawGpsMarker() {
    // Calculate screen position of GPS
    int gpsPixelX, gpsPixelY;
    latLonToPixel(_gpsLat, _gpsLon, _zoom, gpsPixelX, gpsPixelY);

    int centerPixelX, centerPixelY;
    latLonToPixel(_centerLat, _centerLon, _zoom, centerPixelX, centerPixelY);

    int cx = (CANVAS_WIDTH / 2) + (gpsPixelX - centerPixelX);
    int cy = (CANVAS_HEIGHT / 2) + (gpsPixelY - centerPixelY);

    // Check if marker is on-screen
    if (cx < -10 || cx >= CANVAS_WIDTH + 10 || cy < -10 || cy >= CANVAS_HEIGHT - 10) return;

    // Helper lambda to set a pixel with bounds checking
    auto setpx = [&](int x, int y, lv_color_t c) {
        if (x >= 0 && x < CANVAS_WIDTH && y >= 0 && y < CANVAS_HEIGHT - 20) {
            lv_canvas_set_px_color(_map_canvas, x, y, c);
        }
    };

    if (_hasHeading) {
        // Draw heading arrow: filled triangle rotated by heading
        // Triangle vertices relative to center: tip(0,-8), left-tail(-5,6), right-tail(5,6)
        float rad = _gpsHeading * M_PI / 180.0f;
        float cosA = cosf(rad);
        float sinA = sinf(rad);

        // Rotate vertices
        struct Pt { float x, y; };
        Pt verts[3] = {
            { 0 * cosA - (-8) * sinA,  0 * sinA + (-8) * cosA },   // tip
            { (-5) * cosA - 6 * sinA,  (-5) * sinA + 6 * cosA },   // left-tail
            { 5 * cosA - 6 * sinA,     5 * sinA + 6 * cosA },      // right-tail
        };

        // Scanline triangle fill: sort vertices by Y
        Pt sorted[3] = { verts[0], verts[1], verts[2] };
        if (sorted[0].y > sorted[1].y) std::swap(sorted[0], sorted[1]);
        if (sorted[1].y > sorted[2].y) std::swap(sorted[1], sorted[2]);
        if (sorted[0].y > sorted[1].y) std::swap(sorted[0], sorted[1]);

        int yMin = (int)floorf(sorted[0].y);
        int yMid = (int)floorf(sorted[1].y);
        int yMax = (int)ceilf(sorted[2].y);

        for (int sy = yMin; sy <= yMax; sy++) {
            float t = (float)sy;
            float xLeft, xRight;

            // Interpolate X along edges
            auto interpX = [](Pt a, Pt b, float y) -> float {
                if (fabsf(b.y - a.y) < 0.001f) return a.x;
                return a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y);
            };

            // Long edge: sorted[0] to sorted[2]
            float xLong = interpX(sorted[0], sorted[2], t);

            if (sy <= yMid) {
                // Top half: sorted[0] to sorted[1]
                float xShort = interpX(sorted[0], sorted[1], t);
                xLeft = fminf(xLong, xShort);
                xRight = fmaxf(xLong, xShort);
            } else {
                // Bottom half: sorted[1] to sorted[2]
                float xShort = interpX(sorted[1], sorted[2], t);
                xLeft = fminf(xLong, xShort);
                xRight = fmaxf(xLong, xShort);
            }

            for (int sx = (int)floorf(xLeft); sx <= (int)ceilf(xRight); sx++) {
                setpx(cx + sx, cy + sy, lv_color_black());
            }
        }

        // White center pixel for contrast
        setpx(cx, cy, lv_color_white());
    } else {
        // No heading: draw filled circle (radius 4) with white center
        const int R = 4;
        for (int dy = -R; dy <= R; dy++) {
            for (int dx = -R; dx <= R; dx++) {
                if (dx * dx + dy * dy <= R * R) {
                    setpx(cx + dx, cy + dy, lv_color_black());
                }
            }
        }
        // White center dot
        setpx(cx, cy, lv_color_white());
        setpx(cx - 1, cy, lv_color_white());
        setpx(cx + 1, cy, lv_color_white());
        setpx(cx, cy - 1, lv_color_white());
        setpx(cx, cy + 1, lv_color_white());
    }
}

void Maps::drawSearchPin() {
    // Calculate screen position of the pin
    int pinPixelX, pinPixelY;
    latLonToPixel(_searchPinLat / 1e7, _searchPinLon / 1e7, _zoom, pinPixelX, pinPixelY);

    int centerPixelX, centerPixelY;
    latLonToPixel(_centerLat, _centerLon, _zoom, centerPixelX, centerPixelY);

    int cx = (CANVAS_WIDTH / 2) + (pinPixelX - centerPixelX);
    int cy = (CANVAS_HEIGHT / 2) + (pinPixelY - centerPixelY);

    // Check if pin is on-screen
    if (cx < -12 || cx >= CANVAS_WIDTH + 12 || cy < -12 || cy >= CANVAS_HEIGHT - 8) return;

    // Helper lambda to set a pixel with bounds checking
    auto setpx = [&](int x, int y, lv_color_t c) {
        if (x >= 0 && x < CANVAS_WIDTH && y >= 0 && y < CANVAS_HEIGHT - 20) {
            lv_canvas_set_px_color(_map_canvas, x, y, c);
        }
    };

    // Draw hollow diamond shape (tip at bottom pointing at coordinate)
    // Diamond: 11px tall, 11px wide at widest
    // The tip (bottom vertex) is at the exact coordinate (cx, cy)
    const int R = 5;  // half-size of diamond
    for (int dy = -2 * R; dy <= 0; dy++) {
        // Diamond width at this row
        int halfW = (R * (2 * R + dy)) / (2 * R);  // narrows toward bottom
        if (dy == 0) halfW = 0;  // tip is single pixel
        for (int dx = -halfW; dx <= halfW; dx++) {
            // Outline: draw only border pixels
            bool isEdge = (dx == -halfW || dx == halfW ||
                          dy == -2 * R || dy == 0);
            if (isEdge) {
                setpx(cx + dx, cy + dy, lv_color_black());
            } else {
                setpx(cx + dx, cy + dy, lv_color_white());
            }
        }
    }

    // Black center dot
    setpx(cx, cy - R, lv_color_black());
    setpx(cx - 1, cy - R, lv_color_black());
    setpx(cx + 1, cy - R, lv_color_black());
    setpx(cx, cy - R - 1, lv_color_black());
    setpx(cx, cy - R + 1, lv_color_black());
}

void Maps::drawStatusBar() {
    lv_label_set_text_fmt(_zoom_label, "Z%d%s", _zoom, _followGps ? " [F]" : "");

    if (_hasSearchPin && !_searchPinName.empty()) {
        // Show truncated pin name instead of coordinates
        std::string display = _searchPinName;
        if (display.length() > 20) {
            display = display.substr(0, 17) + "...";
        }
        lv_label_set_text(_coords_label, display.c_str());
    } else {
        lv_label_set_text_fmt(_coords_label, "%.4f, %.4f", _centerLat, _centerLon);
    }
}

void Maps::requestTile(uint8_t z, uint32_t x, uint32_t y) {
    TileKey key = {z, x, y};

    // Don't request if already pending or cached
    if (_pendingTileRequests.count(key) > 0) return;

    CachedTile cached;
    if (getTileFromCache(key, cached)) return;

    // Check pending request limit
    if (_pendingTileRequests.size() >= MAX_PENDING_REQUESTS) return;

    if (_mapsServerHash.size() == 0) {
        if (!findMapsServer()) return;
    }

    RnsService* rns = _retos->fetchService<RnsService>();
    if (!rns || rns->status() != RUNNING) return;

    // Show loading indicator
    lv_obj_clear_flag(_loading_spinner, LV_OBJ_FLAG_HIDDEN);

    // Generate request ID for tracking
    uint32_t requestId = (uint32_t)(millis() & 0xFFFFFFFF);
    _pendingTileRequests[key] = requestId;

    RNS::Bytes serverHash = _mapsServerHash;
    rns->queueAction([rns, serverHash, z, x, y]() {
        rns->requestMapTile(serverHash, z, x, y, Retcon::Service::TileFormat::MONO_RLE);
    });
}

void Maps::requestVisibleTiles() {
    // Calculate which tiles are visible
    uint32_t centerTileX, centerTileY;
    latLonToTile(_centerLat, _centerLon, _zoom, centerTileX, centerTileY);

    // Request tiles in a 3x3 grid around center
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            requestTile(_zoom, centerTileX + dx, centerTileY + dy);
        }
    }
}

bool Maps::getTileFromCache(const TileKey& key, CachedTile& tile) {
    auto it = _tileCache.find(key);
    if (it != _tileCache.end()) {
        it->second.lastAccess = millis();
        tile = it->second;
        return true;
    }

    // Try loading from disk
    if (loadTileFromDisk(key, tile)) {
        addTileToCache(key, tile.data);
        return true;
    }

    return false;
}

void Maps::addTileToCache(const TileKey& key, const std::vector<uint8_t>& data) {
    // Evict if full
    while (_tileCache.size() >= MAX_CACHED_TILES) {
        evictOldestTile();
    }

    CachedTile cached;
    cached.data = data;
    cached.lastAccess = millis();

    _tileCache[key] = cached;
    _tileLruOrder.push_back(key);

    // Save to disk cache
    saveTileToDisk(key, data);
}

void Maps::evictOldestTile() {
    if (_tileLruOrder.empty()) return;

    TileKey oldest = _tileLruOrder.front();
    _tileLruOrder.pop_front();
    _tileCache.erase(oldest);
}

std::vector<uint8_t> Maps::decompressRle(const std::vector<uint8_t>& rleData) {
    std::vector<uint8_t> result;
    result.reserve(TILE_SIZE * TILE_SIZE / 8);

    for (size_t i = 0; i + 1 < rleData.size(); i += 2) {
        uint8_t count = rleData[i];
        uint8_t value = rleData[i + 1];
        for (uint8_t j = 0; j < count; j++) {
            result.push_back(value);
        }
    }

    return result;
}

std::string Maps::getTileCachePath(const TileKey& key) {
    char path[64];
    snprintf(path, sizeof(path), "/tiles/%d_%u_%u.bin", key.z, key.x, key.y);
    return std::string(path);
}

bool Maps::loadTileFromDisk(const TileKey& key, CachedTile& tile) {
    // Get filesystem from RetOS
    FS* fs = _retos->hal().fs;
    if (!fs) return false;

    std::string path = getTileCachePath(key);
    if (!fs->exists(path.c_str())) return false;

    File file = fs->open(path.c_str(), FILE_READ);
    if (!file) return false;

    size_t size = file.size();
    tile.data.resize(size);
    file.read(tile.data.data(), size);
    file.close();

    tile.lastAccess = millis();
    return true;
}

void Maps::saveTileToDisk(const TileKey& key, const std::vector<uint8_t>& data) {
    FS* fs = _retos->hal().fs;
    if (!fs) return;

    // Ensure tiles directory exists
    if (!fs->exists("/tiles")) {
        fs->mkdir("/tiles");
    }

    std::string path = getTileCachePath(key);
    File file = fs->open(path.c_str(), FILE_WRITE, true);
    if (!file) return;

    file.write(data.data(), data.size());
    file.close();
}

// Coordinate conversion utilities
void Maps::latLonToTile(double lat, double lon, uint8_t z, uint32_t& tileX, uint32_t& tileY) {
    double n = pow(2.0, z);
    tileX = (uint32_t)((lon + 180.0) / 360.0 * n);
    double latRad = lat * M_PI / 180.0;
    tileY = (uint32_t)((1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / M_PI) / 2.0 * n);
}

void Maps::tileToLatLon(uint32_t tileX, uint32_t tileY, uint8_t z, double& lat, double& lon) {
    double n = pow(2.0, z);
    lon = tileX / n * 360.0 - 180.0;
    double latRad = atan(sinh(M_PI * (1 - 2 * tileY / n)));
    lat = latRad * 180.0 / M_PI;
}

void Maps::latLonToPixel(double lat, double lon, uint8_t z, int& pixelX, int& pixelY) {
    double n = pow(2.0, z);
    pixelX = (int)((lon + 180.0) / 360.0 * n * TILE_SIZE);
    double latRad = lat * M_PI / 180.0;
    pixelY = (int)((1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / M_PI) / 2.0 * n * TILE_SIZE);
}

// Navigation
void Maps::pan(int dx, int dy) {
    // Convert pixel delta to lat/lon delta
    double scale = 360.0 / pow(2.0, _zoom) / TILE_SIZE;

    _centerLon += dx * scale;
    _centerLat -= dy * scale * cos(_centerLat * M_PI / 180.0);

    // Clamp longitude
    while (_centerLon > 180.0) _centerLon -= 360.0;
    while (_centerLon < -180.0) _centerLon += 360.0;

    // Clamp latitude
    if (_centerLat > 85.0) _centerLat = 85.0;
    if (_centerLat < -85.0) _centerLat = -85.0;

    // Disable follow mode when user pans
    _followGps = false;

    renderMap();
    requestVisibleTiles();
}

void Maps::zoom(int delta) {
    int newZoom = _zoom + delta;
    if (newZoom < MIN_ZOOM) newZoom = MIN_ZOOM;
    if (newZoom > MAX_ZOOM) newZoom = MAX_ZOOM;

    if (newZoom != _zoom) {
        _zoom = newZoom;
        renderMap();
        requestVisibleTiles();
    }
}

void Maps::centerOnGps() {
    if (_hasGps) {
        _centerLat = _gpsLat;
        _centerLon = _gpsLon;
        renderMap();
        requestVisibleTiles();
    }
}

void Maps::toggleFollowGps() {
    _followGps = !_followGps;
    if (_followGps && _hasGps) {
        centerOnGps();
    }
    drawStatusBar();
}

// Search
void Maps::startSearch() {
    _searchMode = true;
    _hasSearchPin = false;
    _searchPinName.clear();
    _searchPending = false;
    lv_obj_clear_flag(_search_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(_search_input, "");
    lv_obj_clean(_search_results_list);
    lv_obj_add_flag(_search_status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_search_retry_btn, LV_OBJ_FLAG_HIDDEN);
    lv_group_add_obj(_retos->ui()->default_input_group(), _search_input);
    lv_group_focus_obj(_search_input);
}

void Maps::performSearch(const std::string& query) {
    if (_mapsServerHash.size() == 0 && !findMapsServer()) {
        return;
    }

    RnsService* rns = _retos->fetchService<RnsService>();
    if (!rns || rns->status() != RUNNING) return;

    // Track search state for timeout
    _searchPending = true;
    _searchStartTime = millis();
    _lastSearchQuery = query;

    // Bias search near current map center
    int32_t biasLat = (int32_t)(_centerLat * 1e7);
    int32_t biasLon = (int32_t)(_centerLon * 1e7);

    RNS::Bytes serverHash = _mapsServerHash;
    std::string queryCopy = query;
    rns->queueAction([rns, serverHash, queryCopy, biasLat, biasLon]() {
        rns->requestGeocode(serverHash, queryCopy, biasLat, biasLon, true, 5);
    });

    // Show loading
    lv_obj_clean(_search_results_list);
    lv_list_add_text(_search_results_list, "Searching...");
    lv_obj_add_flag(_search_retry_btn, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_search_status_label, "");
    lv_obj_clear_flag(_search_status_label, LV_OBJ_FLAG_HIDDEN);
}

void Maps::displaySearchResults(const Retcon::Service::MapGeocodeResponsePayload& response) {
    _searchPending = false;
    lv_obj_clean(_search_results_list);
    _searchResults.clear();
    lv_obj_add_flag(_search_retry_btn, LV_OBJ_FLAG_HIDDEN);

    if (!response.error.empty()) {
        lv_list_add_text(_search_results_list, response.error.c_str());
        lv_label_set_text(_search_status_label, "Error from server");
        lv_obj_clear_flag(_search_status_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_search_retry_btn, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (response.results.empty()) {
        lv_list_add_text(_search_results_list, "No results found");
        lv_obj_add_flag(_search_status_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    _searchResults = response.results;

    for (size_t i = 0; i < response.results.size(); i++) {
        const auto& result = response.results[i];

        // Truncate display name for UI
        std::string displayName = result.display_name;
        if (displayName.length() > 40) {
            displayName = displayName.substr(0, 37) + "...";
        }

        lv_obj_t* btn = lv_list_add_btn(_search_results_list, NULL, displayName.c_str());
        lv_obj_set_user_data(btn, (void*)(uintptr_t)i);
        lv_obj_add_event_cb(btn, searchResultCallback, LV_EVENT_CLICKED, this);
    }

    lv_label_set_text_fmt(_search_status_label, "%d result(s)", (int)response.results.size());
    lv_obj_clear_flag(_search_status_label, LV_OBJ_FLAG_HIDDEN);
}

void Maps::displaySearchTimeout() {
    _searchPending = false;
    lv_obj_clean(_search_results_list);
    lv_list_add_text(_search_results_list, "Search timed out");
    lv_label_set_text(_search_status_label, "No response from server");
    lv_obj_clear_flag(_search_status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_search_retry_btn, LV_OBJ_FLAG_HIDDEN);
}

void Maps::retrySearch() {
    if (!_lastSearchQuery.empty()) {
        performSearch(_lastSearchQuery);
    }
}

void Maps::goToLocation(int32_t lat, int32_t lon) {
    _centerLat = lat / 1e7;
    _centerLon = lon / 1e7;
    _followGps = false;

    // Set search pin at this location
    _hasSearchPin = true;
    _searchPinLat = lat;
    _searchPinLon = lon;

    renderMap();
    requestVisibleTiles();
}

// Route (placeholder for now)
void Maps::startRouting() {
    _routeMode = true;
    _hasRouteStart = false;
    // TODO: Implement route start/end selection UI
}

void Maps::calculateRoute() {
    if (!_hasRouteStart) return;
    if (_mapsServerHash.size() == 0 && !findMapsServer()) return;

    RnsService* rns = _retos->fetchService<RnsService>();
    if (!rns || rns->status() != RUNNING) return;

    int32_t startLat = (int32_t)(_routeStart.lat * 1e7);
    int32_t startLon = (int32_t)(_routeStart.lon * 1e7);
    int32_t endLat = (int32_t)(_routeEnd.lat * 1e7);
    int32_t endLon = (int32_t)(_routeEnd.lon * 1e7);

    RNS::Bytes serverHash = _mapsServerHash;
    rns->queueAction([rns, serverHash, startLat, startLon, endLat, endLon]() {
        rns->requestRoute(serverHash, startLat, startLon, endLat, endLon, Retcon::Service::TravelMode::WALK);
    });
}

void Maps::displayRoute(const Retcon::Service::MapRouteResponsePayload& response) {
    if (!response.error.empty()) {
        Serial.printf("[Maps] Route error: %s\n", response.error.c_str());
        return;
    }

    _routePoints = response.points;
    _hasRoute = true;
    renderMap();
}

void Maps::clearRoute() {
    _routePoints.clear();
    _hasRoute = false;
    renderMap();
}

EventStatus Maps::onEvent(const Event& event) {
    switch (event.type) {
        case EventType::MAP_TILE_RECEIVED: {
            auto payload = std::static_pointer_cast<Retcon::Service::MapTileResponsePayload>(event.data);
            if (payload) {
                TileKey key = {payload->z, payload->x, payload->y};

                // Remove from pending
                _pendingTileRequests.erase(key);

                // Hide spinner if no more pending
                if (_pendingTileRequests.empty()) {
                    lv_obj_add_flag(_loading_spinner, LV_OBJ_FLAG_HIDDEN);
                }

                if (payload->error.empty() && !payload->data.empty()) {
                    // Decompress RLE data
                    std::vector<uint8_t> decompressed;
                    if (payload->format == Retcon::Service::TileFormat::MONO_RLE) {
                        decompressed = decompressRle(payload->data);
                    } else {
                        decompressed = payload->data;
                    }

                    // Verify size
                    if (decompressed.size() == TILE_SIZE * TILE_SIZE / 8) {
                        addTileToCache(key, decompressed);
                        renderMap();
                    }
                }
            }
            return EventStatus::HANDLED;
        }

        case EventType::MAP_GEOCODE_RESULTS: {
            auto payload = std::static_pointer_cast<Retcon::Service::MapGeocodeResponsePayload>(event.data);
            if (payload) {
                displaySearchResults(*payload);
            }
            return EventStatus::HANDLED;
        }

        case EventType::MAP_ROUTE_RECEIVED: {
            auto payload = std::static_pointer_cast<Retcon::Service::MapRouteResponsePayload>(event.data);
            if (payload) {
                displayRoute(*payload);
            }
            return EventStatus::HANDLED;
        }

        case EventType::LOCATION_CHANGE: {
            // Update GPS position
            // args[0] = lat*1e7, args[1] = lon*1e7, args[2] = heading*100, args[3] = heading valid
            _gpsLat = ((int32_t)event.args[0]) / 1e7;
            _gpsLon = ((int32_t)event.args[1]) / 1e7;
            _gpsHeading = event.args[2] / 100.0f;
            _hasHeading = (event.args[3] != 0);
            _hasGps = true;

            if (_followGps) {
                _centerLat = _gpsLat;
                _centerLon = _gpsLon;
                renderMap();
                requestVisibleTiles();
            } else {
                // Just redraw GPS marker
                renderMap();
            }
            return EventStatus::HANDLED_PROPOGATE;
        }

        default:
            return EventStatus::IGNORED;
    }
}
