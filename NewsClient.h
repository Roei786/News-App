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

/**
 * @brief Manages news data, API fetching, and bookmarks.
 */
class NewsClient {
public:
    NewsClient();
    ~NewsClient();

    // --- Async Operations ---
    /**
     * @brief Starts an asynchronous fetch for a specific news category.
     */
    void fetchNewsAsync(const std::string& category);

    /**
     * @brief Starts an asynchronous search query.
     */
    void searchNewsAsync(const std::string& query);

    /**
     * @brief Checks if the background thread has finished fetching data.
     */
    bool isDataReady() const { return m_dataReady; }

    // --- Data Access ---
    /**
     * @brief Retrieves the latest fetched news (and syncs bookmark status).
     */
    std::vector<NewsItem> getNews();

    /**
     * @brief Returns a reference to the bookmarks list.
     */
    std::vector<NewsItem>& getBookmarks();

    // --- Bookmark Management ---
    void toggleBookmark(NewsItem& item);
    void saveBookmarks();
    void loadBookmarks();

private:
    // Internal helper for category fetching (runs on thread)
    void fetchNewsInternal(std::string category);

    // Internal helper for search (runs on thread)
    void searchNewsInternal(std::string query);

    // Checks if a URL exists in bookmarks
    bool isBookmarked(const std::string& url);

    std::vector<NewsItem> m_newsList;   // Current active list (fetched from API)
    std::vector<NewsItem> m_bookmarks;  // Saved articles

    std::atomic<bool> m_dataReady;      // Thread-safe flag for completion status
    std::thread m_workerThread;         // The background worker thread
};