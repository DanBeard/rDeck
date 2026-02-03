#include "WebSearch.h"
#include "services/RnsService.h"
#include "services/RnsUtils/TrustedServers.h"
#include "lvgl.h"

// Forward declaration for button callback
static void searchBtnCallback(lv_event_t* e);

void WebSearch::start(RetOS* retos) {
    _keep_awake = true;
    drawUI();
}

void WebSearch::tick(const unsigned long tickMillis) {
    // Nothing to do - results come via events
}

void WebSearch::stop() {
    _results.clear();
}

void WebSearch::drawUI() {
    // Main container with vertical flex layout
    lv_obj_t* main = lv_obj_create(screen);
    lv_obj_set_size(main, LV_PCT(100), LV_PCT(100));
    lv_obj_align(main, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_flex_flow(main, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(main, 5, LV_PART_MAIN);

    // Search input row
    lv_obj_t* search_row = lv_obj_create(main);
    lv_obj_set_size(search_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(search_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(search_row, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(search_row, 0, LV_PART_MAIN);

    // Search text input
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

    // Status label
    _status_label = lv_label_create(main);
    lv_obj_set_size(_status_label, LV_PCT(100), LV_SIZE_CONTENT);
    lv_label_set_text(_status_label, "Enter search query");
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0x888888), LV_PART_MAIN);

    // Results container (scrollable)
    _results_container = lv_obj_create(main);
    lv_obj_set_size(_results_container, LV_PCT(100), LV_PCT(80));
    lv_obj_set_flex_flow(_results_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(_results_container, 5, LV_PART_MAIN);
}

static void searchBtnCallback(lv_event_t* e) {
    WebSearch* app = (WebSearch*)lv_event_get_user_data(e);
    if (app) {
        app->performSearch();
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

    _searching = true;
    clearResults();
    lv_label_set_text(_status_label, "Searching...");

    rns->requestSearch(searchServer->hash, query);
}

void WebSearch::displayResults(const Retcon::Service::SearchResponsePayload& response) {
    _searching = false;
    clearResults();

    if (!response.error.empty()) {
        displayError(response.error.c_str());
        return;
    }

    if (response.results.empty()) {
        lv_label_set_text(_status_label, "No results found");
        return;
    }

    char status[64];
    snprintf(status, sizeof(status), "Found %d results", (int)response.results.size());
    lv_label_set_text(_status_label, status);

    // Display each result
    for (const auto& result : response.results) {
        lv_obj_t* result_container = lv_obj_create(_results_container);
        lv_obj_set_size(result_container, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(result_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(result_container, 5, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(result_container, 5, LV_PART_MAIN);

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

        // Snippet
        if (!result.snippet.empty()) {
            lv_obj_t* snippet = lv_label_create(result_container);
            lv_label_set_text(snippet, result.snippet.c_str());
            lv_obj_set_size(snippet, LV_PCT(100), LV_SIZE_CONTENT);
            lv_label_set_long_mode(snippet, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_color(snippet, lv_color_hex(0x666666), LV_PART_MAIN);
        }
    }
}

void WebSearch::displayError(const char* message) {
    _searching = false;
    lv_label_set_text(_status_label, message);
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0xCC0000), LV_PART_MAIN);
}

void WebSearch::clearResults() {
    lv_obj_clean(_results_container);
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0x888888), LV_PART_MAIN);
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
