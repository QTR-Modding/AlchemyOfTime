#pragma once
#include <shared_mutex>
#include <unordered_set>

namespace Lorebox {
    static std::string aot_kw_name = "LoreBox_quantAoT";
    static std::string aot_kw_name_lorebox = "$LoreBox_quantAoT";
    static std::wstring return_str = L"";
    inline RE::BGSKeyword* aot_kw = nullptr;

    inline std::shared_mutex kw_mutex;
    inline std::unordered_set<FormID> kw_added;

    inline std::unordered_set<FormID> kw_removed;

    // hasher for pair<FormID,RefID>
    using HandledKey = std::pair<FormID, RefID>;

    struct HandledKeyHash {
        std::size_t operator()(const HandledKey& k) const noexcept {
            // standard 64-bit mix
            const std::size_t h1 = std::hash<FormID>{}(k.first);
            const std::size_t h2 = std::hash<RefID>{}(k.second);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
        }
    };

    inline std::unordered_set<HandledKey, HandledKeyHash> handled_item_data; // formid of the item and its owner

    // UI toggle: show a title before rows in lorebox
    inline std::atomic show_title{true};
    // UI toggle: show progress percentage per instance (default true)
    inline std::atomic show_percentage{true};
    // UI toggle: show delayer (time modulator) and transformer name
    inline std::atomic show_modulator_name{false};
    // UI toggle: colorize each row based on modulation/transform state
    inline std::atomic colorize_rows{true};
    // UI toggle: append multiplier to modulator tag (e.g., ": x0.5")
    inline std::atomic show_multiplier{false};

    // Configurable colors (0xRRGGBB)
    inline std::atomic<uint32_t> color_title{0xA86CCD}; // title accent
    inline std::atomic<uint32_t> color_neutral{0xE8EAED}; // default light text
    inline std::atomic<uint32_t> color_slow{0xFF8A80}; // light red for x>1 (slower)
    inline std::atomic<uint32_t> color_fast{0x80D8FF}; // light blue for x<1 (faster)
    inline std::atomic<uint32_t> color_transform{0xFFCC80}; // light orange for transform
    inline std::atomic<uint32_t> color_separator{0x9AA0A6}; // bullet/separator color

    // Configurable symbols (ASCII recommended for maximum font support)
    inline std::wstring separator_symbol = L"-"; // bullet default
    inline std::wstring arrow_right = L"->"; // forward arrow
    inline std::wstring arrow_left = L"<-"; // backward arrow

    // Lorebox layout settings
    inline constexpr int MAX_ROWS = 8;

    inline bool Load_KW_AoT() {
        aot_kw = RE::BGSKeyword::CreateKeyword(aot_kw_name);
        return aot_kw != nullptr;
    }

    bool AddKeyword(RE::BGSKeywordForm* a_form, FormID a_formid);

    bool ReAddKW(RE::TESForm* a_form);

    bool RemoveKW(RE::TESForm* a_form);

    void ReAddKWs();

    bool HasKW(const RE::TESForm* a_form);
    bool IsRemoved(FormID a_formid);

    static std::wstring loreboxStr;

    // Returns a UTF-16 (wstring) body to be used as translation result
    std::wstring BuildLoreFor(FormID hovered, RefID ownerId);
    // Build a frozen-state lore string (title optional, time 9999d, percentage 0% if enabled)
    std::wstring BuildFrozenLore();
    std::wstring BuildFrozenLore(const std::wstring& currentStageName);

    extern "C" __declspec(dllexport) const wchar_t* OnDynamicTranslationRequest(std::string_view a_key);
}