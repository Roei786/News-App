#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include "Httplib/httplib.h"

/**
 * @brief Holds raw data of a downloaded image before it's converted to a texture.
 */
struct LoadedImageData {
    std::string url;
    std::vector<unsigned char> data; // Raw byte data of the image
    int width;
    int height;
    bool success;
};

/**
 * @brief Singleton class responsible for downloading images asynchronously.
 * Uses a separate thread for downloading to avoid blocking the UI.
 */
class ImageLoader {
public:
    /**
     * @brief Access the Singleton instance.
     * @return Reference to the single ImageLoader instance.
     */
    static ImageLoader& Get() {
        static ImageLoader instance;
        return instance;
    }

    /**
     * @brief Requests an image download.
     * Checks cache first. If not found, spawns a detached thread to download.
     * @param url The image URL.
     */
    void LoadImageAsync(const std::string& url) {
        // Thread-safe check: Don't download if already cached or pending
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_cache.find(url) != m_cache.end() || m_pendingUrls.find(url) != m_pendingUrls.end()) {
            return;
        }

        // Mark as pending to prevent duplicate requests
        m_pendingUrls[url] = true;

        // Spawn a new thread for the network request
        std::thread([this, url]() {
            this->DownloadThreadFunc(url);
            }).detach();
    }

    /**
     * @brief Clears internal cache and pending flags.
     * Useful when switching categories to allow redownloading images if needed.
     */
    void ClearCache() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cache.clear();
        m_pendingUrls.clear();
        // Note: We deliberately keep m_completedQueue to handle images already in flight.
    }

    /**
     * @brief Retrieves the next completed image from the queue.
     * Called by the Main Thread to process results and create textures.
     * @param outData Reference to store the result.
     * @return true if an image was retrieved, false if queue is empty.
     */
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
    ImageLoader() {} // Private Constructor for Singleton

    /**
     * @brief Logic executed on the background thread.
     * Parses URL, performs HTTP GET, and stores result.
     */
    void DownloadThreadFunc(std::string url) {
        // 1. Parse Host and Path from URL
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

        // 2. Perform Network Request
        httplib::Client cli("https://" + host);
        cli.set_connection_timeout(0, 300000);
        cli.enable_server_certificate_verification(false);

        auto res = cli.Get(path.c_str());

        LoadedImageData result;
        result.url = url;

        if (res && res->status == 200) {
            // Copy raw bytes
            result.data = std::vector<unsigned char>(res->body.begin(), res->body.end());
            result.success = true;
        }
        else {
            result.success = false;
        }

        // 3. Store result safely (Critical Section)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_completedQueue.push_back(result);
            m_cache[url] = true; // Mark as processed
        }
    }

    std::mutex m_mutex; // Protects shared resources (queue, maps)
    std::vector<LoadedImageData> m_completedQueue; // Queue of downloaded images ready for the UI
    std::unordered_map<std::string, bool> m_pendingUrls; // Tracks currently downloading URLs
    std::unordered_map<std::string, bool> m_cache; // Tracks finished URLs
};