#include "Hooks.h"
#include "CLibUtilsQTR/DrawDebug.hpp"
#include "Lorebox.h"
#include "Manager.h"
#include "Utils.h"

template <typename MenuType>
void Hooks::MenuHook<MenuType>::InstallHook(const REL::VariantID& varID) {
    REL::Relocation<std::uintptr_t> vTable(varID);
    _ProcessMessage = vTable.write_vfunc(0x4, &MenuHook<MenuType>::ProcessMessage_Hook);
    _AdvanceMovie = vTable.write_vfunc(0x6, &MenuHook<MenuType>::AdvanceMovie_Hook);
}

template <typename MenuType>
RE::UI_MESSAGE_RESULTS Hooks::MenuHook<MenuType>::ProcessMessage_Hook(RE::UIMessage& a_message) {
    if (const std::string_view menuname = MenuType::MENU_NAME; a_message.menu == menuname) {
        const auto msg_type = static_cast<int>(a_message.type.get());
        if (msg_type == 1) {
            if (menuname == RE::FavoritesMenu::MENU_NAME) {
                M->UpdateNow(RE::PlayerCharacter::GetSingleton());
            } else if (menuname == RE::InventoryMenu::MENU_NAME) {
                M->UpdateNow(RE::PlayerCharacter::GetSingleton());
            } else if (menuname == RE::BarterMenu::MENU_NAME) {
                M->UpdateNow(RE::PlayerCharacter::GetSingleton());
                //if (const auto vendor_chest = Menu::GetVendorChestFromMenu()) {
                //    M->Update(vendor_chest);
                //} else logger ::error("Could not get vendor chest.");
            } else if (menuname == RE::ContainerMenu::MENU_NAME) {
                if (const auto container = Utils::Menu::GetContainerFromMenu()) {
                    M->UpdateNow(RE::PlayerCharacter::GetSingleton());
                    M->UpdateNow(container);
                } else logger::error("Could not get container.");
            }

            is_menu_open = true;
        } else if (msg_type == 3) {
            is_menu_open = false;
            Lorebox::ReAddKWs();
        }
    }
    return _ProcessMessage(this, a_message);
}

template <typename MenuType>
void Hooks::MenuHook<MenuType>::AdvanceMovie_Hook(float a_interval, std::uint32_t a_currentTime) {
    _AdvanceMovie(this, a_interval, a_currentTime);
    M->ProcessDirtyRefs_();
}

void Hooks::Install() {
    MenuHook<RE::ContainerMenu>::InstallHook(RE::ContainerMenu::VTABLE[0]);
    MenuHook<RE::BarterMenu>::InstallHook(RE::BarterMenu::VTABLE[0]);
    MenuHook<RE::FavoritesMenu>::InstallHook(RE::FavoritesMenu::VTABLE[0]);
    MenuHook<RE::InventoryMenu>::InstallHook(RE::InventoryMenu::VTABLE[0]);

    UpdateHook::Install();

    MoveItemHooks<RE::PlayerCharacter>::install();
    MoveItemHooks<RE::TESObjectREFR>::install(false);
    MoveItemHooks<RE::Character>::install();

    auto& trampoline = SKSE::GetTrampoline();
    constexpr size_t size_per_hook = 14;
    constexpr size_t NUM_TRAMPOLINE_HOOKS = 2;
    trampoline.create(size_per_hook * NUM_TRAMPOLINE_HOOKS);

    const REL::Relocation<std::uintptr_t> add_item_functor_hook{RELOCATION_ID(55946, 56490)};
    add_item_functor_ = trampoline.write_call<5>(add_item_functor_hook.address() + 0x15D, add_item_functor);
}

void Hooks::UpdateHook::Update(RE::Actor* a_this, float a_delta) {
    Update_(a_this, a_delta);
    M->ProcessDirtyRefs_();

    #ifndef NDEBUG
    DebugAPI_IMPL::DebugAPI::GetSingleton()->Update();
    #endif
}

void Hooks::UpdateHook::Install() {
    REL::Relocation<std::uintptr_t> PlayerCharacterVtbl{RE::VTABLE_PlayerCharacter[0]};
    Update_ = PlayerCharacterVtbl.write_vfunc(0xAD, Update);
}

void Hooks::add_item_functor(RE::TESObjectREFR* a_this, RE::TESObjectREFR* a_object, int32_t a_count, bool a4,
                             bool a5) {
    if (M->isUninstalled.load() || M->isLoading.load() || !ListenEnabled() || a_count <= 0) {
        return add_item_functor_(a_this, a_object, a_count, a4, a5);
    }

    add_item_functor_(a_this, a_object, a_count, a4, a5);

    M->Update(nullptr, a_this, a_object, a_count);
}

template <typename RefType>
void Hooks::MoveItemHooks<RefType>::pickUpObject(RefType* a_this, RE::TESObjectREFR* a_object, int32_t a_count,
                                                 bool a_arg3,
                                                 bool a_play_sound) {
    if (M->isUninstalled.load() || M->isLoading.load() || !ListenEnabled() || a_count <= 0) {
        return pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
    }

    const auto from_refid = a_object->GetFormID();

    pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);

    M->Update(nullptr, a_this, a_object->GetBaseObject(), a_count, from_refid);
}

template <typename RefType>
RE::ObjectRefHandle* Hooks::MoveItemHooks<RefType>::RemoveItem(RefType* a_this,
                                                               RE::ObjectRefHandle& a_hidden_return_argument,
                                                               RE::TESBoundObject* a_item, std::int32_t a_count,
                                                               RE::ITEM_REMOVE_REASON a_reason,
                                                               RE::ExtraDataList* a_extra_list,
                                                               RE::TESObjectREFR* a_move_to_ref,
                                                               const RE::NiPoint3* a_drop_loc,
                                                               const RE::NiPoint3* a_rotate) {
    if (a_move_to_ref || M->isUninstalled.load() || M->isLoading.load() || !ListenEnabled() || a_count
        <= 0 || a_this == a_move_to_ref) {
        return remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref,
                            a_drop_loc, a_rotate);
    }

    RE::ObjectRefHandle* res = remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list,
                                            a_move_to_ref,
                                            a_drop_loc, a_rotate);

    M->Update(a_this,
              a_move_to_ref
                  ? a_move_to_ref
                  : res
                  ? res->get().get()
                  : nullptr,
              a_item, a_count);

    return res;
}

template <typename RefType>
void Hooks::MoveItemHooks<RefType>::addObjectToContainer(RefType* a_this, RE::TESBoundObject* a_object,
                                                         RE::ExtraDataList* a_extraList, std::int32_t a_count,
                                                         RE::TESObjectREFR* a_fromRefr) {
    if (M->isUninstalled.load() || M->isLoading.load() || !ListenEnabled() || a_count <= 0 || a_this
        == a_fromRefr) {
        return add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
    }

    add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);

    M->Update(a_fromRefr, a_this, a_object, a_count);
}