#pragma once
#include "Manager.h"

namespace Hooks {

    /*const uint8_t n_hooks = 0;
    const size_t trampoline_size = n_hooks * 14;*/

    void Install(Manager* mngr);

    template <typename MenuType>
    class MenuHook : public MenuType {
        using ProcessMessage_t = decltype(&MenuType::ProcessMessage);
        static inline REL::Relocation<ProcessMessage_t> _ProcessMessage;
        RE::UI_MESSAGE_RESULTS ProcessMessage_Hook(RE::UIMessage& a_message);
        static inline Manager* M = nullptr;
    public:
        static void InstallHook(const REL::VariantID& varID, Manager* mngr);
    };

    struct UpdateHook {
        static inline RE::TESObjectREFR* object = nullptr;
		static void Update(RE::Actor* a_this, float a_delta);
        static inline REL::Relocation<decltype(Update)> Update_;
		static void Install();
	};

    template <typename RefType>
    class MoveItemHooks {
    public:
        static void install(const bool is_actor = true) {
			REL::Relocation<std::uintptr_t> _vtbl{ RefType::VTABLE[0] };
			if (is_actor) {
			    pick_up_object_ = _vtbl.write_vfunc(0xCC, pickUpObject);
			}
			remove_item_ = _vtbl.write_vfunc(0x56, RemoveItem);
			add_object_to_container_ = _vtbl.write_vfunc(0x5A, addObjectToContainer);
        }

    private:
        static void pickUpObject(RefType* a_this,
                                   RE::TESObjectREFR* a_object,
                                   int32_t a_count,
                                   bool a_arg3,
                                   bool a_play_sound);
        static inline REL::Relocation<decltype(pickUpObject)> pick_up_object_;

        static RE::ObjectRefHandle* RemoveItem(RefType* a_this,
            RE::ObjectRefHandle& a_hidden_return_argument,
            RE::TESBoundObject* a_item,
            std::int32_t a_count,
            RE::ITEM_REMOVE_REASON a_reason,
            RE::ExtraDataList* a_extra_list,
            RE::TESObjectREFR* a_move_to_ref,
            const RE::NiPoint3* a_drop_loc,
            const RE::NiPoint3* a_rotate);
        static inline REL::Relocation<decltype(RemoveItem)> remove_item_;

        static void addObjectToContainer(RefType* a_this,
                                    RE::TESBoundObject* a_object, 
                                    RE::ExtraDataList* a_extraList, 
                                    std::int32_t a_count,
                                    RE::TESObjectREFR* a_fromRefr
                                );
        static inline REL::Relocation<decltype(addObjectToContainer)> add_object_to_container_;

    };

    static void add_item_functor(RE::TESObjectREFR* a_this, RE::TESObjectREFR* a_object, int32_t a_count, bool a4, bool a5);
	static inline REL::Relocation<decltype(add_item_functor)> add_item_functor_;
};