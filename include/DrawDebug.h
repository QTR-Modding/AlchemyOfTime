#pragma once
// #include <boost\type_traits\function_traits.hpp>
#define NOMINMAX
#define GLM_ENABLE_EXPERIMENTAL
// #include <xbyak\xbyak.h>
#include <shared_mutex>
#include <glm/glm.hpp>

namespace DebugAPI_IMPL {
    class DebugAPILine {
    public:
        DebugAPILine(glm::vec3 from, glm::vec3 to, glm::vec4 color, float lineThickness,
                     unsigned __int64 destroyTickCount);

        glm::vec3 From;
        glm::vec3 To;
        glm::vec4 Color;
        float fColor;
        float Alpha;
        float LineThickness;

        unsigned __int64 DestroyTickCount;
    };

    class DebugAPI {
    public:
        static void Update();

        static RE::GPtr<RE::IMenu> GetHUD();

        static void DrawLine2D(const RE::GPtr<RE::GFxMovieView>& movie, glm::vec2 from, glm::vec2 to, float color,
                               float lineThickness, float alpha);
        static void DrawLine2D(const RE::GPtr<RE::GFxMovieView>& movie, glm::vec2 from, glm::vec2 to, glm::vec4 color,
                               float lineThickness);
        static void DrawLine3D(const RE::GPtr<RE::GFxMovieView>& movie, glm::vec3 from, glm::vec3 to, float color,
                               float lineThickness, float alpha);
        static void DrawLine3D(const RE::GPtr<RE::GFxMovieView>& movie, glm::vec3 from, glm::vec3 to, glm::vec4 color,
                               float lineThickness);
        static void ClearLines2D(const RE::GPtr<RE::GFxMovieView>& movie);

        static void DrawLineForMS(const glm::vec3& from, const glm::vec3& to, int liftetimeMS = 10,
                                  const glm::vec4& color = {1.0f, 0.0f, 0.0f, 1.0f}, float lineThickness = 1);
        static void DrawSphere(glm::vec3, float radius, int liftetimeMS = 10,
                               const glm::vec4& color = {1.0f, 0.0f, 0.0f, 1.0f}, float lineThickness = 1);
        static void DrawCircle(glm::vec3, float radius, glm::vec3 eulerAngles, int liftetimeMS = 10,
                               const glm::vec4& color = {1.0f, 0.0f, 0.0f, 1.0f}, float lineThickness = 1);

        static inline std::vector<DebugAPILine*> LinesToDraw;
        static inline std::shared_mutex mutex_;

        static bool DEBUG_API_REGISTERED;

        static constexpr int CIRCLE_NUM_SEGMENTS = 32;

        static constexpr float DRAW_LOC_MAX_DIF = 5.0f;

        static glm::vec2 WorldToScreenLoc(const RE::GPtr<RE::GFxMovieView>& movie, glm::vec3 worldLoc);
        static float RGBToHex(glm::vec3 rgb);

        static void FastClampToScreen(glm::vec2& point);

        // 	static void ClampVectorToScreen(glm::vec2& from, glm::vec2& to);
        // 	static void ClampPointToScreen(glm::vec2& point, float lineAngle);

        static bool IsOnScreen(glm::vec2 from, glm::vec2 to);
        static bool IsOnScreen(glm::vec2 point);

        static void CacheMenuData();

        static bool CachedMenuData;

        static float ScreenResX;
        static float ScreenResY;

    private:
        static float ConvertComponentR(float value);
        static float ConvertComponentG(float value);
        static float ConvertComponentB(float value);
        // returns true if there is already a line with the same color at around the same from and to position
        // with some leniency to bundle together lines in roughly the same spot (see DRAW_LOC_MAX_DIF)
        static DebugAPILine* GetExistingLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color,
                                             float lineThickness);
    };

    class DebugOverlayMenu : RE::IMenu {
    public:
        static constexpr auto MENU_PATH = "BetterThirdPersonSelection/overlay_menu";
        static constexpr auto MENU_NAME = "HUD Menu";

        DebugOverlayMenu();

        static void Register();

        static void Show();
        static void Hide();

        static RE::stl::owner<IMenu*> Creator() { return new DebugOverlayMenu(); }

        void AdvanceMovie(float a_interval, std::uint32_t a_currentTime) override;

    private:
        class Logger : public RE::GFxLog {
        public:
            void LogMessageVarg(LogMessageType, const char* a_fmt, const std::va_list a_argList) override {
                std::string fmt(a_fmt ? a_fmt : "");
                while (!fmt.empty() && fmt.back() == '\n') {
                    fmt.pop_back();
                }

                std::va_list args;
                va_copy(args, a_argList);
                std::vector<char> buf(static_cast<std::size_t>(std::vsnprintf(nullptr, 0, fmt.c_str(), a_argList) + 1));
                std::vsnprintf(buf.data(), buf.size(), fmt.c_str(), args);
                va_end(args);

                logger::info("{}", buf.data());
            }
        };
    };

    namespace DrawDebug {
        namespace Colors {
            static constexpr auto RED = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
            static constexpr auto GRN = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
            static constexpr auto BLU = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        }

        template <int time = 100>
        void draw_line(const RE::NiPoint3& _from, const RE::NiPoint3& _to, const float size = 5.0f,
                       const glm::vec4 Color = Colors::RED) {
            const glm::vec3 from(_from.x, _from.y, _from.z);
            const glm::vec3 to(_to.x, _to.y, _to.z);
            DebugAPI::DrawLineForMS(from, to, time, Color, size);
        }


        template <glm::vec4 Color = Colors::RED>
        void draw_sphere(const RE::NiPoint3& _center, const float r = 5.0f, const float size = 5.0f,
                         const int time = 3000) {
            const glm::vec3 center(_center.x, _center.y, _center.z);
            DebugAPI::DrawSphere(center, r, time, Color, size);
        }
    }
}

using namespace DebugAPI_IMPL::DrawDebug;