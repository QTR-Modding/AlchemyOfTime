#include "Hooks.h"
#include "Utils.h"
#include "DrawDebug.h"
#include "Lorebox.h"
#include "ClibUtilsQTR/Tasker.hpp"

using namespace Hooks;


namespace {

    
    std::string wide_to_utf8(const wchar_t* w) {
        if (!w) return {};
        const int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 1) return {};
        std::string out(len - 1, '\0');                 // drop the trailing NUL
        WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), len, nullptr, nullptr);
        return out;
    }

    using TranslateFn = void (*)(RE::GFxTranslator*, RE::GFxTranslator::TranslateInfo*);

    TranslateFn g_OrigTranslateAny = nullptr;
    void**      g_TranslatorVTable = nullptr;  // remembers which vtable we patched

    std::string utf8_from_w(const wchar_t* w)
    {
        return wide_to_utf8(w);
    }

    // Generic hook for any GFxTranslator::Translate (vanilla or custom)
    void AnyTranslator_Translate_Hook(RE::GFxTranslator* a_this, RE::GFxTranslator::TranslateInfo* a_info)
    {
        // Always fall through to original unless we actually handle a specific key
        if (g_OrigTranslateAny) {
            // Optionally: inspect key before calling original
            const auto keyUtf8 = utf8_from_w(a_info->GetKey());

            // Let the current translator do its work first
            g_OrigTranslateAny(a_this, a_info);

            if (is_menu_open && keyUtf8 == Lorebox::aot_kw_name_lorebox) {
                const std::wstring body = Lorebox::BuildLoreForHover();
                a_info->SetResult(body.c_str(), body.size());
                if (body == Lorebox::return_str) {
                    if (const auto selected_item = Menu::GetSelectedItemDataInMenu()) {
					    if (!inventory_update_timeout && Lorebox::RemoveKW(selected_item->objDesc->GetObject())) {
                            SKSE::GetTaskInterface()->AddUITask([] {
                                RE::SendUIMessage::SendInventoryUpdateMessage(RE::PlayerCharacter::GetSingleton(),nullptr);
                                inventory_update_timeout = true;
                                clib_utilsQTR::Tasker::GetSingleton()->PushTask(
                                    [] {
                                        inventory_update_timeout = false;
                                    },
						            inventory_update_timeout_ms
                                );
                            });
                        }
                    }
                }
            }
        }
    }

    bool InstallTranslatorVtableHook()
    {
        const auto sfm    = RE::BSScaleformManager::GetSingleton();
        const auto loader = sfm ? sfm->loader : nullptr;
        const auto tr     = loader ? loader->GetState<RE::GFxTranslator>(RE::GFxState::StateType::kTranslator) : RE::GPtr<RE::GFxTranslator>{};

        if (!tr) {
            logger::warn("Translator not available yet; will skip vtable hook.");
            return false;
        }

        // vtable of the active translator instance
        auto** vtbl = *reinterpret_cast<void***>(tr.get());

        // Resolve the vanilla BSScaleformTranslator vtable pointer for comparison
        const REL::Relocation<std::uintptr_t> baseVtblRel{ RE::BSScaleformTranslator::VTABLE[0] };
        auto** baseVtbl = reinterpret_cast<void**>(baseVtblRel.address());

        // Choose the vtable we will patch: if it's the vanilla class vtable, patch that one; otherwise, patch the instance's
        auto** targetVtbl = vtbl == baseVtbl ? baseVtbl : vtbl;

        // Already patched?
        if (g_TranslatorVTable == targetVtbl && g_OrigTranslateAny) {
            logger::info("Translator vtable already hooked {:p}", static_cast<void*>(targetVtbl));
            return true;
        }

        // Patch vtable slot 0x2 (Translate)
        DWORD oldProt{};
        if (!VirtualProtect(&targetVtbl[2], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt)) {
            logger::error("VirtualProtect failed while preparing to patch translator vtable.");
            return false;
        }

        g_OrigTranslateAny = reinterpret_cast<TranslateFn>(targetVtbl[2]);
        targetVtbl[2] = reinterpret_cast<void*>(&AnyTranslator_Translate_Hook);

        DWORD dummy{};
        VirtualProtect(&targetVtbl[2], sizeof(void*), oldProt, &dummy);

        g_TranslatorVTable = targetVtbl;

        logger::info("Installed Translate vtable hook on translator {:p}", static_cast<void*>(targetVtbl));
        return true;
    }
}



template <typename MenuType>
void MenuHook<MenuType>::InstallHook(const REL::VariantID& varID, Manager* mngr) {
    REL::Relocation<std::uintptr_t> vTable(varID);
    _ProcessMessage = vTable.write_vfunc(0x4, &MenuHook<MenuType>::ProcessMessage_Hook);
	M = mngr;
}

template <typename MenuType>
RE::UI_MESSAGE_RESULTS MenuHook<MenuType>::ProcessMessage_Hook(RE::UIMessage& a_message) {

    if (const std::string_view menuname = MenuType::MENU_NAME; a_message.menu==menuname) {
        const auto msg_type = static_cast<int>(a_message.type.get());
        if (msg_type == 1) {
            if (menuname == RE::FavoritesMenu::MENU_NAME) {
                M->Update(RE::PlayerCharacter::GetSingleton());
            }
            else if (menuname == RE::InventoryMenu::MENU_NAME) {
                M->Update(RE::PlayerCharacter::GetSingleton());
            }
            else if (menuname == RE::BarterMenu::MENU_NAME){
                M->Update(RE::PlayerCharacter::GetSingleton());
                //if (const auto vendor_chest = Menu::GetVendorChestFromMenu()) {
                //    M->Update(vendor_chest);
                //} else logger ::error("Could not get vendor chest.");
            }
            else if (menuname == RE::ContainerMenu::MENU_NAME) {
                if (const auto container = Menu::GetContainerFromMenu()) {
                    M->Update(RE::PlayerCharacter::GetSingleton());
                    M->Update(container);
                } else logger::error("Could not get container.");
            }

			is_menu_open = true;
        }
		else if (msg_type == 3) {
			is_menu_open = false;
            Lorebox::ReAddKWs();
        }
    }
    return _ProcessMessage(this, a_message);
}

void Hooks::Install(Manager* mngr){
    MenuHook<RE::ContainerMenu>::InstallHook(RE::VTABLE_ContainerMenu[0],mngr);
    MenuHook<RE::BarterMenu>::InstallHook(RE::VTABLE_BarterMenu[0],mngr);
    MenuHook<RE::FavoritesMenu>::InstallHook(RE::VTABLE_FavoritesMenu[0],mngr);
	MenuHook<RE::InventoryMenu>::InstallHook(RE::VTABLE_InventoryMenu[0],mngr);

#ifndef NDEBUG
    UpdateHook::Install();
#endif

    MoveItemHooks<RE::PlayerCharacter>::install();
	MoveItemHooks<RE::TESObjectREFR>::install(false);
	MoveItemHooks<RE::Character>::install();

	auto& trampoline = SKSE::GetTrampoline();
    constexpr size_t size_per_hook = 14;
    constexpr size_t NUM_TRAMPOLINE_HOOKS = 2;
	trampoline.create(size_per_hook * NUM_TRAMPOLINE_HOOKS);

	const REL::Relocation<std::uintptr_t> add_item_functor_hook{ RELOCATION_ID(55946, 56490) };
	add_item_functor_ = trampoline.write_call<5>(add_item_functor_hook.address() + 0x15D, add_item_functor);

    const REL::Relocation<std::uintptr_t> function{REL::RelocationID(51019, 51897)};
    InventoryHoverHook::originalFunction = trampoline.write_call<5>(function.address() + REL::Relocate(0x114, 0x22c), InventoryHoverHook::thunk);

    // Install a Translate hook for whatever translator is currently active (vanilla or custom)
    InstallTranslatorVtableHook();
}

void UpdateHook::Update(RE::Actor* a_this, float a_delta)
{
    Update_(a_this, a_delta);
    DebugAPI_IMPL::DebugAPI::Update();
}

void UpdateHook::Install() {
#ifndef NDEBUG
    REL::Relocation<std::uintptr_t> PlayerCharacterVtbl{ RE::VTABLE_PlayerCharacter[0] };
	Update_ = PlayerCharacterVtbl.write_vfunc(0xAD, Update);
#endif
}

void Hooks::add_item_functor(RE::TESObjectREFR* a_this, RE::TESObjectREFR* a_object, int32_t a_count, bool a4, bool a5)
{
	if (M->isUninstalled.load() || M->isLoading.load() || !M->listen_container_change.load() || a_count <= 0) {
		return add_item_functor_(a_this, a_object, a_count, a4, a5);
	}

	add_item_functor_(a_this, a_object, a_count, a4, a5);

    M->Update(nullptr, a_this, a_object, a_count);

}

template<typename RefType>
void Hooks::MoveItemHooks<RefType>::pickUpObject(RefType * a_this, RE::TESObjectREFR * a_object, int32_t a_count, bool a_arg3, bool a_play_sound)
{
	if (M->isUninstalled.load() || M->isLoading.load() || !M->listen_container_change.load() || a_count <= 0) {
		return pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
	}

	const auto from_refid = a_object->GetFormID();

	pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);

	M->Update(nullptr, a_this, a_object->GetBaseObject(), a_count, from_refid);
}

template<typename RefType>
RE::ObjectRefHandle* Hooks::MoveItemHooks<RefType>::RemoveItem(RefType * a_this, RE::ObjectRefHandle & a_hidden_return_argument, RE::TESBoundObject * a_item, std::int32_t a_count, RE::ITEM_REMOVE_REASON a_reason, RE::ExtraDataList * a_extra_list, RE::TESObjectREFR * a_move_to_ref, const RE::NiPoint3 * a_drop_loc, const RE::NiPoint3 * a_rotate)
{
	if (a_move_to_ref || M->isUninstalled.load() || M->isLoading.load() || !M->listen_container_change.load() || a_count <= 0 || a_this == a_move_to_ref) {
		return remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
	}

	auto res = remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);

    M->Update(a_this, a_move_to_ref ? a_move_to_ref : res->get().get(), a_item, a_count);

	return res;
}

template<typename RefType>
void Hooks::MoveItemHooks<RefType>::addObjectToContainer(RefType* a_this, RE::TESBoundObject* a_object, RE::ExtraDataList* a_extraList, std::int32_t a_count, RE::TESObjectREFR* a_fromRefr)
{
	if (M->isUninstalled.load() || M->isLoading.load() || !M->listen_container_change.load() || a_count <= 0 || a_this == a_fromRefr)
	{
		return add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
	}

    add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);

    M->Update(a_fromRefr, a_this, a_object, a_count);
}

int64_t Hooks::InventoryHoverHook::thunk(RE::InventoryEntryData* a1)
{
	// gets called before translation hook

	const auto a_bound = a1->GetObject();
	const auto a_bound_formid = a_bound->GetFormID();
	const auto res = originalFunction(a1);

    if (!inventory_update_timeout && Lorebox::IsRemoved(a_bound_formid)) {
        if (const auto selected_item = Menu::GetSelectedItemDataInMenu();
			selected_item && selected_item->objDesc == a1) {
            if (const auto owner = Menu::GetOwnerOfItem(selected_item);
                owner &&
                Lorebox::BuildLoreFor(a_bound_formid, owner->GetFormID()) != Lorebox::return_str &&
                Lorebox::ReAddKW(a_bound)) {
                SKSE::GetTaskInterface()->AddUITask([] {
                    RE::SendUIMessage::SendInventoryUpdateMessage(RE::PlayerCharacter::GetSingleton(), nullptr);
					inventory_update_timeout = true;
                    clib_utilsQTR::Tasker::GetSingleton()->PushTask(
                        [] {
                            inventory_update_timeout = false;
                        },
						inventory_update_timeout_ms
                    );
                });
                
            }
        }
    }
    return res;
}
