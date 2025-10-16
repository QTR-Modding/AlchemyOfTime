#pragma once
#include <windows.h>
#include <shared_mutex>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/writer.h>
#include "CLibUtilsQTR/FormReader.hpp"

const auto mod_name = static_cast<std::string>(SKSE::PluginDeclaration::GetSingleton()->GetName());
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

void FavoriteItem(const RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner);

[[nodiscard]] bool IsFavorited(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner);

[[nodiscard]] inline bool IsFavorited(const RE::FormID formid, const RE::FormID refid) {
    return IsFavorited(FormReader::GetFormByID<RE::TESBoundObject>(formid), FormReader::GetFormByID<RE::TESObjectREFR>(refid));
}

inline void FavoriteItem(const FormID formid, const FormID refid) {
	FavoriteItem(FormReader::GetFormByID<RE::TESBoundObject>(formid), FormReader::GetFormByID<RE::TESObjectREFR>(refid));
}

[[nodiscard]] inline bool IsPlayerFavorited(RE::TESBoundObject* item) {
    return IsFavorited(item, RE::PlayerCharacter::GetSingleton()->AsReference());
}

void EquipItem(const RE::TESBoundObject* item, bool unequip = false);

inline void EquipItem(const FormID formid, const bool unequip = false) {
	EquipItem(FormReader::GetFormByID<RE::TESBoundObject>(formid), unequip);
}

[[nodiscard]] bool IsEquipped(RE::TESBoundObject* item);

[[nodiscard]] inline bool IsEquipped(const FormID formid) {
	return IsEquipped(FormReader::GetFormByID<RE::TESBoundObject>(formid));
}

template <typename T>
void ForEachForm(std::function<bool(T*)> a_callback) {
    const auto& [map,lock] = RE::TESForm::GetAllForms();
	RE::BSReadLockGuard locker{ lock };
    for (auto& [id, form] : *map) {
		if (!form) continue;
		if (!form->Is(T::FORMTYPE)) continue;
		if (auto* casted_form = skyrim_cast<T*>(form); casted_form && a_callback(casted_form)) return;
    }
};

// https://github.com/SteveTownsend/SmartHarvestSE/blob/f709333c4cedba061ad21b4d92c90a720e20d2b1/src/WorldState/LocationTracker.cpp#L756
bool AreAdjacentCells(RE::TESObjectCELL* cellA, RE::TESObjectCELL* cellB);

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
        FormID form_id=0;
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

namespace Vector {
    template <typename T>
    inline bool HasElement(const std::vector<T>& vec, const T& element) { return std::find(vec.begin(), vec.end(), element) != vec.end(); };
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

        class Geometry {
            std::vector<RE::NiPoint3> positions;
            std::vector<uint16_t> indexes;
            const RE::TESObjectREFR* obj;

	        void FetchVertices(const RE::BSGeometry* o3d, RE::BSGraphics::TriShape* triShape);


        public:
            static RE::NiPoint3 Rotate(const RE::NiPoint3& A, const RE::NiPoint3& angles);

	        ~Geometry() = default;
            explicit Geometry(const RE::TESObjectREFR* obj);
            [[nodiscard]] std::pair<RE::NiPoint3, RE::NiPoint3> GetBoundingBox() const;
        };

        std::array<RE::NiPoint3,3> GetClosest3Vertices(const std::array<RE::NiPoint3, 8>& a_bounding_box, const RE::NiPoint3& outside_point);
        std::array<RE::NiPoint3,3> GetClosest3Vertices(const std::array<RE::NiPoint3, 4>& a_bounded_plane, const RE::NiPoint3& outside_point);
        RE::NiPoint3 CalculateNormalOfPlane(const RE::NiPoint3& span1, const RE::NiPoint3& span2);
        RE::NiPoint3 closestPointOnPlane(const RE::NiPoint3& a_point_on_plane,const RE::NiPoint3& a_point_not_on_plane, const RE::NiPoint3& v_normal);
        RE::NiPoint3 intersectLine(const std::array<RE::NiPoint3,3>& vertices, const RE::NiPoint3& outside_plane_point);
    };
};

namespace String {

    template <typename T>
    std::string join(const T& container, const std::string_view& delimiter) {
        std::ostringstream oss;
        auto iter = container.begin();

        if (iter != container.end()) {
            oss << *iter;
            ++iter;
        }

        for (; iter != container.end(); ++iter) {
            oss << delimiter << *iter;
        }

        return oss.str();
    }

    std::string toLowercase(const std::string& str);

    std::string replaceLineBreaksWithSpace(const std::string& input);

    std::string trim(const std::string& str);

    bool includesWord(const std::string& input, const std::vector<std::string>& strings);
};

namespace MsgBoxesNotifs {
    namespace Windows {

        inline int Po3ErrMsg() {
            MessageBoxA(nullptr, po3_err_msgbox.c_str(), "Error", MB_OK | MB_ICONERROR);
            return 1;
        };
    };

    namespace InGame{
        inline void CustomMsg(const std::string& msg) { RE::DebugMessageBox((mod_name + ": " + msg).c_str()); };
        inline void GeneralErr() { RE::DebugMessageBox(general_err_msgbox.c_str()); };
        inline void InitErr() { RE::DebugMessageBox(init_err_msgbox.c_str()); };
    };
    
};

namespace xData {

    namespace Copy {
        void CopyEnchantment(const RE::ExtraEnchantment* from, RE::ExtraEnchantment* to);
            
        void CopyHealth(const RE::ExtraHealth* from, RE::ExtraHealth* to);

        void CopyRank(const RE::ExtraRank* from, RE::ExtraRank* to);

        void CopyTimeLeft(const RE::ExtraTimeLeft* from, RE::ExtraTimeLeft* to);

        void CopyCharge(const RE::ExtraCharge* from, RE::ExtraCharge* to);

        void CopyScale(const RE::ExtraScale* from, RE::ExtraScale* to);

        void CopyUniqueID(const RE::ExtraUniqueID* from, RE::ExtraUniqueID* to);

        void CopyPoison(const RE::ExtraPoison* from, RE::ExtraPoison* to);

        void CopyObjectHealth(const RE::ExtraObjectHealth* from, RE::ExtraObjectHealth* to);

        void CopyLight(const RE::ExtraLight* from, RE::ExtraLight* to);

        void CopyRadius(const RE::ExtraRadius* from, RE::ExtraRadius* to);

        void CopyHorse(const RE::ExtraHorse* from, RE::ExtraHorse* to);

        void CopyHotkey(const RE::ExtraHotkey* from, RE::ExtraHotkey* to);

        void CopyTextDisplayData(const RE::ExtraTextDisplayData* from, RE::ExtraTextDisplayData* to);

        void CopySoul(const RE::ExtraSoul* from, RE::ExtraSoul* to);

        void CopyOwnership(const RE::ExtraOwnership* from, RE::ExtraOwnership* to);
    };

    template <typename T>
    void CopyExtraData(T* from, T* to){
        if (!from || !to) return;
        switch (T::EXTRADATATYPE) {
            case RE::ExtraDataType::kEnchantment:
                CopyEnchantment(from, to);
                break;
            case RE::ExtraDataType::kHealth:
                CopyHealth(from, to);
                break;
            case RE::ExtraDataType::kRank:
                CopyRank(from, to);
                break;
            case RE::ExtraDataType::kTimeLeft:
                CopyTimeLeft(from, to);
                break;
            case RE::ExtraDataType::kCharge:
                CopyCharge(from, to);
                break;
            case RE::ExtraDataType::kScale:
                CopyScale(from, to);
                break;
            case RE::ExtraDataType::kUniqueID:
                CopyUniqueID(from, to);
                break;
            case RE::ExtraDataType::kPoison:
                CopyPoison(from, to);
                break;
            case RE::ExtraDataType::kObjectHealth:
                CopyObjectHealth(from, to);
                break;
            case RE::ExtraDataType::kLight:
                CopyLight(from, to);
                break;
            case RE::ExtraDataType::kRadius:
                CopyRadius(from, to);
                break;
            case RE::ExtraDataType::kHorse:
                CopyHorse(from, to);
				break;
            case RE::ExtraDataType::kHotkey:
                CopyHotkey(from, to);
				break;
            case RE::ExtraDataType::kTextDisplayData:
				CopyTextDisplayData(from, to);
				break;
            case RE::ExtraDataType::kSoul:
				CopySoul(from, to);
                break;
            case RE::ExtraDataType::kOwnership:
                CopyOwnership(from, to);
                break;
            default:
                logger::warn("ExtraData type not found");
                break;
        }
    }

    [[nodiscard]] bool UpdateExtras(RE::ExtraDataList* copy_from, RE::ExtraDataList* copy_to);
};

namespace WorldObject {
    int16_t GetObjectCount(RE::TESObjectREFR* ref);
    void SetObjectCount(RE::TESObjectREFR* ref, Count count);

    RE::TESObjectREFR* DropObjectIntoTheWorld(RE::TESBoundObject* obj, Count count, bool player_owned=true);

    void SwapObjects(RE::TESObjectREFR* a_from, RE::TESBoundObject* a_to, bool apply_havok=true);

	float GetDistanceFromPlayer(const RE::TESObjectREFR* ref);
    [[nodiscard]] bool PlayerPickUpObject(RE::TESObjectREFR* item, Count count, unsigned int max_try = 3);

    RefID TryToGetRefIDFromHandle(const RE::ObjectRefHandle& handle);

	RE::TESObjectREFR* TryToGetRefFromHandle(RE::ObjectRefHandle& handle, unsigned int max_try = 1);

	RE::TESObjectREFR* TryToGetRefInCell(FormID baseid, Count count, float radius = 180);

	bool IsPlacedObject(RE::TESObjectREFR* ref);

    template <typename T>
    void ForEachRefInCell(T func) {
        const auto player_cell = RE::PlayerCharacter::GetSingleton()->GetParentCell();
        if (!player_cell) {
			logger::error("Player cell is null.");
			return;
		}
        auto& runtimeData = player_cell->GetRuntimeData();
        RE::BSSpinLockGuard locker(runtimeData.spinLock);
        for (auto& ref : runtimeData.references) {
			if (!ref) continue;
			func(ref.get());
		}
    }

    RE::bhkRigidBody* GetRigidBody(const RE::TESObjectREFR* refr);

    RE::NiPoint3 GetPosition(const RE::TESObjectREFR* obj);

    bool IsNextTo(const RE::TESObjectREFR* a_obj, const RE::TESObjectREFR* a_target,float range);

	std::array<RE::NiPoint3, 8> GetBoundingBox(const RE::TESObjectREFR* a_obj);
	void DrawBoundingBox(const RE::TESObjectREFR* a_obj);
	void DrawBoundingBox(const std::array<RE::NiPoint3, 8>& a_box);
	RE::NiPoint3 GetClosestPoint(const RE::NiPoint3& a_point_from , const RE::TESObjectREFR* a_obj_to);

    bool AreClose(const RE::TESObjectREFR* a_obj1, const RE::TESObjectREFR* a_obj2, float threshold);

};

namespace Inventory {
    bool EntryHasXData(const RE::InventoryEntryData* entry);

    bool HasItemEntry(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner,
                      bool nonzero_entry_check = false);

    inline std::int32_t GetItemCount(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner);

    bool IsQuestItem(FormID formid, RE::TESObjectREFR* inv_owner);

    /*template <typename T>
    void UpdateItemList() {
        if (const auto ui = RE::UI::GetSingleton(); ui->IsMenuOpen(T::MENU_NAME)) {
            if (auto inventory_menu = ui->GetMenu<T>()) {
                if (auto itemlist = inventory_menu->GetRuntimeData().itemList) {
                    logger::trace("Updating itemlist.");
                    itemlist->Update();
                } else logger::info("Itemlist is null.");
            } else logger::info("Inventory menu is null.");
        } else logger::info("Inventory menu is not open.");
    }*/

};

namespace Menu {

    RE::TESObjectREFR* GetContainerFromMenu();

    RE::TESObjectREFR* GetVendorChestFromMenu();

    template <typename T>
    void UpdateItemList() {
        if (const auto ui = RE::UI::GetSingleton(); ui->IsMenuOpen(T::MENU_NAME)) {
            if (auto inventory_menu = ui->GetMenu<T>()) {
                if (auto itemlist = inventory_menu->GetRuntimeData().itemList) {
                    //logger::trace("Updating itemlist.");
                    itemlist->Update();
                } else logger::info("Itemlist is null.");
            } else logger::info("Inventory menu is null.");
        } else logger::info("Inventory menu is not open.");
    }
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
        // Default implementation, set the weight if T has a member variable 'weight'
        return;
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