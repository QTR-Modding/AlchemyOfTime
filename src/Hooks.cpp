#include "Hooks.h"
#include "Utils.h"
#include "DrawDebug.h"
#include "Lorebox.h"
#include "ClibUtilsQTR/Tasker.hpp"

using namespace Hooks;

template <typename MenuType>
void MenuHook<MenuType>::InstallHook(const REL::VariantID& varID, Manager* mngr) {
    REL::Relocation<std::uintptr_t> vTable(varID);
    _ProcessMessage = vTable.write_vfunc(0x4, &MenuHook<MenuType>::ProcessMessage_Hook);
	M = mngr;
}

template <typename MenuType>
RE::UI_MESSAGE_RESULTS MenuHook<MenuType>::ProcessMessage_Hook(RE::UIMessage& a_message) {

    if (const std::string_view menuname = MenuType::MENU_NAME; a_message.menu==menuname) {
        if (const auto msg_type = static_cast<int>(a_message.type.get()); msg_type == 1) {
            if (menuname == RE::FavoritesMenu::MENU_NAME) {
                logger::trace("Favorites menu is open.");
                M->Update(RE::PlayerCharacter::GetSingleton());
            }
            else if (menuname == RE::InventoryMenu::MENU_NAME) {
                logger::trace("Inventory menu is open.");
                M->Update(RE::PlayerCharacter::GetSingleton());
            }
            else if (menuname == RE::BarterMenu::MENU_NAME){
                logger::trace("Barter menu is open.");
                M->Update(RE::PlayerCharacter::GetSingleton());
                //if (const auto vendor_chest = Menu::GetVendorChestFromMenu()) {
                //    M->Update(vendor_chest);
                //} else logger ::error("Could not get vendor chest.");
            } 
            else if (menuname == RE::ContainerMenu::MENU_NAME) {
                logger::trace("Container menu is open.");
                if (const auto container = Menu::GetContainerFromMenu()) {
                    M->Update(RE::PlayerCharacter::GetSingleton());
                    M->Update(container);
                } else logger::error("Could not get container.");
            }
        }
    }
    return _ProcessMessage(this, a_message);
}

void Hooks::Install(Manager* mngr){
    MenuHook<RE::ContainerMenu>::InstallHook(RE::VTABLE_ContainerMenu[0],mngr);
    MenuHook<RE::BarterMenu>::InstallHook(RE::VTABLE_BarterMenu[0],mngr);
    MenuHook<RE::FavoritesMenu>::InstallHook(RE::VTABLE_FavoritesMenu[0],mngr);
	MenuHook<RE::InventoryMenu>::InstallHook(RE::VTABLE_InventoryMenu[0],mngr);

    ScaleformTranslatorHook::Install();

    UpdateHook::Install();
}

inline std::string wide_to_utf8(const wchar_t* w) {
    if (!w) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return {};
    std::string out(len - 1, '\0');                 // drop the trailing NUL
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), len, nullptr, nullptr);
    return out;
}

static bool safe_cstr(const char* p, size_t max = 4096) {
    // MSVC CRT has strnlen_s that won’t crash on bad ptrs (uses SEH).
    // Fallback still risks AV; use only if you trust the pointer domain.
#ifdef _MSC_VER
    size_t len = strnlen_s(p, max);
    return len > 0 && len < max;
#else
    if (!p) return false;
    for (size_t i = 0; i < max; ++i) {
        volatile char c = p[i];          // force a read
        if (c == '\0') return true;
    }
    return false; // no terminator within limit
#endif
}

void ScaleformTranslatorHook::Translate(RE::BSScaleformTranslator* a_this, RE::GFxTranslator::TranslateInfo* a_translateInfo) {
    if (a_translateInfo && wide_to_utf8(a_translateInfo->GetKey()) == "$LoreBox_DBArmor") {
        Translate_(a_this, a_translateInfo);
		wchar_t* buffer = new wchar_t[256];
		swprintf_s(buffer, 256, L"ASDASD");
        a_translateInfo->SetResult(buffer);
		delete[] buffer;
        return;
    }
    Translate_(a_this, a_translateInfo);
}

void ScaleformTranslatorHook::Install() {
    REL::Relocation<std::uintptr_t> ScaleformTranslatorVtbl{ RE::BSScaleformTranslator::VTABLE[0] };
	Translate_ = ScaleformTranslatorVtbl.write_vfunc(0x2, Translate);
}

void UpdateHook::Update(RE::Actor* a_this, float a_delta)
{
        Update_(a_this, a_delta);
#ifndef NDEBUG
        DebugAPI_IMPL::DebugAPI::Update();
#endif
}

void UpdateHook::Install() {
#ifndef NDEBUG
    REL::Relocation<std::uintptr_t> PlayerCharacterVtbl{ RE::VTABLE_PlayerCharacter[0] };
	Update_ = PlayerCharacterVtbl.write_vfunc(0xAD, Update);
#endif
}