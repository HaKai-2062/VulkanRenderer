#pragma once
#include <stdint.h>

struct GLFWwindow;

struct VkInstance_T;
struct VkSurfaceKHR_T;
using VkInstance = VkInstance_T*;
using VkSurfaceKHR = VkSurfaceKHR_T*;

class BackEndWindow
{
public:
    BackEndWindow(const BackEndWindow&) = delete;
    BackEndWindow& operator=(const BackEndWindow&) = delete;
    BackEndWindow(BackEndWindow&&) = delete;
    BackEndWindow& operator=(BackEndWindow&&) = delete;

    static BackEndWindow& Get()
    {
        static BackEndWindow instance;
        return instance;
    }

    static GLFWwindow* GetWindow() { return Get().m_Window; }
    static void CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
    static std::pair<uint32_t, uint32_t> GetWindowSize();
    static void SetWindowTitle(const char* title);
    static void BeginFrame();

    static void CloseWindow();
    static void DestroyWindow();
    static bool ShouldWindowClose();

    static void SetCursorShow();
    static void SetCursorHide();
    static void SetCursorDisable();
    static std::pair<double, double> GetCursorPosition();

    static bool IsCursorShown();
    static bool IsCursorHidden();
    static bool IsCursorDisabled();
    static bool IsKeyPressed(uint32_t key);
    static bool IsWindowMinimized();

    // Use a timer class later on
    static double GetTime();

private:
    BackEndWindow();
    ~BackEndWindow();

private:
    std::pair<uint32_t, uint32_t> m_WindowExtent = { 1920, 1080 };
    GLFWwindow* m_Window{ nullptr };
};