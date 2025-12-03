#include "Utils.h"
#include "BoundingBox.hpp"
#include "CLibUtilsQTR/FormReader.hpp"
#include "Settings.h"
#include "DrawDebug.hpp"
#include "MCP.h"

bool Types::FormEditorID::operator<(const FormEditorID& other) const {
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

bool Types::FormEditorIDX::operator==(const FormEditorIDX& other) const {
    return form_id == other.form_id;
}

std::string DecodeTypeCode(const std::uint32_t typeCode) {
    char buf[4];
    buf[3] = static_cast<char>(typeCode);
    buf[2] = static_cast<char>(typeCode >> 8);
    buf[1] = static_cast<char>(typeCode >> 16);
    buf[0] = static_cast<char>(typeCode >> 24);
    return std::string(buf, buf + 4);
}

bool FileIsEmpty(const std::string& filename) {
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

void hexToRGBA(const uint32_t color_code, RE::NiColorA& nicolora) {
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

bool IsFoodItem(const RE::TESForm* form) {
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

bool IsPoisonItem(const RE::TESForm* form) {
    if (form->Is(RE::AlchemyItem::FORMTYPE)) {
        const RE::AlchemyItem* form_as_ = form->As<RE::AlchemyItem>();
        if (!form_as_) return false;
        if (!form_as_->IsPoison()) return false;
    } else return false;
    return true;
}

bool IsMedicineItem(const RE::TESForm* form) {
    if (form->Is(RE::AlchemyItem::FORMTYPE)) {
        const RE::AlchemyItem* form_as_ = form->As<RE::AlchemyItem>();
        if (!form_as_) return false;
        if (!form_as_->IsMedicine()) return false;
    } else return false;
    return true;
}

void OverrideMGEFFs(RE::BSTArray<RE::Effect*>& effect_array, const std::vector<FormID>& new_effects,
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

void FavoriteItem(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
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
        if (!formid) logger::critical("Formid is null");
        if (formid == item->GetFormID()) {
            logger::trace("Favoriting item: {}", item->GetName());
            const auto xLists = (*it)->extraLists;
            bool no_extra_ = false;
            if (!xLists || xLists->empty()) {
                logger::trace("No extraLists");
                no_extra_ = true;
            }
            if (no_extra_) {
                logger::trace("No extraLists");
                //inventory_changes->SetFavorite((*it), nullptr);
            } else if (xLists->front()) {
                logger::trace("ExtraLists found");
                inventory_changes->SetFavorite(*it, xLists->front());
            }
            return;
        }
    }
    logger::error("Item not found in inventory");
}

void FavoriteItem(const FormID formid, const FormID refid) {
    FavoriteItem(FormReader::GetFormByID<RE::TESBoundObject>(formid),
                 FormReader::GetFormByID<RE::TESObjectREFR>(refid));
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
bool IsFavorited(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
    if (!item) {
        logger::warn("Item is null");
        return false;
    }
    if (!inventory_owner) {
        logger::warn("Inventory owner is null");
        return false;
    }
    auto inventory = inventory_owner->GetInventory();
    if (const auto it = inventory.find(item); it != inventory.end()) {
        if (it->second.first <= 0) logger::warn("Item count is 0");
        return it->second.second->IsFavorited();
    }
    return false;
}

bool IsFavorited(const RE::FormID formid, const RE::FormID refid) {
    return IsFavorited(FormReader::GetFormByID<RE::TESBoundObject>(formid),
                       FormReader::GetFormByID<RE::TESObjectREFR>(refid));
}

void EquipItem(const RE::TESBoundObject* item, const bool unequip) {
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

void EquipItem(const FormID formid, const bool unequip) {
    EquipItem(FormReader::GetFormByID<RE::TESBoundObject>(formid), unequip);
}

bool IsEquipped(RE::TESBoundObject* item) {
    if (!item) {
        logger::trace("Item is null");
        return false;
    }

    const auto player_ref = RE::PlayerCharacter::GetSingleton();
    auto inventory = player_ref->GetInventory();
    if (const auto it = inventory.find(item); it != inventory.end()) {
        if (it->second.first <= 0) logger::warn("Item count is 0");
        return it->second.second->IsWorn();
    }
    return false;
}

bool IsEquipped(const FormID formid) {
    return IsEquipped(FormReader::GetFormByID<RE::TESBoundObject>(formid));
}

bool AreAdjacentCells(RE::TESObjectCELL* cellA, RE::TESObjectCELL* cellB) {
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

std::string String::EncodeEscapesToAscii(const std::wstring& ws) {
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

std::wstring String::DecodeEscapesFromAscii(const char* s) {
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

int16_t WorldObject::GetObjectCount(RE::TESObjectREFR* ref) {
    if (!ref) {
        logger::error("Ref is null.");
        return 0;
    }
    if (ref->extraList.HasType(RE::ExtraDataType::kCount)) {
        const RE::ExtraCount* xCount = ref->extraList.GetByType<RE::ExtraCount>();
        return xCount->count;
    }
    return 0;
}

void WorldObject::SetObjectCount(RE::TESObjectREFR* ref, const Count count) {
    if (!ref) {
        logger::error("Ref is null.");
        return;
    }
    int max_try = 1;
    while (ref->extraList.HasType(RE::ExtraDataType::kCount) && max_try) {
        ref->extraList.RemoveByType(RE::ExtraDataType::kCount);
        max_try--;
    }
    // ref->extraList.SetCount(static_cast<uint16_t>(count));
    const auto xCount = new RE::ExtraCount(static_cast<int16_t>(count));
    ref->extraList.Add(xCount);
}

RE::TESObjectREFR* WorldObject::DropObjectIntoTheWorld(RE::TESBoundObject* obj, const Count count,
                                                       const bool player_owned) {
    const auto player_ch = RE::PlayerCharacter::GetSingleton();

    constexpr auto multiplier = 100.0f;
    constexpr float q_pi = std::numbers::pi_v<float>;
    auto orji_vec = RE::NiPoint3{multiplier, 0.f, player_ch->GetHeight()};
    Math::LinAlg::R3::rotateZ(orji_vec, q_pi / 4.f - player_ch->GetAngleZ());
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

void WorldObject::SwapObjects(RE::TESObjectREFR* a_from, RE::TESBoundObject* a_to, const bool apply_havok) {
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
        logger::trace("Ref and base are the same.");
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

RE::TESObjectREFR* WorldObject::TryToGetRefInCell(const FormID baseid, const Count count, const float radius) {
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto player_cell = player->GetParentCell();
    if (!player_cell) {
        logger::error("Player cell is null.");
        return nullptr;
    }
    const auto player_pos = player->GetPosition();
    auto& runtimeData = player_cell->GetRuntimeData();
    RE::BSSpinLockGuard locker(runtimeData.spinLock);
    for (const auto& ref : runtimeData.references) {
        if (!ref) continue;
        const auto ref_base = ref->GetBaseObject();
        if (!ref_base) continue;
        const auto ref_baseid = ref_base->GetFormID();
        const auto ref_id = ref->GetFormID();
        const auto ref_pos = ref->GetPosition();
        if (ref_baseid == baseid && ref->extraList.GetCount() == count) {
            // get radius and check if ref is in radius
            if (ref_id < 4278190080) {
                logger::trace("Ref is a placed reference. Continuing search.");
                continue;
            }
            logger::trace("Ref found in cell: {} with id {}", ref_base->GetName(), ref_id);
            if (radius > 0.f) {
                if (player_pos.GetDistance(ref_pos) < radius) return ref.get();
                logger::trace("Ref is not in radius");
            } else return ref.get();
        }
    }
    return nullptr;
}

bool WorldObject::IsPlacedObject(RE::TESObjectREFR* ref) {
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

RE::bhkRigidBody* WorldObject::GetRigidBody(const RE::TESObjectREFR* refr) {
    const auto object3D = refr->GetCurrent3D();
    if (!object3D) {
        return nullptr;
    }
    if (const auto body = object3D->GetCollisionObject()) {
        return body->GetRigidBody();
    }
    return nullptr;
}

RE::NiPoint3 WorldObject::GetPosition(const RE::TESObjectREFR* obj) {
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

bool WorldObject::AreClose(const RE::TESObjectREFR* a_obj1, const RE::TESObjectREFR* a_obj2, const float threshold) {
    const auto c1 = GetPosition(a_obj1);
    const auto closestOn2 = BoundingBox::ClosestPoint(c1, a_obj2);
    const auto closestOn1 = BoundingBox::ClosestPoint(closestOn2, a_obj1);
    if (closestOn2.GetDistance(closestOn1) < threshold) {
        #ifndef NDEBUG
        if (UI::draw_debug) {
            draw_line(closestOn2, closestOn1, 3, glm::vec4(1.f, 1.f, 1.f, 1.f));
        }
        #endif
        return true;
    }
    return false;
}

void Math::LinAlg::R3::rotateX(RE::NiPoint3& v, const float angle) {
    const float y = v.y * cos(angle) - v.z * sin(angle);
    const float z = v.y * sin(angle) + v.z * cos(angle);
    v.y = y;
    v.z = z;
}

void Math::LinAlg::R3::rotateY(RE::NiPoint3& v, const float angle) {
    const float x = v.x * cos(angle) + v.z * sin(angle);
    const float z = -v.x * sin(angle) + v.z * cos(angle);
    v.x = x;
    v.z = z;
}

void Math::LinAlg::R3::rotateZ(RE::NiPoint3& v, const float angle) {
    const float x = v.x * cos(angle) - v.y * sin(angle);
    const float y = v.x * sin(angle) + v.y * cos(angle);
    v.x = x;
    v.y = y;
}

void Math::LinAlg::R3::rotate(RE::NiPoint3& v, const float angleX, const float angleY, const float angleZ) {
    rotateX(v, angleX);
    rotateY(v, angleY);
    rotateZ(v, angleZ);
}

bool Inventory::IsQuestItem(const FormID formid, RE::TESObjectREFR* inv_owner) {
    if (const auto item = FormReader::GetFormByID<RE::TESBoundObject>(formid)) {
        const auto inventory = inv_owner->GetInventory();
        if (const auto it = inventory.find(item); it != inventory.end()) {
            if (it->second.second->IsQuestObject()) return true;
        }
    }
    return false;
}

void DynamicForm::copyBookAppearence(RE::TESForm* source, RE::TESForm* target) {
    const auto* sourceBook = source->As<RE::TESObjectBOOK>();

    auto* targetBook = target->As<RE::TESObjectBOOK>();

    if (sourceBook && targetBook) {
        targetBook->inventoryModel = sourceBook->inventoryModel;
    }
}

void DynamicForm::copyFormArmorModel(RE::TESForm* source, RE::TESForm* target) {
    const auto* sourceModelBipedForm = source->As<RE::TESObjectARMO>();

    auto* targeteModelBipedForm = target->As<RE::TESObjectARMO>();

    if (sourceModelBipedForm && targeteModelBipedForm) {
        logger::info("armor");

        targeteModelBipedForm->armorAddons = sourceModelBipedForm->armorAddons;
    }
}

void DynamicForm::copyFormObjectWeaponModel(RE::TESForm* source, RE::TESForm* target) {
    const auto* sourceModelWeapon = source->As<RE::TESObjectWEAP>();

    auto* targeteModelWeapon = target->As<RE::TESObjectWEAP>();

    if (sourceModelWeapon && targeteModelWeapon) {
        logger::info("weapon");

        targeteModelWeapon->firstPersonModelObject = sourceModelWeapon->firstPersonModelObject;

        targeteModelWeapon->attackSound = sourceModelWeapon->attackSound;

        targeteModelWeapon->attackSound2D = sourceModelWeapon->attackSound2D;

        targeteModelWeapon->attackSound = sourceModelWeapon->attackSound;

        targeteModelWeapon->attackFailSound = sourceModelWeapon->attackFailSound;

        targeteModelWeapon->idleSound = sourceModelWeapon->idleSound;

        targeteModelWeapon->equipSound = sourceModelWeapon->equipSound;

        targeteModelWeapon->unequipSound = sourceModelWeapon->unequipSound;

        targeteModelWeapon->soundLevel = sourceModelWeapon->soundLevel;
    }
}

void DynamicForm::copyMagicEffect(RE::TESForm* source, RE::TESForm* target) {
    const auto* sourceEffect = source->As<RE::EffectSetting>();

    auto* targetEffect = target->As<RE::EffectSetting>();

    if (sourceEffect && targetEffect) {
        targetEffect->effectSounds = sourceEffect->effectSounds;

        targetEffect->data.castingArt = sourceEffect->data.castingArt;

        targetEffect->data.light = sourceEffect->data.light;

        targetEffect->data.hitEffectArt = sourceEffect->data.hitEffectArt;

        targetEffect->data.effectShader = sourceEffect->data.effectShader;

        targetEffect->data.hitVisuals = sourceEffect->data.hitVisuals;

        targetEffect->data.enchantShader = sourceEffect->data.enchantShader;

        targetEffect->data.enchantEffectArt = sourceEffect->data.enchantEffectArt;

        targetEffect->data.enchantVisuals = sourceEffect->data.enchantVisuals;

        targetEffect->data.projectileBase = sourceEffect->data.projectileBase;

        targetEffect->data.explosion = sourceEffect->data.explosion;

        targetEffect->data.impactDataSet = sourceEffect->data.impactDataSet;

        targetEffect->data.imageSpaceMod = sourceEffect->data.imageSpaceMod;
    }
}

void DynamicForm::copyAppearence(RE::TESForm* source, RE::TESForm* target) {
    copyFormArmorModel(source, target);

    copyFormObjectWeaponModel(source, target);

    copyMagicEffect(source, target);

    copyBookAppearence(source, target);

    copyComponent<RE::BGSPickupPutdownSounds>(source, target);

    copyComponent<RE::BGSMenuDisplayObject>(source, target);

    copyComponent<RE::TESModel>(source, target);

    copyComponent<RE::TESBipedModelForm>(source, target);
}

RE::TESObjectREFR* Menu::GetContainerFromMenu() {
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

RE::TESObjectREFR* Menu::GetVendorChestFromMenu() {
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

void Menu::UpdateItemList() {
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

RE::StandardItemData* Menu::GetSelectedItemDataInMenu(std::string& a_menuOut) {
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
            return GetSelectedItemData<RE::BarterMenu>();
        }
    }
    return nullptr;
}

RE::StandardItemData* Menu::GetSelectedItemDataInMenu() {
    std::string menu_name;
    return GetSelectedItemDataInMenu(menu_name);
}

RE::TESObjectREFR* Menu::GetOwnerOfItem(const RE::StandardItemData* a_itemdata) {
    auto& refHandle = a_itemdata->owner;
    if (const auto owner = RE::TESObjectREFR::LookupByHandle(refHandle)) {
        return owner.get();
    }
    if (const auto owner_actor = RE::Actor::LookupByHandle(refHandle)) {
        return owner_actor->AsReference();
    }
    return nullptr;
}


float Math::Round(const float value, const int n) {
    const float factor = std::powf(10.0f, static_cast<float>(n));
    return std::round(value * factor) / factor;
}

float Math::Ceil(const float value, const int n) {
    const float factor = std::powf(10.0f, static_cast<float>(n));
    return std::ceil(value * factor) / factor;
}

std::array<RE::NiPoint3, 3> Math::LinAlg::GetClosest3Vertices(const std::array<RE::NiPoint3, 8>& a_bounding_box,
                                                              const RE::NiPoint3& outside_point) {
    std::array<RE::NiPoint3, 3> result;

    std::vector<std::pair<float, size_t>> distances;
    for (size_t i = 0; i < a_bounding_box.size(); ++i) {
        float dist = outside_point.GetDistance(a_bounding_box[i]);
        distances.emplace_back(dist, i);
    }
    std::ranges::sort(distances);
    for (size_t i = 0; i < 3 && i < distances.size(); ++i) {
        const size_t index = distances[i].second;
        result[i] = a_bounding_box[index];
    }

    return result;
}

std::array<RE::NiPoint3, 3> Math::LinAlg::GetClosest3Vertices(const std::array<RE::NiPoint3, 4>& a_bounded_plane,
                                                              const RE::NiPoint3& outside_point) {
    std::array<RE::NiPoint3, 3> result;

    std::vector<std::pair<float, size_t>> distances;
    for (size_t i = 0; i < a_bounded_plane.size(); ++i) {
        float dist = outside_point.GetDistance(a_bounded_plane[i]);
        distances.emplace_back(dist, i);
    }
    std::ranges::sort(distances);
    for (size_t i = 0; i < 3 && i < distances.size(); ++i) {
        const size_t index = distances[i].second;
        result[i] = a_bounded_plane[index];
    }

    return result;
}

RE::NiPoint3 Math::LinAlg::CalculateNormalOfPlane(const RE::NiPoint3& span1, const RE::NiPoint3& span2) {
    const auto crossed = span1.Cross(span2);
    const auto length = crossed.Length();
    if (fabs(length) < EPSILON) {
        return {0, 0, 0};
    }
    return crossed / length;
}

RE::NiPoint3 Math::LinAlg::closestPointOnPlane(const RE::NiPoint3& a_point_on_plane,
                                               const RE::NiPoint3& a_point_not_on_plane, const RE::NiPoint3& v_normal) {
    const auto distance = (a_point_not_on_plane - a_point_on_plane).Dot(v_normal);
    return a_point_not_on_plane - v_normal * distance;
}

RE::NiPoint3 Math::LinAlg::intersectLine(const std::array<RE::NiPoint3, 3>& vertices,
                                         const RE::NiPoint3& outside_plane_point) {
    const RE::NiPoint3 edge0 = vertices[1] - vertices[0]; // AtoB
    const RE::NiPoint3 edge1 = vertices[2] - vertices[1]; // BtoC
    const RE::NiPoint3 edge2 = vertices[0] - vertices[2]; // CtoA

    const float mags[3] = {edge0.Length(), edge1.Length(), edge2.Length()};

    size_t maxIndex = 0;
    if (mags[1] > mags[maxIndex]) maxIndex = 1;
    if (mags[2] > mags[maxIndex]) maxIndex = 2;

    //[[maybe_unused]] const auto& hypotenuse = edges[index];
    const auto index1 = (maxIndex + 1) % 3;
    const auto index2 = (maxIndex + 2) % 3;

    const auto& orthogonal_vertex = vertices[index2]; // B

    for (const auto a_index : {maxIndex, index1}) {
        const auto& hypotenuse_vertex = vertices[a_index]; // C or A depending on closed loop orientation

        const auto temp = orthogonal_vertex - hypotenuse_vertex;
        const auto temp_length = temp.Length();
        if (temp_length == 0.f) continue;
        const auto temp_unit = temp / temp_length;

        const auto& other_hypotenuse_vertex = a_index == index1 ? vertices[maxIndex] : vertices[index1];
        const auto temp2 = other_hypotenuse_vertex - orthogonal_vertex;
        const auto temp2_length = temp2.Length();
        if (temp2_length == 0.f) continue;
        const auto temp2_unit = temp2 / temp2_length;

        const auto theta_max = atan(temp2_length / temp_length);
        const auto distance_vector = outside_plane_point - hypotenuse_vertex;
        const auto distance_vector_length = distance_vector.Length();
        const auto distance_vector_unit = distance_vector / distance_vector_length;

        if (const auto theta = acos(distance_vector_unit.Dot(temp_unit));
            0.f <= theta && theta <= theta_max) {
            if (temp2_unit.Dot(distance_vector_unit) > 0) {
                const auto a_span_size = tan(theta) * temp_length;
                if (const auto intersect = temp + temp2_unit * a_span_size;
                    intersect.Length() > distance_vector_length) {
                    return outside_plane_point; // it is inside the triangle
                }
                const auto normal_distance = (outside_plane_point - orthogonal_vertex).Dot(temp2_unit);
                return temp2_unit * normal_distance + orthogonal_vertex;
            }
        }
    }

    return orthogonal_vertex;
}