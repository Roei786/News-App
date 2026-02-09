#include "NewsClient.h" 
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <vector>
#include <string>
#include <algorithm>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

// --- משתנים גלובליים ---
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// --- הצהרות ---
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// --- עיצוב ---
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

int main(int, char**)
{
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"NewsAggregator", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"News Grid", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    SetupStyle();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    NewsClient newsClient;
    static int selectedCategory = 0;
    const char* categories[] = { "General", "Technology", "Business", "Sports", "Science" };

    std::vector<NewsItem> currentNews;
    bool isLoading = false;
    bool firstRun = true;

    // --- לולאה ראשית ---
    while (true)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) goto Done;
        }

        if (firstRun) {
            isLoading = true;
            newsClient.fetchNewsAsync(categories[selectedCategory]);
            firstRun = false;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

        // --- בר עליון ---
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
        ImGui::BeginChild("TopBar", ImVec2(0, 60), false);
        ImGui::SetCursorPos(ImVec2(20, 15));
        ImGui::TextColored(ImVec4(1, 1, 1, 1), "NEWS FEED");

        ImGui::SameLine(150);
        ImGui::SetCursorPosY(12);
        for (int i = 0; i < IM_ARRAYSIZE(categories); i++) {
            if (i > 0) ImGui::SameLine();
            if (i == selectedCategory) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.1f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1));
            }
            if (ImGui::Button(categories[i], ImVec2(100, 35))) {
                selectedCategory = i;
                isLoading = true;
                currentNews.clear();
                newsClient.fetchNewsAsync(categories[i]);
            }
            ImGui::PopStyleColor(2);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // --- תוכן ---
        if (newsClient.isDataReady()) {
            currentNews = newsClient.getNews();
            isLoading = false;
        }

        ImGui::SetCursorPos(ImVec2(20, 80));
        if (isLoading) {
            ImGui::Text("Loading news...");
        }
        else if (currentNews.empty()) {
            ImGui::Text("No news found. Check your API Key.");
        }
        else {
            // חישוב גריד
            float availWidth = ImGui::GetContentRegionAvail().x;
            float cardWidth = 300.0f;
            float spacing = 20.0f;
            int columns = (int)(availWidth / (cardWidth + spacing));
            if (columns < 1) columns = 1;

            ImGui::BeginChild("Grid", ImVec2(0, 0), false);

            for (int i = 0; i < currentNews.size(); i++) {
                ImGui::PushID(i); // חובה!

                // כרטיס
                ImGui::BeginChild("Card", ImVec2(cardWidth, 220), true);

                // התיקון הקריטי: שם הכפתור הוא "##img" (נסתר) במקום ריק
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                ImGui::Button("##img", ImVec2(-1, 80));
                ImGui::PopStyleColor();

                ImGui::Spacing();
                std::string title = currentNews[i].title;
                if (title.length() > 50) title = title.substr(0, 47) + "...";
                ImGui::TextColored(ImVec4(0, 0, 0.5f, 1), "%s", title.c_str());

                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "%s", currentNews[i].date.c_str());
                ImGui::Separator();

                ImGui::BeginChild("Content", ImVec2(0, 40));
                ImGui::TextWrapped("%s", currentNews[i].content.c_str());
                ImGui::EndChild();

                if (ImGui::Button("Read More", ImVec2(-1, 0))) {
                    // כאן ייפתח הקישור
                }

                ImGui::EndChild(); // סיום כרטיס
                ImGui::PopID();

                if ((i + 1) % columns != 0) {
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + spacing);
                }
                else {
                    ImGui::Dummy(ImVec2(0, spacing));
                }
            }
            ImGui::EndChild();
        }

        ImGui::End();
        ImGui::PopStyleVar();

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

// --- פונקציות טכניות ---
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