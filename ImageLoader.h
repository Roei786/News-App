#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include "Httplib/httplib.h"

struct LoadedImageData {
    std::string url;
    std::vector<unsigned char> data;
    int width;
    int height;
    bool success;
};

class ImageLoader {
public:
    // Singleton - גישה גלובלית נוחה
    static ImageLoader& Get() {
        static ImageLoader instance;
        return instance;
    }

    // פונקציה שהממשק קורא לה כדי לבקש תמונה
    void LoadImageAsync(const std::string& url) {
        // אם כבר הורדנו או שאנחנו מורידים עכשיו - נתעלם
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_cache.find(url) != m_cache.end() || m_pendingUrls.find(url) != m_pendingUrls.end()) {
            return;
        }

        // סימון שהתמונה בתהליך
        m_pendingUrls[url] = true;

        // הפעלת Thread להורדה
        std::thread([this, url]() {
            this->DownloadThreadFunc(url);
            }).detach();
    }
    void ClearCache() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache.clear();
        m_pendingUrls.clear();
        // הערה: אנחנו לא מנקים את m_completedQueue כי אולי יש שם דברים שעדיין לא נאספו
    }

    // פונקציה שה-Main Loop קורא לה כדי לקבל תמונות שסיימו לרדת
    bool TryGetNextImage(LoadedImageData& outData) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_completedQueue.empty()) {
            return false;
        }

        outData = m_completedQueue.back();
        m_completedQueue.pop_back();
        return true;
    }

private:
    ImageLoader() {} // Private Constructor

    void DownloadThreadFunc(std::string url) {
        // לוגיקה פשוטה לחילוץ Host/Path
        std::string host, path;
        std::string cleanUrl = url;
        size_t protocol = cleanUrl.find("://");
        if (protocol != std::string::npos) cleanUrl = cleanUrl.substr(protocol + 3);

        size_t slash = cleanUrl.find('/');
        if (slash != std::string::npos) {
            host = cleanUrl.substr(0, slash);
            path = cleanUrl.substr(slash);
        }
        else {
            host = cleanUrl;
            path = "/";
        }

        // הורדה
        httplib::Client cli("https://" + host);
        cli.set_connection_timeout(0, 300000);
        cli.enable_server_certificate_verification(false);

        auto res = cli.Get(path.c_str());

        LoadedImageData result;
        result.url = url;

        if (res && res->status == 200) {
            result.data = std::vector<unsigned char>(res->body.begin(), res->body.end());
            result.success = true;
        }
        else {
            result.success = false;
        }
        

        // --- Critical Section Start ---
        // כאן אנחנו משתמשים ב-Mutex כי אנחנו כותבים למשתנה משותף
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_completedQueue.push_back(result);
            // לא מוחקים מ-pendingUrls כדי שלא ננסה להוריד שוב אם נכשל
            // אבל לצורך הפרויקט זה מספיק
            m_cache[url] = true; // סימון שיש לנו את התמונה (או ניסיון)
        }
        // --- Critical Section End ---
    }

    std::mutex m_mutex; // ה-Mutex המפורסם!
    std::vector<LoadedImageData> m_completedQueue; // תור תוצאות
    std::unordered_map<std::string, bool> m_pendingUrls; // למנוע הורדות כפולות
    std::unordered_map<std::string, bool> m_cache;
};