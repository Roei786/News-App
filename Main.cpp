#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

#include "NewsClient.h"
#include "ImageLoader.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <vector>
#include <string>
#include <algorithm>
#include <shellapi.h> 

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "shell32.lib")

// --- Global DirectX Variables ---
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
ImFont* fontRegular = nullptr;
ImFont* fontBold = nullptr;

// --- Forward Declarations ---
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Creates a DirectX 11 Shader Resource View from raw image memory.
 * Uses stb_image to decode the raw bytes (JPG/PNG) into pixel data.
 * This MUST run on the Main Thread because DirectX 11 is not thread-safe for creation.
 */
bool CreateTextureFromMemory(const unsigned char* data, int dataSize, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height) {
    int image_width = 0;
    int image_height = 0;
    int channels = 0;

    // Decode image
    unsigned char* image_data = stbi_load_from_memory(data, dataSize, &image_width, &image_height, &channels, 4);
    if (image_data == NULL) return false;

    // Setup Texture Descriptor
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = image_width;
    desc.Height = image_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    // Define initial data
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = image_data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;

    // Create the texture object
    ID3D11Texture2D* pTexture = NULL;
    g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    // Create the View (SRV) for the shader/ImGui
    if (pTexture != NULL) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        ZeroMemory(&srvDesc, sizeof(srvDesc));
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = desc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
        g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
        pTexture->Release();
    }

    stbi_image_free(image_data);
    *out_width = image_width;
    *out_height = image_height;

    return *out_srv != nullptr;
}

// Configures global ImGui style (Rounding, Colors, Spacing)
void SetupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.ItemSpacing = ImVec2(12, 12);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_Text] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.90f, 0.92f, 0.95f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.85f, 0.85f, 0.90f, 1.00f);
}

// --- MAIN ENTRY POINT ---
int main(int, char**)
{
    // 1. Create Window Class and Window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"NewsAggregator", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"News Grid", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // 2. Initialize Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // 3. Setup ImGui Context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    SetupStyle();

    // Load Fonts
    try {
        fontRegular = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
        fontBold = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 22.0f);
    }
    catch (...) {}

    if (fontRegular == nullptr) fontRegular = io.Fonts->AddFontDefault();
    if (fontBold == nullptr) fontBold = fontRegular;

    // 4. Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Application State
    NewsClient newsClient;
    static int selectedCategory = 0;
    const char* categories[] = { "General", "Technology", "Business", "Sports", "Science", "Saved" };

    std::vector<NewsItem> currentNews;
    bool isLoading = false;
    bool firstRun = true;

    // --- Main Loop ---
    while (true)
    {
        static char searchBuffer[128] = "";

        // Poll and handle Windows messages
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) goto Done;
        }

        // Initial Data Fetch
        if (firstRun) {
            isLoading = true;
            newsClient.fetchNewsAsync(categories[selectedCategory]);
            firstRun = false;
        }

        // --- Process Downloaded Images ---
        // Check if the ImageLoader thread has finished any downloads.
        // If so, create the DirectX texture here on the main thread.
        LoadedImageData loadedImg;
        while (ImageLoader::Get().TryGetNextImage(loadedImg)) {
            if (loadedImg.success) {
                for (auto& news : currentNews) {
                    if (news.imageUrl == loadedImg.url) {
                        CreateTextureFromMemory(loadedImg.data.data(), (int)loadedImg.data.size(), &news.texture, &news.width, &news.height);
                        news.imageLoaded = true;
                    }
                }
            }
        }

        // Start ImGui Frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Setup Main Window (Fullscreen inside the OS Window)
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

        // --- 1. Top Bar (Categories & Search) ---
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
        ImGui::BeginChild("TopBar", ImVec2(0, 60), false);
        ImGui::SetCursorPos(ImVec2(20, 15));
        ImGui::TextColored(ImVec4(1, 1, 1, 1), "NEWS FEED");

        ImGui::SameLine(150);
        ImGui::SetCursorPosY(12);

        // Render Category Buttons
        for (int i = 0; i < IM_ARRAYSIZE(categories); i++) {
            if (i > 0) ImGui::SameLine();
            bool isSelected = (selectedCategory == i);

            // Highlight selected category
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.1f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1));
            }

            if (ImGui::Button(categories[i], ImVec2(100, 35))) {
                selectedCategory = i;

                // Cleanup previous textures to avoid memory leaks
                for (auto& item : currentNews) {
                    if (item.texture) {
                        item.texture->Release();
                        item.texture = nullptr;
                    }
                }
                currentNews.clear();

                // Clear ImageLoader cache to allow re-downloading images if revisited
                ImageLoader::Get().ClearCache();

                // Logic for "Saved" vs API Categories
                if (std::string(categories[i]) == "Saved") {
                    currentNews = newsClient.getBookmarks();
                    isLoading = false;
                }
                else {
                    isLoading = true;
                    newsClient.fetchNewsAsync(categories[i]);
                }
            }
            ImGui::PopStyleColor(2);
        }

        // --- Search Box Logic ---
        float searchWidth = 200.0f;
        float rightAlign = ImGui::GetWindowWidth() - (searchWidth + 100);

        if (rightAlign > ImGui::GetCursorPosX()) {
            ImGui::SameLine(rightAlign);
        }
        else {
            ImGui::SameLine();
        }

        ImGui::PushItemWidth(searchWidth);

        // Search Box Styling (White Background, Black Text)
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));       // White
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));          // Black
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.0f, 0.5f, 1.0f, 0.35f)); // Light Blue

        bool enterPressed = ImGui::InputTextWithHint("##Search", "Search...", searchBuffer, IM_ARRAYSIZE(searchBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::PopStyleColor(3); // Restore colors
        ImGui::PopItemWidth();

        ImGui::SameLine();

        if (ImGui::Button("Search") || enterPressed) {
            if (strlen(searchBuffer) > 0) {
                // Clear current data
                for (auto& item : currentNews) {
                    if (item.texture) { item.texture->Release(); item.texture = nullptr; }
                }
                currentNews.clear();
                ImageLoader::Get().ClearCache();

                isLoading = true;
                selectedCategory = -1; // Deselect categories
                newsClient.searchNewsAsync(searchBuffer);
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        // --- 2. Content Area (News Grid) ---

        // Check if async data is ready
        if (newsClient.isDataReady()) {
            currentNews = newsClient.getNews();
            isLoading = false;
        }

        ImGui::SetCursorPos(ImVec2(20, 80));

        // Handle Loading / Empty states
        if (isLoading) {
            ImGui::Text("Loading news...");
        }
        else if (currentNews.empty()) {
            ImGui::Text(selectedCategory == 5 ? "No saved bookmarks." : "No news found.");
        }
        else {
            // --- Render Centered Grid ---

            float availWidth = ImGui::GetContentRegionAvail().x;
            float cardWidth = 450.0f;
            float spacing = 25.0f;

            // Calculate columns
            int columns = (int)(availWidth / (cardWidth + spacing));
            if (columns < 1) columns = 1;

            // Calculate centering offset
            float totalGridWidth = (columns * cardWidth) + ((columns - 1) * spacing);
            float startX = (availWidth - totalGridWidth) * 0.5f;
            if (startX < 0) startX = 0;

            // Define grid window with full available width to prevent clipping crashes
            ImGui::BeginChild("Grid", ImVec2(availWidth, 0), false);

            // Indent to center the grid content
            ImGui::Indent(startX);

            for (int i = 0; i < currentNews.size(); i++) {
                ImGui::PushID(i); // Unique ID for ImGui elements

                ImGui::BeginChild("Card", ImVec2(cardWidth, 360), true);

                // A. Image Loading Request
                if (!currentNews[i].imageLoaded && !currentNews[i].imageUrl.empty()) {
                    ImageLoader::Get().LoadImageAsync(currentNews[i].imageUrl);
                }

                // B. Render Image
                if (currentNews[i].texture) {
                    float aspectRatio = (float)currentNews[i].width / (float)currentNews[i].height;
                    float imgHeight = 200.0f;
                    float imgWidth = imgHeight * aspectRatio;

                    if (imgWidth > cardWidth) {
                        imgWidth = cardWidth;
                        imgHeight = imgWidth / aspectRatio;
                    }

                    // Center image inside card
                    float offsetX = (cardWidth - imgWidth) * 0.5f;
                    ImGui::SetCursorPosX(offsetX);

                    ImGui::Image((void*)currentNews[i].texture, ImVec2(imgWidth, imgHeight));
                }
                else {
                    // Placeholder
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                    ImGui::Button(" ", ImVec2(cardWidth, 200));
                    ImGui::PopStyleColor();
                }

                ImGui::Spacing();

                // C. Render Title
                ImGui::PushFont(fontBold);
                std::string title = currentNews[i].title;
                if (title.length() > 80) title = title.substr(0, 77) + "...";

                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + cardWidth - 10);
                ImGui::TextColored(ImVec4(0.1f, 0.1f, 0.1f, 1.0f), "%s", title.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopFont();

                // D. Render Date
                ImGui::PushFont(fontRegular);
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", currentNews[i].date.c_str());

                ImGui::Spacing();

                // E. Render Content
                std::string content = currentNews[i].content;
                if (content.length() > 120) content = content.substr(0, 117) + "...";

                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + cardWidth - 10);
                ImGui::TextColored(ImVec4(0.3f, 0.3f, 0.3f, 1.0f), "%s", content.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopFont();

                // F. Footer Buttons (Fixed at bottom)
                float currentY = ImGui::GetCursorPosY();
                if (currentY < 320) ImGui::SetCursorPosY(320);

                ImGui::Separator();

                // Read More Button
                if (ImGui::Button("READ MORE", ImVec2(140, 0))) {
                    ShellExecuteA(NULL, "open", currentNews[i].readMoreUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
                }

                ImGui::SameLine();
                float availableSpace = ImGui::GetContentRegionAvail().x;

                // Bookmark Button
                bool isSaved = currentNews[i].isSaved;
                if (isSaved) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                    if (ImGui::Button("SAVED", ImVec2(availableSpace, 0))) {
                        newsClient.toggleBookmark(currentNews[i]);
                        if (std::string(categories[selectedCategory]) == "Saved") {
                            currentNews = newsClient.getBookmarks();
                        }
                    }
                    ImGui::PopStyleColor(2);
                }
                else {
                    if (ImGui::Button("Bookmark", ImVec2(availableSpace, 0))) {
                        newsClient.toggleBookmark(currentNews[i]);
                    }
                }

                ImGui::EndChild();
                ImGui::PopID();

                // G. New Line Logic
                if ((i + 1) % columns != 0) {
                    ImGui::SameLine(0, spacing);
                }
                else {
                    ImGui::Dummy(ImVec2(0, spacing));
                }
            }
            ImGui::Unindent(startX); // Un-indent at the end
            ImGui::EndChild();
        }

        ImGui::End();
        ImGui::PopStyleVar();

        // Rendering
        ImGui::Render();
        const float clear_color[4] = { 0.92f, 0.92f, 0.94f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

Done:
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

// --- DirectX Boilerplate Functions ---
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK) return false;
    CreateRenderTarget();
    return true;
}
void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}
void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}
void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}