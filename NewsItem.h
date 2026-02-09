#pragma once


#define WIN32_LEAN_AND_MEAN
#include <winsock2.h> 
#include <windows.h>


#include <d3d11.h> 
#include <string>
#include <vector>
#include "Json/json.hpp" 

using json = nlohmann::json;

struct NewsItem {
    std::string title;
    std::string author;
    std::string content;
    std::string date;
    std::string time;
    std::string readMoreUrl;
    std::string imageUrl;


    bool isSaved = false;

  
    ID3D11ShaderResourceView* texture = nullptr;
    int width = 0;
    int height = 0;
    bool imageLoaded = false;

  
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


    static NewsItem fromJson(const json& j) {
        NewsItem item;
        item.title = j.value("title", "");
        item.author = j.value("author", "");
        item.content = j.value("content", "");
        item.date = j.value("date", "");
        item.readMoreUrl = j.value("url", "");
        item.imageUrl = j.value("urlToImage", "");
        item.isSaved = true;
        return item;
    }
};