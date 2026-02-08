#pragma once
#include "BaseApp.h"
#include "services/RnsUtils/ServiceProtocol.h"
#include <vector>

/**
 * WebSearch App - Search the web via companion server
 *
 * Sends search queries to a trusted companion server and displays results.
 * Features:
 * - Single-line search input
 * - Loading spinner with status updates
 * - Timeout handling (30s) with retry option
 * - Optional AI summary mode
 */
class WebSearch : public BaseApp {
public:
    // inherit default ctor
    using BaseApp::BaseApp;

    virtual void start(RetOS* retos) override;
    virtual void tick(const unsigned long tickMillis) override;
    virtual void stop() override;
    virtual EventStatus onEvent(const Event& event) override;

    void performSearch();
    void retrySearch();

protected:
    void drawUI();
    void displayResults(const Retcon::Service::SearchResponsePayload& results);
    void displayError(const char* message);
    void displayTimeout();
    void clearResults();
    void updateSpinner();
    void setSearchingState(bool searching);

    // UI elements
    lv_obj_t* _search_input = nullptr;
    lv_obj_t* _search_btn = nullptr;
    lv_obj_t* _ai_summary_cb = nullptr;
    lv_obj_t* _status_container = nullptr;
    lv_obj_t* _spinner = nullptr;
    lv_obj_t* _status_label = nullptr;
    lv_obj_t* _retry_btn = nullptr;
    lv_obj_t* _results_container = nullptr;

    // Search state
    bool _searching = false;
    uint32_t _search_request_id = 0;
    unsigned long _search_start_time = 0;
    std::string _last_query;
    bool _last_ai_summary = false;

    // Timeout config
    static const unsigned long SEARCH_TIMEOUT_MS = 30000;  // 30 seconds

    // Results storage
    std::vector<Retcon::Service::SearchResult> _results;
};
