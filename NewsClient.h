#pragma once

#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <iostream>
#include <fstream> 

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
    void toggleBookmark(NewsItem& item);
    void saveBookmarks();
    void loadBookmarks();
    void searchNewsAsync(const std::string& query);
    std::vector<NewsItem>& getBookmarks();
    std::vector<NewsItem> getNews();

private:
    void fetchNewsInternal(std::string category);
    bool isBookmarked(const std::string& url);
    void searchNewsInternal(std::string query);
    std::vector<NewsItem> m_newsList;
    std::vector<NewsItem> m_bookmarks;
    std::atomic<bool> m_dataReady;
    std::thread m_workerThread;
};