#include "NewsClient.h"


#include <algorithm> 
#include <fstream>   
#include <iostream>  


std::string SafeGetString(const json& j, const std::string& key, const std::string& defaultValue) {
    if (j.contains(key) && !j[key].is_null() && j[key].is_string()) {
        return j[key].get<std::string>();
    }
    return defaultValue;
}

NewsClient::NewsClient() : m_dataReady(false) {
  
    loadBookmarks();
}

NewsClient::~NewsClient() {
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}



void NewsClient::loadBookmarks() {
    std::ifstream file("bookmarks.json");
    if (file.is_open()) {
        try {
            json j;
            file >> j;
            m_bookmarks.clear();
            if (j.is_array()) {
                for (const auto& item : j) {
                    m_bookmarks.push_back(NewsItem::fromJson(item));
                }
            }
        }
        catch (const std::exception& e) {
            std::cout << "[Error] Failed to load bookmarks: " << e.what() << std::endl;
        }
        file.close();
    }
}

void NewsClient::saveBookmarks() {
    json j = json::array();
    for (const auto& item : m_bookmarks) {
        j.push_back(item.toJson());
    }

    std::ofstream file("bookmarks.json");
    if (file.is_open()) {
        file << j.dump(4); 
        file.close();
    }
}

void NewsClient::toggleBookmark(NewsItem& item) {
   
    if (item.isSaved) {
     
        auto it = std::remove_if(m_bookmarks.begin(), m_bookmarks.end(),
            [&](const NewsItem& saved) { return saved.readMoreUrl == item.readMoreUrl; });

        if (it != m_bookmarks.end()) {
            m_bookmarks.erase(it, m_bookmarks.end());
        }
        item.isSaved = false;
    }
    else {
      
        item.isSaved = true;
        m_bookmarks.push_back(item);
    }
  
    saveBookmarks();
}

std::vector<NewsItem>& NewsClient::getBookmarks() {
    return m_bookmarks;
}

bool NewsClient::isBookmarked(const std::string& url) {
    for (const auto& item : m_bookmarks) {
        if (item.readMoreUrl == url) return true;
    }
    return false;
}

// --- ניהול רשת ---

std::vector<NewsItem> NewsClient::getNews() {
    m_dataReady = false;
    
    for (auto& item : m_newsList) {
        if (isBookmarked(item.readMoreUrl)) {
            item.isSaved = true;
        }
    }
    return m_newsList;
}

void NewsClient::fetchNewsAsync(const std::string& category) {
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    m_dataReady = false;
    m_newsList.clear();
    m_workerThread = std::thread(&NewsClient::fetchNewsInternal, this, category);
}

void NewsClient::fetchNewsInternal(std::string category) {
    std::string apiKey = "c797c00565084a2e832ab96e0c39fd5f";

    std::string host = "newsapi.org";
    std::string path = "/v2/top-headlines?country=us&category=" + category + "&apiKey=" + apiKey;

    if (apiKey == "key") {
        std::cout << "[Warning] No API Key! Switching to JSONPlaceholder." << std::endl;
        host = "jsonplaceholder.typicode.com";
        path = "/posts";
    }

    httplib::Client cli("https://" + host);
    cli.enable_server_certificate_verification(false);
    cli.set_connection_timeout(0, 300000); 
    cli.set_read_timeout(10, 0);

    httplib::Headers headers = { { "User-Agent", "CppNewsApp/1.0" } };

    auto res = cli.Get(path.c_str(), headers);

    if (res && res->status == 200) {
        try {
            auto jsonResult = json::parse(res->body);
            const auto& items = (jsonResult.contains("articles")) ? jsonResult["articles"] : jsonResult;

            if (items.is_array()) {
                int count = 0;
                for (const auto& item : items) {
                    if (count++ >= 20) break;

                    std::string title = SafeGetString(item, "title", "[No Title]");
                    if (title == "[Removed]") continue;

                    NewsItem news;
                    news.title = title;
                    news.content = SafeGetString(item, "description", SafeGetString(item, "body", "Click to read more..."));
                    news.author = SafeGetString(item, "author", "Unknown Source");
                    news.date = SafeGetString(item, "publishedAt", "Recent");
                    news.readMoreUrl = SafeGetString(item, "url", "");
                    news.imageUrl = SafeGetString(item, "urlToImage", "");

                    if (news.date.length() > 10) news.date = news.date.substr(0, 10);

                    // בדיקה האם כבר שמור אצלנו
                    if (isBookmarked(news.readMoreUrl)) {
                        news.isSaved = true;
                    }

                    m_newsList.push_back(news);
                }
                std::cout << "[Network] Loaded " << m_newsList.size() << " articles." << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cout << "[Network] JSON Parsing Error: " << e.what() << std::endl;
        }
    }
    else {
        if (res) std::cout << "[Network] HTTP Error: " << res->status << std::endl;
        else std::cout << "[Network] Connection Error." << std::endl;
    }

    m_dataReady = true;
}
void NewsClient::searchNewsAsync(const std::string& query) {
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    m_dataReady = false;
    m_newsList.clear();
   
    m_workerThread = std::thread(&NewsClient::searchNewsInternal, this, query);
}

void NewsClient::searchNewsInternal(std::string query) {
    
    std::string apiKey = "c797c00565084a2e832ab96e0c39fd5f";
    std::string encodedQuery = query;
    size_t pos = 0;
    while ((pos = encodedQuery.find(" ", pos)) != std::string::npos) {
        encodedQuery.replace(pos, 1, "%20");
        pos += 3;
    }

    
    std::string host = "newsapi.org";
    std::string path = "/v2/everything?q=" + encodedQuery + "&language=en&sortBy=publishedAt&apiKey=" + apiKey;

    httplib::Client cli("https://" + host);
    cli.enable_server_certificate_verification(false);
    cli.set_connection_timeout(0, 300000);
    cli.set_read_timeout(10, 0);

    httplib::Headers headers = { { "User-Agent", "CppNewsApp/1.0" } };

    auto res = cli.Get(path.c_str(), headers);

    if (res && res->status == 200) {
        try {
            auto jsonResult = json::parse(res->body);
            const auto& items = (jsonResult.contains("articles")) ? jsonResult["articles"] : jsonResult;

            if (items.is_array()) {
                int count = 0;
                for (const auto& item : items) {
                    if (count++ >= 20) break;

                    std::string title = SafeGetString(item, "title", "[No Title]");
                    if (title == "[Removed]") continue;

                    NewsItem news;
                    news.title = title;
                    news.content = SafeGetString(item, "description", "");
                    news.author = SafeGetString(item, "author", "Unknown Source");
                    news.date = SafeGetString(item, "publishedAt", "");
                    news.readMoreUrl = SafeGetString(item, "url", "");
                    news.imageUrl = SafeGetString(item, "urlToImage", "");

                    if (news.date.length() > 10) news.date = news.date.substr(0, 10);
                    if (isBookmarked(news.readMoreUrl)) news.isSaved = true;

                    m_newsList.push_back(news);
                }
            }
        }
        catch (const std::exception& e) {
            std::cout << "[Search Error] JSON: " << e.what() << std::endl;
        }
    }
    m_dataReady = true;
}