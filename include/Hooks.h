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

    struct ScaleformTranslatorHook {
		static void Translate(RE::BSScaleformTranslator* a_this, RE::GFxTranslator::TranslateInfo* a_translateInfo);
        static inline REL::Relocation<decltype(Translate)> Translate_;
		static void Install();
	};

    struct UpdateHook {
        static inline RE::TESObjectREFR* object = nullptr;
		static void Update(RE::Actor* a_this, float a_delta);
        static inline REL::Relocation<decltype(Update)> Update_;
		static void Install();
	};
};