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

    // --- פונקציות חדשות לניהול סימניות ---
    void toggleBookmark(NewsItem& item);     // הוספה/הסרה
    std::vector<NewsItem> getBookmarks();    // החזרת רשימת השמורים
    void saveBookmarks();                    // שמירה לקובץ
    void loadBookmarks();                    // טעינה מקובץ

private:
    void fetchNewsInternal(std::string category);
    bool isBookmarked(const std::string& url); // בדיקה פנימית

    std::vector<NewsItem> m_newsList;
    std::vector<NewsItem> m_bookmarks; // כאן נשמור את הסימניות

    std::atomic<bool> m_dataReady;
    std::thread m_workerThread;
};