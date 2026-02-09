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

    std::vector<NewsItem> getNews();

    void toggleBookmark(NewsItem& item);

    // --- התיקון: הוספת & ---
    std::vector<NewsItem>& getBookmarks();

    void saveBookmarks();
    void loadBookmarks();

private:
    void fetchNewsInternal(std::string category);
    bool isBookmarked(const std::string& url);

    std::vector<NewsItem> m_newsList;
    std::vector<NewsItem> m_bookmarks;

    std::atomic<bool> m_dataReady;
    std::thread m_workerThread;
};