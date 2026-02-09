#include "NewsClient.h"


std::string SafeGetString(const json& j, const std::string& key, const std::string& defaultValue) {
  
    if (j.contains(key) && !j[key].is_null() && j[key].is_string()) {
        return j[key].get<std::string>();
    }
    return defaultValue;
}

NewsClient::NewsClient() : m_dataReady(false) {
}

NewsClient::~NewsClient() {
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
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
    std::string apiKey = "key"; 

    
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

            // תמיכה גם ב-NewsAPI וגם ב-JSONPlaceholder
            const auto& items = (jsonResult.contains("articles")) ? jsonResult["articles"] : jsonResult;

            if (items.is_array()) {
                int count = 0;
                for (const auto& item : items) {
                    if (count++ >= 20) break; // הגבלה ל-20 כתבות

                    // סינון כתבות שנמחקו
                    std::string title = SafeGetString(item, "title", "[No Title]");
                    if (title == "[Removed]") continue;

                    NewsItem news;
                    news.title = title;

                    // שימוש בפונקציה הבטוחה לכל השדות
                    news.content = SafeGetString(item, "description", SafeGetString(item, "body", "Click to read more..."));
                    news.author = SafeGetString(item, "author", "Unknown Source");
                    news.date = SafeGetString(item, "publishedAt", "Recent");
                    news.readMoreUrl = SafeGetString(item, "url", "");
                    news.imageUrl = SafeGetString(item, "urlToImage", "");

                    // קיצור התאריך
                    if (news.date.length() > 10) news.date = news.date.substr(0, 10);

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