#pragma once
#include "CLibUtilsQTR/FormReader.hpp"
#include "CLibUtilsQTR/StringHelpers.hpp"

const auto mod_name = std::string(SKSE::PluginDeclaration::GetSingleton()->GetName());
const auto plugin_version = SKSE::PluginDeclaration::GetSingleton()->GetVersion();
constexpr auto po3path = "Data/SKSE/Plugins/po3_Tweaks.dll";
constexpr auto po3_UoTpath = "Data/SKSE/Plugins/po3_UseOrTake.dll";
inline bool IsPo3Installed() { return std::filesystem::exists(po3path); };
inline bool IsPo3_UoTInstalled() { return std::filesystem::exists(po3_UoTpath); };

const auto po3_err_msgbox = std::format(
    "{}: If you are trying to use Editor IDs, but you must have powerofthree's Tweaks "
    "installed. See mod page for further instructions.",
    mod_name);
const auto general_err_msgbox = std::format("{}: Something went wrong. Please contact the mod author.", mod_name);
const auto init_err_msgbox = std::format("{}: The mod failed to initialize and will be terminated.", mod_name);

std::string DecodeTypeCode(std::uint32_t typeCode);

bool FileIsEmpty(const std::string& filename);

std::vector<std::pair<int, bool>> encodeString(const std::string& inputString);
std::string decodeString(const std::vector<std::pair<int, bool>>& encodedValues);

void hexToRGBA(uint32_t color_code, RE::NiColorA& nicolora);


bool IsFoodItem(const RE::TESForm* form);

bool IsPoisonItem(const RE::TESForm* form);

bool IsMedicineItem(const RE::TESForm* form);

void OverrideMGEFFs(RE::BSTArray<RE::Effect*>& effect_array, const std::vector<FormID>& new_effects,
                    const std::vector<uint32_t>& durations, const std::vector<float>& magnitudes);

inline bool IsDynamicFormID(const FormID a_formID) { return a_formID >= 0xFF000000; }

void FavoriteItem(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner);

[[nodiscard]] bool IsFavorited(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner);

[[nodiscard]] inline bool IsFavorited(const RE::FormID formid, const RE::FormID refid) {
    return IsFavorited(FormReader::GetFormByID<RE::TESBoundObject>(formid),
                       FormReader::GetFormByID<RE::TESObjectREFR>(refid));
}

inline void FavoriteItem(const FormID formid, const FormID refid) {
    FavoriteItem(FormReader::GetFormByID<RE::TESBoundObject>(formid),
                 FormReader::GetFormByID<RE::TESObjectREFR>(refid));
}

[[nodiscard]] inline bool IsPlayerFavorited(RE::TESBoundObject* item) {
    return IsFavorited(item, RE::PlayerCharacter::GetSingleton()->AsReference());
}

void EquipItem(RE::TESBoundObject* item, bool unequip = false);

inline void EquipItem(const FormID formid, const bool unequip = false) {
    EquipItem(FormReader::GetFormByID<RE::TESBoundObject>(formid), unequip);
}

[[nodiscard]] bool IsEquipped(RE::TESBoundObject* item);

[[nodiscard]] inline bool IsEquipped(const FormID formid) {
    return IsEquipped(FormReader::GetFormByID<RE::TESBoundObject>(formid));
}

// https://github.com/SteveTownsend/SmartHarvestSE/blob/f709333c4cedba061ad21b4d92c90a720e20d2b1/src/WorldState/LocationTracker.cpp#L756
bool AreAdjacentCells(RE::TESObjectCELL* cellA, RE::TESObjectCELL* cellB);

namespace String {
    std::string EncodeEscapesToAscii(const std::wstring& ws);

    std::wstring DecodeEscapesFromAscii(const char* s);
}



namespace Types {
    struct FormFormID {
        FormID form_id1;
        FormID form_id2;

        bool operator<(const FormFormID& other) const {
            // Compare form_id1 first
            if (form_id1 < other.form_id1) {
                return true;
            }
            // If form_id1 is equal, compare form_id2
            if (form_id1 == other.form_id1 && form_id2 < other.form_id2) {
                return true;
            }
            // If both form_id1 and form_id2 are equal or if form_id1 is greater, return false
            return false;
        }
    };

    struct FormEditorID {
        FormID form_id = 0;
        std::string editor_id;

        bool operator<(const FormEditorID& other) const;
    };

    struct FormEditorIDX : FormEditorID {
        bool is_fake = false;
        bool is_decayed = false;
        bool is_transforming = false;
        bool crafting_allowed = false;


        bool operator==(const FormEditorIDX& other) const;
    };
};

namespace Math {
    float Round(float value, int n);
    float Ceil(float value, int n);

    namespace LinAlg {
        namespace R3 {
            void rotateX(RE::NiPoint3& v, float angle);

            // Function to rotate a vector around the y-axis
            void rotateY(RE::NiPoint3& v, float angle);

            // Function to rotate a vector around the z-axis
            void rotateZ(RE::NiPoint3& v, float angle);

            void rotate(RE::NiPoint3& v, float angleX, float angleY, float angleZ);
        };

        std::array<RE::NiPoint3, 3> GetClosest3Vertices(const std::array<RE::NiPoint3, 8>& a_bounding_box,
                                                        const RE::NiPoint3& outside_point);
        std::array<RE::NiPoint3, 3> GetClosest3Vertices(const std::array<RE::NiPoint3, 4>& a_bounded_plane,
                                                        const RE::NiPoint3& outside_point);
        RE::NiPoint3 CalculateNormalOfPlane(const RE::NiPoint3& span1, const RE::NiPoint3& span2);
        RE::NiPoint3 closestPointOnPlane(const RE::NiPoint3& a_point_on_plane, const RE::NiPoint3& a_point_not_on_plane,
                                         const RE::NiPoint3& v_normal);
        RE::NiPoint3 intersectLine(const std::array<RE::NiPoint3, 3>& vertices,
                                   const RE::NiPoint3& outside_plane_point);
    };
};

namespace MsgBoxesNotifs {
    namespace Windows {
        inline int Po3ErrMsg() {
            MessageBoxA(nullptr, po3_err_msgbox.c_str(), "Error", MB_OK | MB_ICONERROR);
            return 1;
        };
    };

    namespace InGame {
        inline void CustomMsg(const std::string& msg) { RE::DebugMessageBox((mod_name + ": " + msg).c_str()); };
        inline void GeneralErr() { RE::DebugMessageBox(general_err_msgbox.c_str()); };
        inline void InitErr() { RE::DebugMessageBox(init_err_msgbox.c_str()); };
    };
};

namespace WorldObject {
    int16_t GetObjectCount(RE::TESObjectREFR* ref);
    void SetObjectCount(RE::TESObjectREFR* ref, Count count);

    RE::TESObjectREFR* DropObjectIntoTheWorld(RE::TESBoundObject* obj, Count count, bool player_owned = true);

    void SwapObjects(RE::TESObjectREFR* a_from, RE::TESBoundObject* a_to, bool apply_havok = true);

    RE::TESObjectREFR* TryToGetRefInCell(FormID baseid, Count count, float radius = 180);

    bool IsPlacedObject(RE::TESObjectREFR* ref);

    RE::bhkRigidBody* GetRigidBody(const RE::TESObjectREFR* refr);

    RE::NiPoint3 GetPosition(const RE::TESObjectREFR* obj);

    bool AreClose(const RE::TESObjectREFR* a_obj1, const RE::TESObjectREFR* a_obj2, float threshold);
};

namespace Inventory {
    bool IsQuestItem(FormID formid, RE::TESObjectREFR* inv_owner);
};

namespace Menu {
    RE::TESObjectREFR* GetContainerFromMenu();

    RE::TESObjectREFR* GetVendorChestFromMenu();

    void UpdateItemList();

    template <typename MenuType>
    RE::StandardItemData* GetSelectedItemData() {
        if (const auto ui = RE::UI::GetSingleton()) {
            if (const auto menu = ui->GetMenu<MenuType>()) {
                if (RE::ItemList* a_itemList = menu->GetRuntimeData().itemList) {
                    if (auto* item = a_itemList->GetSelectedItem()) {
                        return &item->data;
                    }
                }
            }
        }
        return nullptr;
    }

    RE::StandardItemData* GetSelectedItemDataInMenu(std::string& a_menuOut);
    RE::StandardItemData* GetSelectedItemDataInMenu();

    RE::TESObjectREFR* GetOwnerOfItem(const RE::StandardItemData* a_itemdata);
};

namespace DynamicForm {
    void copyBookAppearence(RE::TESForm* source, RE::TESForm* target);

    template <class T>
    static void copyComponent(RE::TESForm* from, RE::TESForm* to) {
        auto fromT = from->As<T>();

        auto toT = to->As<T>();

        if (fromT && toT) {
            toT->CopyComponent(fromT);
        }
    }

    void copyFormArmorModel(RE::TESForm* source, RE::TESForm* target);

    void copyFormObjectWeaponModel(RE::TESForm* source, RE::TESForm* target);

    void copyMagicEffect(RE::TESForm* source, RE::TESForm* target);

    void copyAppearence(RE::TESForm* source, RE::TESForm* target);
};


template <typename T>
struct FormTraits {
    static float GetWeight(T* form) {
        // Default implementation, assuming T has a member variable 'weight'
        return form->weight;
    }

    static void SetWeight(T* form, float weight) {
        // Default implementation, set the weight if T has a member variable 'weight'
        form->weight = weight;
    }

    static int GetValue(T* form) {
        // Default implementation, assuming T has a member variable 'value'
        return form->value;
    }

    static void SetValue(T* form, int value) {
        form->value = value;
    }

    static RE::BSTArray<RE::Effect*> GetEffects(T*) {
        RE::BSTArray<RE::Effect*> effects;
        return effects;
    }
};

template <>
struct FormTraits<RE::AlchemyItem> {
    static float GetWeight(const RE::AlchemyItem* form) {
        return form->weight;
    }

    static void SetWeight(RE::AlchemyItem* form, const float weight) {
        form->weight = weight;
    }

    static int GetValue(const RE::AlchemyItem* form) {
        return form->GetGoldValue();
    }

    static void SetValue(RE::AlchemyItem* form, const int value) {
        logger::trace("CostOverride: {}", form->data.costOverride);
        form->data.costOverride = value;
    }

    static RE::BSTArray<RE::Effect*> GetEffects(RE::AlchemyItem* form) {
        return form->effects;
    }
};

template <>
struct FormTraits<RE::IngredientItem> {
    static float GetWeight(const RE::IngredientItem* form) {
        return form->weight;
    }

    static void SetWeight(RE::IngredientItem* form, const float weight) {
        form->weight = weight;
    }

    static int GetValue(const RE::IngredientItem* form) {
        return form->GetGoldValue();
    }

    static void SetValue(RE::IngredientItem* form, const int value) {
        form->value = value;
    }

    static RE::BSTArray<RE::Effect*> GetEffects(RE::IngredientItem* form) {
        return form->effects;
    }
};

template <>
struct FormTraits<RE::TESAmmo> {
    static float GetWeight(const RE::TESAmmo* form) {
        // Default implementation, assuming T has a member variable 'weight'
        return form->GetWeight();
    }

    static void SetWeight(RE::TESAmmo*, float) {
    }

    static int GetValue(const RE::TESAmmo* form) {
        // Default implementation, assuming T has a member variable 'value'
        return form->value;
    }

    static void SetValue(RE::TESAmmo* form, const int value) { form->value = value; }

    static RE::BSTArray<RE::Effect*> GetEffects(RE::TESAmmo*) {
        RE::BSTArray<RE::Effect*> effects;
        return effects;
    }
};