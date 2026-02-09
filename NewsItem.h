#pragma once

// =============================================================
// CRITICAL: Windows & DirectX Header Ordering
// We must define WIN32_LEAN_AND_MEAN and include winsock2.h 
// BEFORE windows.h or d3d11.h to prevent macro redefinition errors.
// =============================================================
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h> 
#include <windows.h>

#include <d3d11.h> // DirectX 11 Interface
#include <string>
#include <vector>
#include "Json/json.hpp" 

using json = nlohmann::json;

/**
 * @brief Represents a single news article in the application.
 * Contains both data (text/strings) and runtime graphical resources (texture).
 */
struct NewsItem {
    // --- Data Fields ---
    std::string title;
    std::string author;
    std::string content;
    std::string date;
    std::string time;
    std::string readMoreUrl; // URL to the full article
    std::string imageUrl;    // URL to the thumbnail image

    // --- UI State ---
    bool isSaved = false;    // True if added to bookmarks

    // --- Graphical Resources (DirectX) ---
    // Pointer to the GPU texture resource. Must be released when no longer needed.
    ID3D11ShaderResourceView* texture = nullptr;
    int width = 0;           // Image width in pixels
    int height = 0;          // Image height in pixels
    bool imageLoaded = false;// Flag to prevent multiple download attempts

    /**
     * @brief Serializes the NewsItem to a JSON object.
     * Used for saving bookmarks to a file.
     * @return json object representing the article.
     */
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

    /**
     * @brief Deserializes a NewsItem from a JSON object.
     * Used for loading bookmarks from a file.
     * @param j The JSON object.
     * @return A populated NewsItem struct.
     */
    static NewsItem fromJson(const json& j) {
        NewsItem item;
        item.title = j.value("title", "");
        item.author = j.value("author", "");
        item.content = j.value("content", "");
        item.date = j.value("date", "");
        item.readMoreUrl = j.value("url", "");
        item.imageUrl = j.value("urlToImage", "");
        item.isSaved = true; // If loaded from file, it is saved by definition
        return item;
    }
};