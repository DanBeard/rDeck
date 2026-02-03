#pragma once
#include "BaseApp.h"
#include "services/RnsUtils/ServiceProtocol.h"
#include <vector>

/**
 * WebSearch App - Search the web via companion server
 *
 * Sends search queries to a trusted companion server and displays results.
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

protected:
    void drawUI();
    void displayResults(const Retcon::Service::SearchResponsePayload& results);
    void displayError(const char* message);
    void clearResults();

    // UI elements
    lv_obj_t* _search_input = nullptr;
    lv_obj_t* _search_btn = nullptr;
    lv_obj_t* _status_label = nullptr;
    lv_obj_t* _results_container = nullptr;

    // Search state
    bool _searching = false;
    uint32_t _search_request_id = 0;

    // Results storage
    std::vector<Retcon::Service::SearchResult> _results;
};
