#include "WebSearch.h"
#include "services/RnsService.h"
#include "services/RnsUtils/TrustedServers.h"
#include "lvgl.h"

// Forward declarations for callbacks
static void searchBtnCallback(lv_event_t* e);
static void retryBtnCallback(lv_event_t* e);

void WebSearch::start(RetOS* retos) {
    _keep_awake = true;
    drawUI();
}

void WebSearch::tick(const unsigned long tickMillis) {
    // Check for timeout while searching
    if (_searching && _search_start_time > 0) {
        unsigned long elapsed = tickMillis - _search_start_time;
        if (elapsed >= SEARCH_TIMEOUT_MS) {
            displayTimeout();
        } else {
            // Update spinner animation (done by LVGL automatically)
            // Update status with elapsed time every second
            static unsigned long lastStatusUpdate = 0;
            if (tickMillis - lastStatusUpdate >= 1000) {
                lastStatusUpdate = tickMillis;
                char status[64];
                snprintf(status, sizeof(status), "Searching... (%lus)", elapsed / 1000);
                lv_label_set_text(_status_label, status);
            }
        }
    }
}

void WebSearch::stop() {
    _results.clear();
    _searching = false;
    _search_start_time = 0;
}

void WebSearch::drawUI() {
    // Main container with vertical flex layout
    lv_obj_t* main = lv_obj_create(screen);
    lv_obj_set_size(main, LV_PCT(100), LV_PCT(100));
    lv_obj_align(main, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_flex_flow(main, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(main, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(main, 5, LV_PART_MAIN);

    // Search input row
    lv_obj_t* search_row = lv_obj_create(main);
    lv_obj_set_size(search_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(search_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(search_row, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(search_row, 0, LV_PART_MAIN);

    // Search text input (single line)
    _search_input = lv_textarea_create(search_row);
    lv_obj_set_size(_search_input, LV_PCT(75), LV_SIZE_CONTENT);
    lv_textarea_set_one_line(_search_input, true);
    lv_textarea_set_placeholder_text(_search_input, "Search...");
    lv_group_add_obj(_retos->ui()->default_input_group(), _search_input);

    // Search button
    _search_btn = lv_btn_create(search_row);
    lv_obj_set_size(_search_btn, LV_PCT(23), LV_SIZE_CONTENT);
    lv_obj_t* btn_label = lv_label_create(_search_btn);
    lv_label_set_text(btn_label, LV_SYMBOL_OK);
    lv_obj_center(btn_label);
    lv_obj_add_event_cb(_search_btn, searchBtnCallback, LV_EVENT_CLICKED, this);

    // AI Summary checkbox row
    lv_obj_t* ai_row = lv_obj_create(main);
    lv_obj_set_size(ai_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ai_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(ai_row, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(ai_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_align(ai_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    _ai_summary_cb = lv_checkbox_create(ai_row);
    lv_checkbox_set_text(_ai_summary_cb, "AI Summary");
    lv_obj_add_flag(_ai_summary_cb, LV_OBJ_FLAG_EVENT_BUBBLE);
    // Default OFF per requirements
    lv_obj_clear_state(_ai_summary_cb, LV_STATE_CHECKED);
    lv_obj_set_style_text_font(_ai_summary_cb, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_ai_summary_cb, lv_color_hex(0x666666), LV_PART_MAIN);

    // Status container (holds spinner + label + retry button)
    _status_container = lv_obj_create(main);
    lv_obj_set_size(_status_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(_status_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(_status_container, 3, LV_PART_MAIN);
    lv_obj_set_style_border_width(_status_container, 0, LV_PART_MAIN);
    lv_obj_set_flex_align(_status_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(_status_container, 8, LV_PART_MAIN);

    // Spinner (hidden by default)
    _spinner = lv_spinner_create(_status_container, 1000, 60);
    lv_obj_set_size(_spinner, 20, 20);
    lv_obj_add_flag(_spinner, LV_OBJ_FLAG_HIDDEN);

    // Status label
    _status_label = lv_label_create(_status_container);
    lv_label_set_text(_status_label, "Enter search query");
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0x888888), LV_PART_MAIN);

    // Retry button (hidden by default)
    _retry_btn = lv_btn_create(_status_container);
    lv_obj_set_size(_retry_btn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_flag(_retry_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* retry_label = lv_label_create(_retry_btn);
    lv_label_set_text(retry_label, "Retry");
    lv_obj_set_style_text_font(retry_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_add_event_cb(_retry_btn, retryBtnCallback, LV_EVENT_CLICKED, this);

    // Results container (scrollable)
    _results_container = lv_obj_create(main);
    lv_obj_set_size(_results_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_grow(_results_container, 1);
    lv_obj_set_flex_flow(_results_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(_results_container, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(_results_container, 8, LV_PART_MAIN);
}

static void searchBtnCallback(lv_event_t* e) {
    WebSearch* app = (WebSearch*)lv_event_get_user_data(e);
    if (app) {
        app->performSearch();
    }
}

static void retryBtnCallback(lv_event_t* e) {
    WebSearch* app = (WebSearch*)lv_event_get_user_data(e);
    if (app) {
        app->retrySearch();
    }
}

void WebSearch::setSearchingState(bool searching) {
    _searching = searching;

    if (searching) {
        // Show spinner, hide retry
        lv_obj_clear_flag(_spinner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_retry_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_search_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(_search_btn, LV_OPA_50, LV_PART_MAIN);
        _search_start_time = millis();
    } else {
        // Hide spinner, enable search button
        lv_obj_add_flag(_spinner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_search_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(_search_btn, LV_OPA_100, LV_PART_MAIN);
        _search_start_time = 0;
    }
}

void WebSearch::performSearch() {
    if (_searching) {
        return;  // Already searching
    }

    const char* query = lv_textarea_get_text(_search_input);
    if (!query || strlen(query) == 0) {
        displayError("Please enter a search query");
        return;
    }

    // Find a trusted server with search capability
    auto& trustedServers = Retcon::Service::getTrustedServers();
    auto servers = trustedServers.getTrustedServers();

    const Retcon::Service::TrustedServer* searchServer = nullptr;
    for (const auto& server : servers) {
        for (const auto& svc : server.services) {
            if (svc == "search") {
                searchServer = &server;
                break;
            }
        }
        if (searchServer) break;
    }

    if (!searchServer) {
        displayError("No trusted search server available");
        return;
    }

    // Send search request
    RnsService* rns = _retos->fetchService<RnsService>();
    if (!rns) {
        displayError("RNS service not available");
        return;
    }

    // Store for retry
    _last_query = query;
    _last_ai_summary = lv_obj_has_state(_ai_summary_cb, LV_STATE_CHECKED);

    setSearchingState(true);
    clearResults();
    lv_label_set_text(_status_label, "Searching...");
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0x888888), LV_PART_MAIN);

    RNS::Bytes serverHash = searchServer->hash;
    std::string queryCopy = query;
    bool aiSummary = _last_ai_summary;
    rns->queueAction([rns, serverHash, queryCopy, aiSummary]() {
        rns->requestSearch(serverHash, queryCopy, aiSummary);
    });
}

void WebSearch::retrySearch() {
    if (_searching || _last_query.empty()) {
        return;
    }

    // Find a trusted server with search capability
    auto& trustedServers = Retcon::Service::getTrustedServers();
    auto servers = trustedServers.getTrustedServers();

    const Retcon::Service::TrustedServer* searchServer = nullptr;
    for (const auto& server : servers) {
        for (const auto& svc : server.services) {
            if (svc == "search") {
                searchServer = &server;
                break;
            }
        }
        if (searchServer) break;
    }

    if (!searchServer) {
        displayError("No trusted search server available");
        return;
    }

    RnsService* rns = _retos->fetchService<RnsService>();
    if (!rns) {
        displayError("RNS service not available");
        return;
    }

    setSearchingState(true);
    lv_obj_add_flag(_retry_btn, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_status_label, "Retrying...");
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0x888888), LV_PART_MAIN);

    RNS::Bytes serverHash = searchServer->hash;
    std::string queryCopy = _last_query;
    bool aiSummary = _last_ai_summary;
    rns->queueAction([rns, serverHash, queryCopy, aiSummary]() {
        rns->requestSearch(serverHash, queryCopy, aiSummary);
    });
}

void WebSearch::displayResults(const Retcon::Service::SearchResponsePayload& response) {
    setSearchingState(false);
    clearResults();

    if (!response.error.empty()) {
        displayError(response.error.c_str());
        return;
    }

    // Check for AI summary first
    if (!response.summary.empty()) {
        lv_label_set_text(_status_label, "AI Summary");
        lv_obj_set_style_text_color(_status_label, lv_color_hex(0x006600), LV_PART_MAIN);

        // Create summary container
        lv_obj_t* summary_container = lv_obj_create(_results_container);
        lv_obj_set_size(summary_container, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(summary_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(summary_container, 8, LV_PART_MAIN);
        lv_obj_set_style_bg_color(summary_container, lv_color_hex(0xF0F8FF), LV_PART_MAIN);
        lv_obj_set_style_border_color(summary_container, lv_color_hex(0x006600), LV_PART_MAIN);
        lv_obj_set_style_border_width(summary_container, 1, LV_PART_MAIN);

        lv_obj_t* summary_label = lv_label_create(summary_container);
        lv_label_set_text(summary_label, response.summary.c_str());
        lv_obj_set_size(summary_label, LV_PCT(100), LV_SIZE_CONTENT);
        lv_label_set_long_mode(summary_label, LV_LABEL_LONG_WRAP);

        // If there are also results, show them below
        if (!response.results.empty()) {
            lv_obj_t* sources_label = lv_label_create(_results_container);
            lv_label_set_text(sources_label, "Sources:");
            lv_obj_set_style_text_color(sources_label, lv_color_hex(0x666666), LV_PART_MAIN);
            lv_obj_set_style_text_font(sources_label, &lv_font_montserrat_14, LV_PART_MAIN);
        }
    } else if (response.results.empty()) {
        lv_label_set_text(_status_label, "No results found");
        lv_obj_set_style_text_color(_status_label, lv_color_hex(0x888888), LV_PART_MAIN);
        return;
    } else {
        char status[64];
        snprintf(status, sizeof(status), "Found %d results", (int)response.results.size());
        lv_label_set_text(_status_label, status);
        lv_obj_set_style_text_color(_status_label, lv_color_hex(0x006600), LV_PART_MAIN);
    }

    // Display each result
    for (const auto& result : response.results) {
        lv_obj_t* result_container = lv_obj_create(_results_container);
        lv_obj_set_size(result_container, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(result_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(result_container, 5, LV_PART_MAIN);
        lv_obj_set_style_pad_gap(result_container, 2, LV_PART_MAIN);

        // Title
        lv_obj_t* title = lv_label_create(result_container);
        lv_label_set_text(title, result.title.c_str());
        lv_obj_set_size(title, LV_PCT(100), LV_SIZE_CONTENT);
        lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);

        // URL (truncated)
        lv_obj_t* url = lv_label_create(result_container);
        std::string urlDisplay = result.url;
        if (urlDisplay.length() > 50) {
            urlDisplay = urlDisplay.substr(0, 47) + "...";
        }
        lv_label_set_text(url, urlDisplay.c_str());
        lv_obj_set_size(url, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_text_color(url, lv_color_hex(0x0066CC), LV_PART_MAIN);
        lv_obj_set_style_text_font(url, &lv_font_montserrat_14, LV_PART_MAIN);

        // Snippet
        if (!result.snippet.empty()) {
            lv_obj_t* snippet = lv_label_create(result_container);
            lv_label_set_text(snippet, result.snippet.c_str());
            lv_obj_set_size(snippet, LV_PCT(100), LV_SIZE_CONTENT);
            lv_label_set_long_mode(snippet, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_color(snippet, lv_color_hex(0x666666), LV_PART_MAIN);
            lv_obj_set_style_text_font(snippet, &lv_font_montserrat_14, LV_PART_MAIN);
        }
    }
}

void WebSearch::displayError(const char* message) {
    setSearchingState(false);
    lv_label_set_text(_status_label, message);
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0xCC0000), LV_PART_MAIN);

    // Show retry button if we have a previous query
    if (!_last_query.empty()) {
        lv_obj_clear_flag(_retry_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

void WebSearch::displayTimeout() {
    setSearchingState(false);
    lv_label_set_text(_status_label, "Search timed out");
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0xCC6600), LV_PART_MAIN);

    // Show retry button
    if (!_last_query.empty()) {
        lv_obj_clear_flag(_retry_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

void WebSearch::clearResults() {
    lv_obj_clean(_results_container);
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_add_flag(_retry_btn, LV_OBJ_FLAG_HIDDEN);
}

EventStatus WebSearch::onEvent(const Event& event) {
    if (event.type == EventType::SEARCH_RESULTS) {
        auto results = std::static_pointer_cast<Retcon::Service::SearchResponsePayload>(event.data);
        if (results) {
            displayResults(*results);
            return EventStatus::HANDLED;
        }
    }
    return EventStatus::IGNORED;
}
