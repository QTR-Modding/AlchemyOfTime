#include "Utils.h"
#include "CLibUtilsQTR/FormReader.hpp"
#include "Settings.h"
#include "CLibUtilsQTR/DrawDebug.hpp"
#include "MCP.h"

std::string Utils::DecodeTypeCode(const std::uint32_t typeCode) {
    char buf[4];
    buf[3] = static_cast<char>(typeCode);
    buf[2] = static_cast<char>(typeCode >> 8);
    buf[1] = static_cast<char>(typeCode >> 16);
    buf[0] = static_cast<char>(typeCode >> 24);
    return std::string(buf, buf + 4);
}

bool Utils::FileIsEmpty(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false; // File could not be opened, treat as not empty or handle error
    }

    char ch;
    while (file.get(ch)) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            return false; // Found a non-whitespace character
        }
    }

    return true; // Only whitespace characters or file is empty
}

void Utils::hexToRGBA(const uint32_t color_code, RE::NiColorA& nicolora) {
    if (color_code > 0xFFFFFF) {
        // 8-digit hex (RRGGBBAA)
        nicolora.red = static_cast<float>(color_code >> 24 & 0xFF); // Bits 24-31
        nicolora.green = static_cast<float>(color_code >> 16 & 0xFF); // Bits 16-23
        nicolora.blue = static_cast<float>(color_code >> 8 & 0xFF); // Bits 8-15
        const uint8_t alphaInt = color_code & 0xFF; // Bits 0-7
        nicolora.alpha = static_cast<float>(alphaInt) / 255.0f;
    } else {
        // 6-digit hex (RRGGBB)
        nicolora.red = static_cast<float>(color_code >> 16 & 0xFF); // Bits 16-23
        nicolora.green = static_cast<float>(color_code >> 8 & 0xFF); // Bits 8-15
        nicolora.blue = static_cast<float>(color_code & 0xFF); // Bits 0-7
        nicolora.alpha = 1.0f; // Default to fully opaque
    }
    nicolora.red /= 255.0f;
    nicolora.green /= 255.0f;
    nicolora.blue /= 255.0f;
}

bool Utils::IsFoodItem(const RE::TESForm* form) {
    if (form->Is(RE::AlchemyItem::FORMTYPE)) {
        const RE::AlchemyItem* form_as_ = form->As<RE::AlchemyItem>();
        if (!form_as_) return false;
        if (!form_as_->IsFood()) return false;
    } else if (form->Is(RE::IngredientItem::FORMTYPE)) {
        const RE::IngredientItem* form_as_ = form->As<RE::IngredientItem>();
        if (!form_as_) return false;
        if (!form_as_->IsFood()) return false;
    } else return false;
    return true;
}

bool Utils::IsPoisonItem(const RE::TESForm* form) {
    if (form->Is(RE::AlchemyItem::FORMTYPE)) {
        const RE::AlchemyItem* form_as_ = form->As<RE::AlchemyItem>();
        if (!form_as_) return false;
        if (!form_as_->IsPoison()) return false;
    } else return false;
    return true;
}

bool Utils::IsMedicineItem(const RE::TESForm* form) {
    if (form->Is(RE::AlchemyItem::FORMTYPE)) {
        const RE::AlchemyItem* form_as_ = form->As<RE::AlchemyItem>();
        if (!form_as_) return false;
        if (!form_as_->IsMedicine()) return false;
    } else return false;
    return true;
}

void Utils::OverrideMGEFFs(RE::BSTArray<RE::Effect*>& effect_array, const std::vector<FormID>& new_effects,
                    const std::vector<uint32_t>& durations, const std::vector<float>& magnitudes) {
    size_t some_index = 0;
    for (auto* effect : effect_array) {
        if (auto* other_eff = FormReader::GetFormByID<RE::EffectSetting>(new_effects[some_index]); !other_eff) {
            effect->effectItem.duration = 0;
            effect->effectItem.magnitude = 0;
        } else {
            effect->baseEffect = other_eff;
            effect->effectItem.duration = durations[some_index];
            effect->effectItem.magnitude = magnitudes[some_index];
        }
        some_index++;
    }
}

void Utils::FavoriteItem(const RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
    if (!item) return;
    if (!inventory_owner) return;
    const auto inventory_changes = inventory_owner->GetInventoryChanges();
    const auto entries = inventory_changes->entryList;
    for (auto it = entries->begin(); it != entries->end(); ++it) {
        if (!*it) {
            logger::error("Item entry is null");
            continue;
        }
        const auto object = (*it)->object;
        if (!object) {
            logger::error("Object is null");
            continue;
        }
        const auto formid = object->GetFormID();
        if (!formid) logger::critical("FormID is null");
        if (formid == item->GetFormID()) {
            const auto xLists = (*it)->extraLists;
            bool no_extra_ = false;
            if (!xLists || xLists->empty()) {
                no_extra_ = true;
            }
            if (no_extra_) {
                //inventory_changes->SetFavorite((*it), nullptr);
            } else if (xLists->front()) {
                inventory_changes->SetFavorite(*it, xLists->front());
            }
            return;
        }
    }
    logger::error("Item not found in inventory");
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
bool Utils::IsFavorited(RE::TESBoundObject* item, const InvMap& inventory) {
    if (const auto it = inventory.find(item); it != inventory.end()) {
        if (it->second.first <= 0) logger::warn("Item count is 0");
        return it->second.second->IsFavorited();
    }
    return false;
}

void Utils::EquipItem(const RE::TESBoundObject* item, const bool unequip) {
    logger::trace("EquipItem");

    if (!item) {
        logger::error("Item is null");
        return;
    }
    const auto player_ref = RE::PlayerCharacter::GetSingleton();
    const auto inventory_changes = player_ref->GetInventoryChanges();
    const auto entries = inventory_changes->entryList;
    for (auto it = entries->begin(); it != entries->end(); ++it) {
        if (const auto formid = (*it)->object->GetFormID(); formid == item->GetFormID()) {
            if (!*it || !(*it)->extraLists) {
                logger::error("Item extraLists is null");
                return;
            }
            if (unequip) {
                if ((*it)->extraLists->empty()) {
                    RE::ActorEquipManager::GetSingleton()->UnequipObject(
                        player_ref, (*it)->object, nullptr, 1,
                        nullptr, true, false, false);
                } else if ((*it)->extraLists->front()) {
                    RE::ActorEquipManager::GetSingleton()->UnequipObject(
                        player_ref, (*it)->object, (*it)->extraLists->front(), 1,
                        nullptr, true, false, false);
                }
            } else {
                if ((*it)->extraLists->empty()) {
                    RE::ActorEquipManager::GetSingleton()->EquipObject(
                        player_ref, (*it)->object, nullptr, 1,
                        nullptr, true, false, false, false);
                } else if ((*it)->extraLists->front()) {
                    RE::ActorEquipManager::GetSingleton()->EquipObject(
                        player_ref, (*it)->object, (*it)->extraLists->front(), 1,
                        nullptr, true, false, false, false);
                }
            }
            return;
        }
    }
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
bool Utils::IsEquipped(RE::TESBoundObject* item, const InvMap& inventory) {
    if (const auto it = inventory.find(item); it != inventory.end()) {
        if (it->second.first <= 0) logger::warn("Item count is 0");
        return it->second.second->IsWorn();
    }
    return false;
}

bool Utils::AreAdjacentCells(RE::TESObjectCELL* cellA, RE::TESObjectCELL* cellB) {
    const auto checkCoordinatesA(cellA->GetCoordinates());
    if (!checkCoordinatesA) {
        logger::error("Coordinates of cellA is null.");
        return false;
    }
    const auto checkCoordinatesB(cellB->GetCoordinates());
    if (!checkCoordinatesB) {
        logger::error("Coordinates of cellB is null.");
        return false;
    }
    const std::int32_t dx(abs(checkCoordinatesA->cellX - checkCoordinatesB->cellX));
    const std::int32_t dy(abs(checkCoordinatesA->cellY - checkCoordinatesB->cellY));
    if (dx <= 1 && dy <= 1) return true;
    return false;
}

std::string Utils::String::EncodeEscapesToAscii(const std::wstring& ws) {
    std::string out;
    for (const auto& wc : ws) {
        switch (wc) {
            case L'\n':
                out += "\\n";
                break;
            case L'\r':
                out += "\\r";
                break;
            case L'\t':
                out += "\\t";
                break;
            case L'\\':
                out += "\\\\";
                break;
            default:
                if (wc < 0x20 || wc > 0x7E) {
                    char buffer[7];
                    snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned int>(wc));
                    out += buffer;
                } else {
                    out += static_cast<char>(wc);
                }
                break;
        }
    }
    return out;
}

std::wstring Utils::String::DecodeEscapesFromAscii(const char* s) {
    auto hexVal = [](const char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
        return -1;
    };

    std::wstring out;
    for (size_t i = 0; s && s[i];) {
        const char c = s[i++];
        if (c == '\\' && s[i]) {
            const char t = s[i++];
            switch (t) {
                case 'n':
                    out.push_back(L'\n');
                    break;
                case 'r':
                    out.push_back(L'\r');
                    break;
                case 't':
                    out.push_back(L'\t');
                    break;
                case '\\':
                    out.push_back(L'\\');
                    break;
                case 'x': {
                    int val = 0, digits = 0;
                    while (s[i]) {
                        const int hv = hexVal(s[i]);
                        if (hv < 0) break;
                        val = val << 4 | hv;
                        ++i;
                        ++digits;
                        if (digits >= 2) break;
                    }
                    if (digits > 0)
                        out.push_back(static_cast<wchar_t>(val));
                    else
                        out.push_back(L'x');
                    break;
                }
                case 'u': {
                    int val = 0, digits = 0;
                    while (s[i] && digits < 4) {
                        const int hv = hexVal(s[i]);
                        if (hv < 0) break;
                        val = val << 4 | hv;
                        ++i;
                        ++digits;
                    }
                    if (digits == 4)
                        out.push_back(static_cast<wchar_t>(val));
                    else
                        out.push_back(L'u');
                    break;
                }
                default:
                    out.push_back(static_cast<unsigned char>(t));
                    break;
            }
        } else {
            out.push_back(static_cast<unsigned char>(c));
        }
    }
    return out;
}

bool Utils::Types::FormEditorID::operator<(const FormEditorID& other) const {
    // Compare form_id first
    if (form_id < other.form_id) {
        return true;
    }
    // If form_id is equal, compare editor_id
    if (form_id == other.form_id && editor_id < other.editor_id) {
        return true;
    }
    // If both form_id and editor_id are equal or if form_id is greater, return false
    return false;
}

bool Utils::Types::FormEditorIDX::operator==(const FormEditorIDX& other) const {
    return form_id == other.form_id;
}

RE::TESObjectREFR* Utils::WorldObject::DropObjectIntoTheWorld(RE::TESBoundObject* obj, const Count count,
                                                              const bool player_owned) {
    const auto player_ch = RE::PlayerCharacter::GetSingleton();

    constexpr auto multiplier = 100.0f;
    constexpr float q_pi = std::numbers::pi_v<float>;
    auto orji_vec = RE::NiPoint3{multiplier, 0.f, player_ch->GetHeight()};
    Math::rotateZ(orji_vec, q_pi / 4.f - player_ch->GetAngleZ());
    const auto drop_pos = player_ch->GetPosition() + orji_vec;
    const auto player_cell = player_ch->GetParentCell();
    const auto player_ws = player_ch->GetWorldspace();
    if (!player_cell && !player_ws) {
        logger::critical("Player cell AND player world is null.");
        return nullptr;
    }
    const auto newPropRef =
        RE::TESDataHandler::GetSingleton()
        ->CreateReferenceAtLocation(obj, drop_pos, {0.0f, 0.0f, 0.0f}, player_cell,
                                    player_ws, nullptr, nullptr, {}, false, false)
        .get()
        .get();
    if (!newPropRef) {
        logger::critical("New prop ref is null.");
        return nullptr;
    }
    if (player_owned) newPropRef->extraList.SetOwner(RE::TESForm::LookupByID(0x07));
    if (count > 1) newPropRef->extraList.SetCount(static_cast<uint16_t>(count));
    return newPropRef;
}

void Utils::WorldObject::SwapObjects(RE::TESObjectREFR* a_from, RE::TESBoundObject* a_to, const bool apply_havok) {
    logger::trace("SwapObjects");
    if (!a_from) {
        logger::error("Ref is null.");
        return;
    }
    const auto ref_base = a_from->GetBaseObject();
    if (!ref_base) {
        logger::error("Ref base is null.");
        return;
    }
    if (!a_to) {
        logger::error("Base is null.");
        return;
    }
    if (ref_base->GetFormID() == a_to->GetFormID()) {
        return;
    }
    a_from->SetObjectReference(a_to);
    if (!apply_havok) return;
    auto a_handle = a_from->GetHandle();
    SKSE::GetTaskInterface()->AddTask([a_handle]() {
        if (const auto refr = a_handle.get().get()) {
            refr->Disable();
            refr->Enable(false);
        }
    });
}

bool Utils::WorldObject::IsPlacedObject(RE::TESObjectREFR* ref) {
    if (ref->extraList.HasType(RE::ExtraDataType::kStartingPosition)) {
        if (const auto starting_pos = ref->extraList.GetByType<RE::ExtraStartingPosition>(); starting_pos->location) {
            /*logger::trace("has location.");
            logger::trace("Location: {}", starting_pos->location->GetName());
            logger::trace("Location: {}", starting_pos->location->GetFullName());*/
            return true;
        }
        /*logger::trace("Position: {}", starting_pos->startPosition.pos.x);
        logger::trace("Position: {}", starting_pos->startPosition.pos.y);
        logger::trace("Position: {}", starting_pos->startPosition.pos.z);*/
    }
    return false;
}

RE::bhkRigidBody* Utils::WorldObject::GetRigidBody(const RE::TESObjectREFR* refr) {
    const auto object3D = refr->GetCurrent3D();
    if (!object3D) {
        return nullptr;
    }
    if (const auto body = object3D->GetCollisionObject()) {
        return body->GetRigidBody();
    }
    return nullptr;
}

RE::NiPoint3 Utils::WorldObject::GetPosition(const RE::TESObjectREFR* obj) {
    // Prefer accurate mesh world translate when available
    if (const auto mesh = obj->GetCurrent3D()) {
        return mesh->world.translate;
    }
    // Fallback to Havok/body or ref pos
    const auto body = GetRigidBody(obj);
    if (!body) return obj->GetPosition();
    RE::hkVector4 havockPosition;
    body->GetPosition(havockPosition);
    float components[4];
    _mm_store_ps(components, havockPosition.quad);
    RE::NiPoint3 newPosition = {components[0], components[1], components[2]};
    constexpr float havockToSkyrimConversionRate = 69.9915f;
    newPosition *= havockToSkyrimConversionRate;
    return newPosition;
}

void Utils::Math::rotateZ(RE::NiPoint3& v, const float angle) {
    const float x = v.x * cos(angle) - v.y * sin(angle);
    const float y = v.x * sin(angle) + v.y * cos(angle);
    v.x = x;
    v.y = y;
}

bool Utils::Inventory::IsQuestItem(const FormID formid, const InvMap& a_inv) {
    if (const auto item = FormReader::GetFormByID<RE::TESBoundObject>(formid)) {
        if (const auto it = a_inv.find(item); it != a_inv.end()) {
            if (it->second.second->IsQuestObject()) return true;
        }
    }
    return false;
}

RE::TESObjectREFR* Utils::Menu::GetContainerFromMenu() {
    const auto ui = RE::UI::GetSingleton()->GetMenu<RE::ContainerMenu>();
    if (!ui) {
        logger::warn("GetContainerFromMenu: Container menu is null");
        return nullptr;
    }
    auto ui_refid = ui->GetTargetRefHandle();
    if (!ui_refid) {
        logger::warn("GetContainerFromMenu: Container menu reference id is null");
        return nullptr;
    }
    logger::trace("UI Reference id {}", ui_refid);
    if (const auto ui_ref = RE::TESObjectREFR::LookupByHandle(ui_refid)) {
        return ui_ref.get();
    }
    return nullptr;
}

RE::TESObjectREFR* Utils::Menu::GetVendorChestFromMenu() {
    const auto ui = RE::UI::GetSingleton()->GetMenu<RE::BarterMenu>();
    if (!ui) {
        logger::warn("GetVendorChestFromMenu: Barter menu is null");
        return nullptr;
    }
    const auto ui_ref = RE::TESObjectREFR::LookupByHandle(ui->GetTargetRefHandle());
    if (!ui_ref) {
        logger::warn("GetVendorChestFromMenu: Barter menu reference is null");
        return nullptr;
    }
    if (ui_ref->IsPlayerRef()) return nullptr;

    if (const auto barter_actor = ui_ref->GetBaseObject()->As<RE::Actor>()) {
        if (const auto* faction = barter_actor->GetVendorFaction()) {
            if (auto* merchant_chest = faction->vendorData.merchantContainer) {
                return merchant_chest;
            }
        }
    }

    if (const auto barter_npc = ui_ref->GetBaseObject()->As<RE::TESNPC>()) {
        for (const auto& faction_rank : barter_npc->factions) {
            if (const auto merchant_chest = faction_rank.faction->vendorData.merchantContainer) {
                return merchant_chest;
            }
        }
    }

    //auto chest = RE::TESObjectREFR::LookupByHandle(ui->GetTargetRefHandle());

    /*for (size_t i = 0; i < 192; i++) {
        if (ui_ref->extraList.HasType(static_cast<RE::ExtraDataType>(i))) {
            logger::trace("ExtraData type: {:x}", i);
        }
    }*/

    return nullptr;
}

void Utils::Menu::UpdateItemList() {
    if (const auto ui = RE::UI::GetSingleton()) {
        if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
            const auto inventory_menu = ui->GetMenu<RE::InventoryMenu>();
            if (const auto itemlist = inventory_menu->GetRuntimeData().itemList) {
                itemlist->Update();
            } else logger::error("Itemlist is null.");
        } else if (ui->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
            const auto barter_menu = ui->GetMenu<RE::BarterMenu>();
            if (const auto itemlist = barter_menu->GetRuntimeData().itemList) {
                itemlist->Update();
            } else logger::error("Itemlist is null.");
        } else if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
            const auto container_menu = ui->GetMenu<RE::ContainerMenu>();
            if (const auto itemlist = container_menu->GetRuntimeData().itemList) {
                itemlist->Update();
            } else logger::error("Itemlist is null.");
        }
    }
}

RE::StandardItemData* Utils::Menu::GetSelectedItemDataInMenu(std::string& a_menuOut) {
    if (const auto ui = RE::UI::GetSingleton()) {
        if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
            a_menuOut = RE::InventoryMenu::MENU_NAME;
            return GetSelectedItemData<RE::InventoryMenu>();
        }
        if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
            a_menuOut = RE::ContainerMenu::MENU_NAME;
            return GetSelectedItemData<RE::ContainerMenu>();
        }
        if (ui->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
            a_menuOut = RE::BarterMenu::MENU_NAME;
            return Utils::Menu::GetSelectedItemData<RE::BarterMenu>();
        }
    }
    return nullptr;
}


RE::StandardItemData* Utils::Menu::GetSelectedItemDataInMenu() {
    std::string menu_name;
    return GetSelectedItemDataInMenu(menu_name);
}

RE::TESObjectREFR* Utils::Menu::GetOwnerOfItem(const RE::StandardItemData* a_itemdata) {
    auto& refHandle = a_itemdata->owner;
    if (const auto owner = RE::TESObjectREFR::LookupByHandle(refHandle)) {
        return owner.get();
    }
    if (const auto owner_actor = RE::Actor::LookupByHandle(refHandle)) {
        return owner_actor->AsReference();
    }
    return nullptr;
}