#pragma once

#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <iostream>

#include "NewsItem.h"
#include "Httplib/httplib.h"
#include "Json/json.hpp"

using json = nlohmann::json;

class NewsClient {
public:
    NewsClient();
    ~NewsClient();
    void fetchNewsAsync(const std::string& category);
    bool isDataReady() const { return m_dataReady; }
    std::vector<NewsItem> getNews() {
        m_dataReady = false;
        return m_newsList;
    }

private:
    void fetchNewsInternal(std::string category);
    std::vector<NewsItem> m_newsList;
    std::atomic<bool> m_dataReady;
    std::thread m_workerThread;
};