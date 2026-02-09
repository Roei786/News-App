#pragma once
#include <string>
#include <vector>
#include "Json/json.hpp" // וודא שהספרייה מקושרת

using json = nlohmann::json;

struct NewsItem {
    std::string title;
    std::string author;
    std::string content;
    std::string date;
    std::string time;
    std::string readMoreUrl;
    std::string imageUrl;

    // שדה חדש לסימון בממשק
    bool isSaved = false;

    // המרה מאובייקט ל-JSON (לשמירה לקובץ)
    json toJson() const {
        return json{
            {"title", title},
            {"author", author},
            {"content", content},
            {"date", date},
            {"url", readMoreUrl},
            {"urlToImage", imageUrl}
        };
    }

    // המרה מ-JSON לאובייקט (לטעינה מהקובץ)
    static NewsItem fromJson(const json& j) {
        NewsItem item;
        item.title = j.value("title", "");
        item.author = j.value("author", "");
        item.content = j.value("content", "");
        item.date = j.value("date", "");
        item.readMoreUrl = j.value("url", "");
        item.imageUrl = j.value("urlToImage", "");
        item.isSaved = true; // אם טענו אותה מהדיסק, היא שמורה
        return item;
    }
};