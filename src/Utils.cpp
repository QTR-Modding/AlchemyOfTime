#include "Utils.h"
#include <numbers>
#include "CLibUtilsQTR/FormReader.hpp"
#include "Settings.h"
#include "DrawDebug.h"
#include "MCP.h"
#pragma comment(lib, "d3d11.lib")


namespace {
    UINT GetBufferLength(RE::ID3D11Buffer* reBuffer) {
        const auto buffer = reinterpret_cast<ID3D11Buffer*>(reBuffer);
        D3D11_BUFFER_DESC bufferDesc = {};
        buffer->GetDesc(&bufferDesc);
        return bufferDesc.ByteWidth;
    }

    void EachGeometry(const RE::TESObjectREFR* obj,
                      const std::function<void(RE::BSGeometry* o3d, RE::BSGraphics::TriShape*)>& callback) {
        if (const auto d3d = obj->Get3D()) {
            RE::BSVisit::TraverseScenegraphGeometries(
                d3d, [&](RE::BSGeometry* a_geometry) -> RE::BSVisit::BSVisitControl {
                    const auto& model = a_geometry->GetGeometryRuntimeData();

                    if (const auto triShape = model.rendererData) {
                        callback(a_geometry, triShape);
                    }

                    return RE::BSVisit::BSVisitControl::kContinue;
                });
        }
    }

    float clampf(const float x, const float lo, const float hi) {
        return x < lo ? lo : x > hi ? hi : x;
    }
};


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

void EquipItem(RE::TESBoundObject* item, const bool unequip) {
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

bool IsEquipped(RE::TESBoundObject* item) {
    logger::trace("IsEquipped");

    if (!item) {
        logger::trace("Item is null");
        return false;
    }

    const auto player_ref = RE::PlayerCharacter::GetSingleton();
    auto inventory = player_ref->GetInventory();
    const auto it = inventory.find(item);
    if (it != inventory.end()) {
        if (it->second.first <= 0) logger::warn("Item count is 0");
        return it->second.second->IsWorn();
    }
    return false;
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
    SKSE::GetTaskInterface()->AddTask([a_from]() {
        //a_from->Release3DRelatedData();
        a_from->Disable();
        a_from->Enable(false);
    });
    /*a_from->Disable();
    a_from->Enable(false);*/

    /*float afX = 100;
    float afY = 100;
    float afZ = 100;
    float afMagnitude = 100;*/
    /*auto args = RE::MakeFunctionArguments(std::move(afX), std::move(afY), std::move(afZ),
    std::move(afMagnitude)); vm->DispatchMethodCall(object, "ApplyHavokImpulse", args, callback);*/
    // Looked up here (wSkeever): https:  // www.nexusmods.com/skyrimspecialedition/mods/73607
    /*SKSE::GetTaskInterface()->AddTask([a_from]() {
        ApplyHavokImpulse(a_from, 0.f, 0.f, 10.f, 5000.f);
    });*/
    //SKSE::GetTaskInterface()->AddTask([a_from]() {
    //    // auto player_ch = RE::PlayerCharacter::GetSingleton();
    //    // player_ch->StartGrabObject();
    //    auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    //    auto policy = vm->GetObjectHandlePolicy();
    //    auto handle = policy->GetHandleForObject(a_from->GetFormType(), a_from);
    //    RE::BSTSmartPointer<RE::BSScript::Object> object;
    //    vm->CreateObject2("ObjectReference", object);
    //    if (!object) logger::warn("Object is null");
    //    vm->BindObject(object, handle, false);
    //    auto args = RE::MakeFunctionArguments(std::move(0.f), std::move(0.f), std::move(1.f), std::move(5.f));
    //    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
    //    if (vm->DispatchMethodCall(object, "ApplyHavokImpulse", args, callback)) logger::trace("FUSRODAH");
    //});
}

float WorldObject::GetDistanceFromPlayer(const RE::TESObjectREFR* ref) {
    if (!ref) {
        logger::error("Ref is null.");
        return 0;
    }
    return RE::PlayerCharacter::GetSingleton()->GetPosition().GetDistance(ref->GetPosition());
}

bool WorldObject::PlayerPickUpObject(RE::TESObjectREFR* item, const Count count, const unsigned int max_try) {
    logger::trace("PickUpItem");

    if (!item) {
        logger::warn("Item is null");
        return false;
    }

    const auto actor = RE::PlayerCharacter::GetSingleton();
    const auto item_bound = item->GetObjectReference();
    const auto item_count = Inventory::GetItemCount(item_bound, actor);
    logger::trace("Item count: {}", item_count);
    unsigned int i = 0;
    if (!item_bound) {
        logger::warn("Item bound is null");
        return false;
    }
    while (i < max_try) {
        logger::trace("Critical: PickUpItem");
        actor->PickUpObject(item, count, false, false);
        logger::trace("Item picked up. Checking if it is in inventory...");
        if (Inventory::GetItemCount(item_bound, actor) > item_count) {
            logger::trace("Item picked up. Took {} extra tries.", i);
            return true;
        }
        logger::trace("item count: {}", Inventory::GetItemCount(item_bound, actor));
        i++;
    }

    return false;
}

RefID WorldObject::TryToGetRefIDFromHandle(const RE::ObjectRefHandle& handle) {
    if (handle.get() && handle.get()->GetFormID()) return handle.get()->GetFormID();
    if (handle.native_handle()
        //&& RE::TESObjectREFR::LookupByID<RE::TESObjectREFR>(handle.native_handle())
    )
        return handle.native_handle();
    return 0;
}

RE::TESObjectREFR* WorldObject::TryToGetRefFromHandle(RE::ObjectRefHandle& handle, unsigned int max_try) {
    RE::TESObjectREFR* ref;
    if (const auto handle_ref = RE::TESObjectREFR::LookupByHandle(handle.native_handle())) {
        logger::trace("Handle ref found");
        ref = handle_ref.get();
        return ref;
    }
    if (handle.get()) {
        ref = handle.get().get();
        return ref;
    }
    if (const auto ref_ = RE::TESForm::LookupByID<RE::TESObjectREFR>(handle.native_handle())) {
        return ref_;
    }
    if (max_try && handle) return TryToGetRefFromHandle(handle, --max_try);
    return nullptr;
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
    const auto object3D = refr->Get3D();
    if (!object3D) {
        return nullptr;
    }
    if (const auto body = object3D->GetCollisionObject()) {
        return body->GetRigidBody();
    }
    return nullptr;
}

RE::NiPoint3 WorldObject::GetPosition(const RE::TESObjectREFR* obj) {
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

bool WorldObject::IsNextTo(const RE::TESObjectREFR* a_obj, const RE::TESObjectREFR* a_target, const float range) {
    //logger::info("a_obj {} bound_max {} bound_min {}", a_obj->GetName(), a_obj->GetBoundMax().Length(), a_obj->GetBoundMin().Length());
    //logger::info("a_target {} bound_max {} bound_min {}", a_target->GetName(), a_target->GetBoundMax().Length(), a_target->GetBoundMin().Length());
    const auto a_obj_center = GetPosition(a_obj);
    const auto a_target_center = GetPosition(a_target);
    const auto a_obj_eff_rad = std::sqrtf(
        a_obj->GetBoundMax().Length() * a_obj->GetBoundMin().Length() *
        std::powf(a_obj->GetReferenceRuntimeData().refScale / 100.f, 2) / std::numbers::pi_v<float>);
    const auto a_target_eff_rad = std::sqrtf(
        a_target->GetBoundMax().Length() * a_target->GetBoundMin().Length() *
        std::powf(a_target->GetReferenceRuntimeData().refScale / 100.f, 2) / std::numbers::pi_v<float>);
    const auto distance = a_obj_center.GetDistance(a_target_center);
    //logger::info("Object effecive radius: {} Target effective radius: {} Distance: {}", a_obj_eff_rad, a_target_eff_rad, distance);
    if (distance < (a_obj_eff_rad + a_target_eff_rad) * Settings::search_scaling + range) {
        return true;
    }
    return false;
}

std::array<RE::NiPoint3, 8> WorldObject::GetBoundingBox(const RE::TESObjectREFR* a_obj) {
    //using namespace Math::LinAlg;
    const auto center = GetPosition(a_obj);
    const Math::LinAlg::Geometry geometry(a_obj);
    auto [min, max] = geometry.GetBoundingBox();

    min = center + min;
    max = center + max;

    const auto obj_angle = a_obj->GetAngle();

    const auto v1 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, min.y, min.z) - center, obj_angle) + center;
    const auto v2 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, min.y, min.z) - center, obj_angle) + center;
    const auto v3 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, max.y, min.z) - center, obj_angle) + center;
    const auto v4 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, max.y, min.z) - center, obj_angle) + center;

    const auto v5 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, min.y, max.z) - center, obj_angle) + center;
    const auto v6 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, min.y, max.z) - center, obj_angle) + center;
    const auto v7 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(max.x, max.y, max.z) - center, obj_angle) + center;
    const auto v8 = Math::LinAlg::Geometry::Rotate(RE::NiPoint3(min.x, max.y, max.z) - center, obj_angle) + center;

    //logger::trace("v1 ({},{},{}), v2 ({},{},{}), v3 ({},{},{}), v4 ({},{},{}), v5 ({},{},{}), v6 ({},{},{}), v7 ({},{},{}), v8 ({},{},{})",
    //		v1.x, v1.y, v1.z,
    //		v2.x, v2.y, v2.z,
    //		v3.x, v3.y, v3.z,
    //		v4.x, v4.y, v4.z,
    //		v5.x, v5.y, v5.z,
    //		v6.x, v6.y, v6.z,
    //		v7.x, v7.y, v7.z,
    //	    v8.x, v8.y, v8.z
    //);

    return {v1, v2, v3, v4, v5, v6, v7, v8};
}

void WorldObject::DrawBoundingBox(const RE::TESObjectREFR* a_obj) {
    const auto boundingBox = GetBoundingBox(a_obj);
    DrawBoundingBox(boundingBox);
}

void WorldObject::DrawBoundingBox(const std::array<RE::NiPoint3, 8>& a_box) {
    const auto& v1 = a_box[0];
    const auto& v2 = a_box[1];
    const auto& v3 = a_box[2];
    const auto& v4 = a_box[3];
    const auto& v5 = a_box[4];
    const auto& v6 = a_box[5];
    const auto& v7 = a_box[6];
    const auto& v8 = a_box[7];

    // Draw bottom face
    draw_line(v1, v2, 1);
    draw_line(v2, v3, 1);
    draw_line(v3, v4, 1);
    draw_line(v4, v1, 1);

    // Draw top face
    draw_line(v5, v6, 1);
    draw_line(v6, v7, 1);
    draw_line(v7, v8, 1);
    draw_line(v8, v5, 1);

    // Connect bottom and top faces
    draw_line(v1, v5, 1);
    draw_line(v2, v6, 1);
    draw_line(v3, v7, 1);
    draw_line(v4, v8, 1);
}

RE::NiPoint3 WorldObject::GetClosestPoint(const RE::NiPoint3& a_point_from, const RE::TESObjectREFR* a_obj_to) {
    using RE::NiPoint3;
    using namespace Math::LinAlg;

    // Center and orientation
    const NiPoint3 C = GetPosition(a_obj_to);
    const NiPoint3 angles = a_obj_to->GetAngle();

    // Local-space AABB (after uniform scale)
    const Geometry geom(a_obj_to);
    auto [minLocal, maxLocal] = geom.GetBoundingBox(); // local min/max

    // Build the box's world axes by rotating the canonical basis
    const NiPoint3 ux = Geometry::Rotate(NiPoint3{1.f, 0.f, 0.f}, angles); // world X-axis of the box
    const NiPoint3 uy = Geometry::Rotate(NiPoint3{0.f, 1.f, 0.f}, angles); // world Y-axis of the box
    const NiPoint3 uz = Geometry::Rotate(NiPoint3{0.f, 0.f, 1.f}, angles); // world Z-axis of the box

    // Project world point into the box's local coordinates (via dot with world axes)
    const NiPoint3 dP = a_point_from - C;
    const float px = dP.Dot(ux);
    const float py = dP.Dot(uy);
    const float pz = dP.Dot(uz);

    // Clamp per axis in local space
    const float qx = clampf(px, minLocal.x, maxLocal.x);
    const float qy = clampf(py, minLocal.y, maxLocal.y);
    const float qz = clampf(pz, minLocal.z, maxLocal.z);

    // Reconstruct closest point in world space
    // (linear combination of the world axes from the center)
    const NiPoint3 closest = C + ux * qx + uy * qy + uz * qz;
    return closest;
}

bool WorldObject::AreClose(const RE::TESObjectREFR* a_obj1, const RE::TESObjectREFR* a_obj2, const float threshold) {
    const auto a_obj1_center = GetPosition(a_obj1);
    const auto closest1 = GetClosestPoint(a_obj1_center, a_obj2);
    const auto closest2 = GetClosestPoint(closest1, a_obj1);
    if (closest1.GetDistance(closest2) < threshold) {
        #ifndef NDEBUG
        if (UI::draw_debug) {
            draw_line(closest1, closest2, 3, glm::vec4(1.f, 1.f, 1.f, 1.f));
        }
        #endif
        return true;
    }
    return false;
}

std::string String::toLowercase(const std::string& str) {
    std::string result = str;
    std::ranges::transform(result, result.begin(),
                           [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::string String::replaceLineBreaksWithSpace(const std::string& input) {
    std::string result = input;
    std::ranges::replace(result, '\n', ' ');
    return result;
}

std::string String::trim(const std::string& str) {
    // Find the first non-whitespace character from the beginning
    const size_t start = str.find_first_not_of(" \t\n\r");

    // If the string is all whitespace, return an empty string
    if (start == std::string::npos) return "";

    // Find the last non-whitespace character from the end
    const size_t end = str.find_last_not_of(" \t\n\r");

    // Return the substring containing the trimmed characters
    return str.substr(start, end - start + 1);
}

bool String::includesWord(const std::string& input, const std::vector<std::string>& strings) {
    std::string lowerInput = toLowercase(input);
    lowerInput = replaceLineBreaksWithSpace(lowerInput);
    lowerInput = trim(lowerInput);
    lowerInput = " " + lowerInput + " "; // Add spaces to the beginning and end of the string

    for (const auto& str : strings) {
        std::string lowerStr = str;
        lowerStr = trim(lowerStr);
        lowerStr = " " + lowerStr + " "; // Add spaces to the beginning and end of the string
        std::ranges::transform(lowerStr, lowerStr.begin(),
                               [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });

        //logger::trace("lowerInput: {} lowerStr: {}", lowerInput, lowerStr);

        if (lowerInput.find(lowerStr) != std::string::npos) {
            return true; // The input string includes one of the strings
        }
    }
    return false; // None of the strings in 'strings' were found in the input string
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
        char c = s[i++];
        if (c == '\\' && s[i]) {
            char t = s[i++];
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
                        int hv = hexVal(s[i]);
                        if (hv < 0) break;
                        val = val << 4 | hv;
                        ++i;
                        ++digits;
                        if (digits >= 2) break;
                    }
                    if (digits > 0) out.push_back(static_cast<wchar_t>(val));
                    else out.push_back(L'x');
                    break;
                }
                case 'u': {
                    int val = 0, digits = 0;
                    while (s[i] && digits < 4) {
                        int hv = hexVal(s[i]);
                        if (hv < 0) break;
                        val = val << 4 | hv;
                        ++i;
                        ++digits;
                    }
                    if (digits == 4) out.push_back(static_cast<wchar_t>(val));
                    else out.push_back(L'u');
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

bool xData::UpdateExtras(RE::ExtraDataList* copy_from, RE::ExtraDataList* copy_to) {
    logger::trace("UpdateExtras");
    if (!copy_from || !copy_to) return false;
    // Enchantment
    if (copy_from->HasType(RE::ExtraDataType::kEnchantment)) {
        logger::trace("Enchantment found");
        const auto enchantment =
            skyrim_cast<RE::ExtraEnchantment*>(copy_from->GetByType(RE::ExtraDataType::kEnchantment));
        if (enchantment) {
            RE::ExtraEnchantment* enchantment_fake = RE::BSExtraData::Create<RE::ExtraEnchantment>();
            // log the associated actor value
            logger::trace("Associated actor value: {}", enchantment->enchantment->GetAssociatedSkill());
            Copy::CopyEnchantment(enchantment, enchantment_fake);
            copy_to->Add(enchantment_fake);
        } else return false;
    }
    // Health
    if (copy_from->HasType(RE::ExtraDataType::kHealth)) {
        logger::trace("Health found");
        if (const auto health = skyrim_cast<RE::ExtraHealth*>(copy_from->GetByType(RE::ExtraDataType::kHealth))) {
            RE::ExtraHealth* health_fake = RE::BSExtraData::Create<RE::ExtraHealth>();
            Copy::CopyHealth(health, health_fake);
            copy_to->Add(health_fake);
        } else return false;
    }
    // Rank
    if (copy_from->HasType(RE::ExtraDataType::kRank)) {
        logger::trace("Rank found");
        if (const auto rank = skyrim_cast<RE::ExtraRank*>(copy_from->GetByType(RE::ExtraDataType::kRank))) {
            RE::ExtraRank* rank_fake = RE::BSExtraData::Create<RE::ExtraRank>();
            Copy::CopyRank(rank, rank_fake);
            copy_to->Add(rank_fake);
        } else return false;
    }
    // TimeLeft
    if (copy_from->HasType(RE::ExtraDataType::kTimeLeft)) {
        logger::trace("TimeLeft found");
        if (const auto timeleft = skyrim_cast<RE::ExtraTimeLeft*>(copy_from->GetByType(RE::ExtraDataType::kTimeLeft))) {
            RE::ExtraTimeLeft* timeleft_fake = RE::BSExtraData::Create<RE::ExtraTimeLeft>();
            Copy::CopyTimeLeft(timeleft, timeleft_fake);
            copy_to->Add(timeleft_fake);
        } else return false;
    }
    // Charge
    if (copy_from->HasType(RE::ExtraDataType::kCharge)) {
        logger::trace("Charge found");
        if (const auto charge = skyrim_cast<RE::ExtraCharge*>(copy_from->GetByType(RE::ExtraDataType::kCharge))) {
            RE::ExtraCharge* charge_fake = RE::BSExtraData::Create<RE::ExtraCharge>();
            Copy::CopyCharge(charge, charge_fake);
            copy_to->Add(charge_fake);
        } else return false;
    }
    // Scale
    if (copy_from->HasType(RE::ExtraDataType::kScale)) {
        logger::trace("Scale found");
        if (const auto scale = skyrim_cast<RE::ExtraScale*>(copy_from->GetByType(RE::ExtraDataType::kScale))) {
            RE::ExtraScale* scale_fake = RE::BSExtraData::Create<RE::ExtraScale>();
            Copy::CopyScale(scale, scale_fake);
            copy_to->Add(scale_fake);
        } else return false;
    }
    // UniqueID
    if (copy_from->HasType(RE::ExtraDataType::kUniqueID)) {
        logger::trace("UniqueID found");
        const auto uniqueid = skyrim_cast<RE::ExtraUniqueID*>(copy_from->GetByType(RE::ExtraDataType::kUniqueID));
        if (uniqueid) {
            RE::ExtraUniqueID* uniqueid_fake = RE::BSExtraData::Create<RE::ExtraUniqueID>();
            Copy::CopyUniqueID(uniqueid, uniqueid_fake);
            copy_to->Add(uniqueid_fake);
        } else return false;
    }
    // Poison
    if (copy_from->HasType(RE::ExtraDataType::kPoison)) {
        logger::trace("Poison found");
        if (const auto poison = skyrim_cast<RE::ExtraPoison*>(copy_from->GetByType(RE::ExtraDataType::kPoison))) {
            RE::ExtraPoison* poison_fake = RE::BSExtraData::Create<RE::ExtraPoison>();
            Copy::CopyPoison(poison, poison_fake);
            copy_to->Add(poison_fake);
        } else return false;
    }
    // ObjectHealth
    if (copy_from->HasType(RE::ExtraDataType::kObjectHealth)) {
        logger::trace("ObjectHealth found");
        const auto objhealth =
            skyrim_cast<RE::ExtraObjectHealth*>(copy_from->GetByType(RE::ExtraDataType::kObjectHealth));
        if (objhealth) {
            RE::ExtraObjectHealth* objhealth_fake = RE::BSExtraData::Create<RE::ExtraObjectHealth>();
            Copy::CopyObjectHealth(objhealth, objhealth_fake);
            copy_to->Add(objhealth_fake);
        } else return false;
    }
    // Light
    if (copy_from->HasType(RE::ExtraDataType::kLight)) {
        logger::trace("Light found");
        if (const auto light = skyrim_cast<RE::ExtraLight*>(copy_from->GetByType(RE::ExtraDataType::kLight))) {
            RE::ExtraLight* light_fake = RE::BSExtraData::Create<RE::ExtraLight>();
            Copy::CopyLight(light, light_fake);
            copy_to->Add(light_fake);
        } else return false;
    }
    // Radius
    if (copy_from->HasType(RE::ExtraDataType::kRadius)) {
        logger::trace("Radius found");
        if (const auto radius = skyrim_cast<RE::ExtraRadius*>(copy_from->GetByType(RE::ExtraDataType::kRadius))) {
            RE::ExtraRadius* radius_fake = RE::BSExtraData::Create<RE::ExtraRadius>();
            Copy::CopyRadius(radius, radius_fake);
            copy_to->Add(radius_fake);
        } else return false;
    }
    // Sound (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kSound)) {
        logger::trace("Sound found");
        auto sound = static_cast<RE::ExtraSound*>(copy_from->GetByType(RE::ExtraDataType::kSound));
        if (sound) {
            RE::ExtraSound* sound_fake = RE::BSExtraData::Create<RE::ExtraSound>();
            sound_fake->handle = sound->handle;
            copy_to->Add(sound_fake);
        } else
            RaiseMngrErr("Failed to get radius from copy_from");
    }*/
    // LinkedRef (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kLinkedRef)) {
        logger::trace("LinkedRef found");
        auto linkedref =
            static_cast<RE::ExtraLinkedRef*>(copy_from->GetByType(RE::ExtraDataType::kLinkedRef));
        if (linkedref) {
            RE::ExtraLinkedRef* linkedref_fake = RE::BSExtraData::Create<RE::ExtraLinkedRef>();
            linkedref_fake->linkedRefs = linkedref->linkedRefs;
            copy_to->Add(linkedref_fake);
        } else
            RaiseMngrErr("Failed to get linkedref from copy_from");
    }*/
    // Horse
    if (copy_from->HasType(RE::ExtraDataType::kHorse)) {
        logger::trace("Horse found");
        if (const auto horse = skyrim_cast<RE::ExtraHorse*>(copy_from->GetByType(RE::ExtraDataType::kHorse))) {
            RE::ExtraHorse* horse_fake = RE::BSExtraData::Create<RE::ExtraHorse>();
            Copy::CopyHorse(horse, horse_fake);
            copy_to->Add(horse_fake);
        } else return false;
    }
    // Hotkey
    if (copy_from->HasType(RE::ExtraDataType::kHotkey)) {
        logger::trace("Hotkey found");
        if (const auto hotkey = skyrim_cast<RE::ExtraHotkey*>(copy_from->GetByType(RE::ExtraDataType::kHotkey))) {
            RE::ExtraHotkey* hotkey_fake = RE::BSExtraData::Create<RE::ExtraHotkey>();
            Copy::CopyHotkey(hotkey, hotkey_fake);
            copy_to->Add(hotkey_fake);
        } else return false;
    }
    // Weapon Attack Sound (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kWeaponAttackSound)) {
        logger::trace("WeaponAttackSound found");
        auto weaponattacksound = static_cast<RE::ExtraWeaponAttackSound*>(
            copy_from->GetByType(RE::ExtraDataType::kWeaponAttackSound));
        if (weaponattacksound) {
            RE::ExtraWeaponAttackSound* weaponattacksound_fake =
                RE::BSExtraData::Create<RE::ExtraWeaponAttackSound>();
            weaponattacksound_fake->handle = weaponattacksound->handle;
            copy_to->Add(weaponattacksound_fake);
        } else
            RaiseMngrErr("Failed to get weaponattacksound from copy_from");
    }*/
    // Activate Ref (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kActivateRef)) {
        logger::trace("ActivateRef found");
        auto activateref =
            static_cast<RE::ExtraActivateRef*>(copy_from->GetByType(RE::ExtraDataType::kActivateRef));
        if (activateref) {
            RE::ExtraActivateRef* activateref_fake = RE::BSExtraData::Create<RE::ExtraActivateRef>();
            activateref_fake->parents = activateref->parents;
            activateref_fake->activateFlags = activateref->activateFlags;
        } else
            RaiseMngrErr("Failed to get activateref from copy_from");
    }*/
    // TextDisplayData
    if (copy_from->HasType(RE::ExtraDataType::kTextDisplayData)) {
        logger::trace("TextDisplayData found");
        const auto textdisplaydata =
            skyrim_cast<RE::ExtraTextDisplayData*>(copy_from->GetByType(RE::ExtraDataType::kTextDisplayData));
        if (textdisplaydata) {
            RE::ExtraTextDisplayData* textdisplaydata_fake =
                RE::BSExtraData::Create<RE::ExtraTextDisplayData>();
            Copy::CopyTextDisplayData(textdisplaydata, textdisplaydata_fake);
            copy_to->Add(textdisplaydata_fake);
        } else return false;
    }
    // Soul
    if (copy_from->HasType(RE::ExtraDataType::kSoul)) {
        logger::trace("Soul found");
        if (const auto soul = skyrim_cast<RE::ExtraSoul*>(copy_from->GetByType(RE::ExtraDataType::kSoul))) {
            RE::ExtraSoul* soul_fake = RE::BSExtraData::Create<RE::ExtraSoul>();
            Copy::CopySoul(soul, soul_fake);
            copy_to->Add(soul_fake);
        } else return false;
    }
    // Flags (OK)
    if (copy_from->HasType(RE::ExtraDataType::kFlags)) {
        logger::trace("Flags found");
        if (const auto flags = skyrim_cast<RE::ExtraFlags*>(copy_from->GetByType(RE::ExtraDataType::kFlags))) {
            SKSE::stl::enumeration<RE::ExtraFlags::Flag, std::uint32_t> flags_fake;
            if (flags->flags.all(RE::ExtraFlags::Flag::kBlockActivate))
                flags_fake.set(RE::ExtraFlags::Flag::kBlockActivate);
            if (flags->flags.all(RE::ExtraFlags::Flag::kBlockPlayerActivate))
                flags_fake.set(RE::ExtraFlags::Flag::kBlockPlayerActivate);
            if (flags->flags.all(RE::ExtraFlags::Flag::kBlockLoadEvents))
                flags_fake.set(RE::ExtraFlags::Flag::kBlockLoadEvents);
            if (flags->flags.all(RE::ExtraFlags::Flag::kBlockActivateText))
                flags_fake.set(RE::ExtraFlags::Flag::kBlockActivateText);
            if (flags->flags.all(RE::ExtraFlags::Flag::kPlayerHasTaken))
                flags_fake.set(RE::ExtraFlags::Flag::kPlayerHasTaken);
            // RE::ExtraFlags* flags_fake = RE::BSExtraData::Create<RE::ExtraFlags>();
            // flags_fake->flags = flags->flags;
            // copy_to->Add(flags_fake);
        } else return false;
    }
    // Lock (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kLock)) {
        logger::trace("Lock found");
        auto lock = static_cast<RE::ExtraLock*>(copy_from->GetByType(RE::ExtraDataType::kLock));
        if (lock) {
            RE::ExtraLock* lock_fake = RE::BSExtraData::Create<RE::ExtraLock>();
            lock_fake->lock = lock->lock;
            copy_to->Add(lock_fake);
        } else
            RaiseMngrErr("Failed to get lock from copy_from");
    }*/
    // Teleport (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kTeleport)) {
        logger::trace("Teleport found");
        auto teleport =
            static_cast<RE::ExtraTeleport*>(copy_from->GetByType(RE::ExtraDataType::kTeleport));
        if (teleport) {
            RE::ExtraTeleport* teleport_fake = RE::BSExtraData::Create<RE::ExtraTeleport>();
            teleport_fake->teleportData = teleport->teleportData;
            copy_to->Add(teleport_fake);
        } else
            RaiseMngrErr("Failed to get teleport from copy_from");
    }*/
    // LockList (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kLockList)) {
        logger::trace("LockList found");
        auto locklist =
            static_cast<RE::ExtraLockList*>(copy_from->GetByType(RE::ExtraDataType::kLockList));
        if (locklist) {
            RE::ExtraLockList* locklist_fake = RE::BSExtraData::Create<RE::ExtraLockList>();
            locklist_fake->list = locklist->list;
            copy_to->Add(locklist_fake);
        } else
            RaiseMngrErr("Failed to get locklist from copy_from");
    }*/
    // OutfitItem (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kOutfitItem)) {
        logger::trace("OutfitItem found");
        auto outfititem =
            static_cast<RE::ExtraOutfitItem*>(copy_from->GetByType(RE::ExtraDataType::kOutfitItem));
        if (outfititem) {
            RE::ExtraOutfitItem* outfititem_fake = RE::BSExtraData::Create<RE::ExtraOutfitItem>();
            outfititem_fake->id = outfititem->id;
            copy_to->Add(outfititem_fake);
        } else
            RaiseMngrErr("Failed to get outfititem from copy_from");
    }*/
    // CannotWear (Disabled)
    /*if (copy_from->HasType(RE::ExtraDataType::kCannotWear)) {
        logger::trace("CannotWear found");
        auto cannotwear =
            static_cast<RE::ExtraCannotWear*>(copy_from->GetByType(RE::ExtraDataType::kCannotWear));
        if (cannotwear) {
            RE::ExtraCannotWear* cannotwear_fake = RE::BSExtraData::Create<RE::ExtraCannotWear>();
            copy_to->Add(cannotwear_fake);
        } else
            RaiseMngrErr("Failed to get cannotwear from copy_from");
    }*/
    // Ownership (OK)
    if (copy_from->HasType(RE::ExtraDataType::kOwnership)) {
        logger::trace("Ownership found");
        if (const auto ownership = skyrim_cast<RE::ExtraOwnership
            *>(copy_from->GetByType(RE::ExtraDataType::kOwnership))) {
            logger::trace("length of fake extradatalist: {}", copy_to->GetCount());
            RE::ExtraOwnership* ownership_fake = RE::BSExtraData::Create<RE::ExtraOwnership>();
            Copy::CopyOwnership(ownership, ownership_fake);
            copy_to->Add(ownership_fake);
            logger::trace("length of fake extradatalist: {}", copy_to->GetCount());
        } else return false;
    }

    return true;
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

bool Inventory::EntryHasXData(const RE::InventoryEntryData* entry) {
    if (entry && entry->extraLists && !entry->extraLists->empty()) return true;
    return false;
}

bool Inventory::HasItemEntry(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner,
                             const bool nonzero_entry_check) {
    if (!item) {
        logger::warn("Item is null");
        return false;
    }
    if (!inventory_owner) {
        logger::warn("Inventory owner is null");
        return false;
    }
    auto inventory = inventory_owner->GetInventory();
    const auto it = inventory.find(item);
    const bool has_entry = it != inventory.end();
    if (nonzero_entry_check) return has_entry && it->second.first > 0;
    return has_entry;
}

std::int32_t Inventory::GetItemCount(RE::TESBoundObject* item, RE::TESObjectREFR* inventory_owner) {
    if (HasItemEntry(item, inventory_owner, true)) {
        const auto inventory = inventory_owner->GetInventory();
        const auto it = inventory.find(item);
        return it->second.first;
    }
    return 0;
}

bool Inventory::IsQuestItem(const FormID formid, RE::TESObjectREFR* inv_owner) {
    const auto inventory = inv_owner->GetInventory();
    if (auto item = FormReader::GetFormByID<RE::TESBoundObject>(formid)) {
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

void xData::Copy::CopyEnchantment(const RE::ExtraEnchantment* from, RE::ExtraEnchantment* to) {
    logger::trace("CopyEnchantment");
    to->enchantment = from->enchantment;
    to->charge = from->charge;
    to->removeOnUnequip = from->removeOnUnequip;
}

void xData::Copy::CopyHealth(const RE::ExtraHealth* from, RE::ExtraHealth* to) {
    logger::trace("CopyHealth");
    to->health = from->health;
}

void xData::Copy::CopyRank(const RE::ExtraRank* from, RE::ExtraRank* to) {
    logger::trace("CopyRank");
    to->rank = from->rank;
}

void xData::Copy::CopyTimeLeft(const RE::ExtraTimeLeft* from, RE::ExtraTimeLeft* to) {
    logger::trace("CopyTimeLeft");
    to->time = from->time;
}

void xData::Copy::CopyCharge(const RE::ExtraCharge* from, RE::ExtraCharge* to) {
    logger::trace("CopyCharge");
    to->charge = from->charge;
}

void xData::Copy::CopyScale(const RE::ExtraScale* from, RE::ExtraScale* to) {
    logger::trace("CopyScale");
    to->scale = from->scale;
}

void xData::Copy::CopyUniqueID(const RE::ExtraUniqueID* from, RE::ExtraUniqueID* to) {
    logger::trace("CopyUniqueID");
    to->baseID = from->baseID;
    to->uniqueID = from->uniqueID;
}

void xData::Copy::CopyPoison(const RE::ExtraPoison* from, RE::ExtraPoison* to) {
    logger::trace("CopyPoison");
    to->poison = from->poison;
    to->count = from->count;
}

void xData::Copy::CopyObjectHealth(const RE::ExtraObjectHealth* from, RE::ExtraObjectHealth* to) {
    logger::trace("CopyObjectHealth");
    to->health = from->health;
}

void xData::Copy::CopyLight(const RE::ExtraLight* from, RE::ExtraLight* to) {
    logger::trace("CopyLight");
    to->lightData = from->lightData;
}

void xData::Copy::CopyRadius(const RE::ExtraRadius* from, RE::ExtraRadius* to) {
    logger::trace("CopyRadius");
    to->radius = from->radius;
}

void xData::Copy::CopyHorse(const RE::ExtraHorse* from, RE::ExtraHorse* to) {
    logger::trace("CopyHorse");
    to->horseRef = from->horseRef;
}

void xData::Copy::CopyHotkey(const RE::ExtraHotkey* from, RE::ExtraHotkey* to) {
    logger::trace("CopyHotkey");
    to->hotkey = from->hotkey;
}

void xData::Copy::CopyTextDisplayData(const RE::ExtraTextDisplayData* from, RE::ExtraTextDisplayData* to) {
    to->displayName = from->displayName;
    to->displayNameText = from->displayNameText;
    to->ownerQuest = from->ownerQuest;
    to->ownerInstance = from->ownerInstance;
    to->temperFactor = from->temperFactor;
    to->customNameLength = from->customNameLength;
}

void xData::Copy::CopySoul(const RE::ExtraSoul* from, RE::ExtraSoul* to) {
    logger::trace("CopySoul");
    to->soul = from->soul;
}

void xData::Copy::CopyOwnership(const RE::ExtraOwnership* from, RE::ExtraOwnership* to) {
    logger::trace("CopyOwnership");
    to->owner = from->owner;
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
            auto inventory_menu = ui->GetMenu<RE::InventoryMenu>();
            if (auto itemlist = inventory_menu->GetRuntimeData().itemList) {
                itemlist->Update();
            } else logger::error("Itemlist is null.");
        } else if (ui->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
            auto barter_menu = ui->GetMenu<RE::BarterMenu>();
            if (auto itemlist = barter_menu->GetRuntimeData().itemList) {
                itemlist->Update();
            } else logger::error("Itemlist is null.");
        } else if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
            auto container_menu = ui->GetMenu<RE::ContainerMenu>();
            if (auto itemlist = container_menu->GetRuntimeData().itemList) {
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
    if (auto owner = RE::TESObjectREFR::LookupByHandle(refHandle)) {
        return owner.get();
    }
    if (auto owner_actor = RE::Actor::LookupByHandle(refHandle)) {
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

void Math::LinAlg::Geometry::FetchVertices(const RE::BSGeometry* o3d, RE::BSGraphics::TriShape* triShape) {
    if (const uint8_t* vertexData = triShape->rawVertexData) {
        const uint32_t stride = triShape->vertexDesc.GetSize();
        const auto numPoints = GetBufferLength(triShape->vertexBuffer);
        const auto numPositions = numPoints / stride;
        positions.reserve(positions.size() + numPositions);
        for (uint32_t i = 0; i < numPoints; i += stride) {
            const uint8_t* currentVertex = vertexData + i;

            const auto position =
                reinterpret_cast<const float*>(currentVertex + triShape->vertexDesc.GetAttributeOffset(
                                                   RE::BSGraphics::Vertex::Attribute::VA_POSITION));

            auto pos = RE::NiPoint3{position[0], position[1], position[2]};
            pos = o3d->local * pos;
            positions.push_back(pos);
        }
    }
}

RE::NiPoint3 Math::LinAlg::Geometry::Rotate(const RE::NiPoint3& A, const RE::NiPoint3& angles) {
    RE::NiMatrix3 R;
    R.SetEulerAnglesXYZ(angles);
    return R * A;
}

Math::LinAlg::Geometry::Geometry(const RE::TESObjectREFR* obj) {
    this->obj = obj;
    EachGeometry(obj, [this](const RE::BSGeometry* o3d, RE::BSGraphics::TriShape* triShape) -> void {
        FetchVertices(o3d, triShape);
        //FetchIndexes(triShape);
    });

    if (positions.empty()) {
        auto from = obj->GetBoundMin();
        auto to = obj->GetBoundMax();

        if ((to - from).Length() < 1) {
            from = {-5, -5, -5};
            to = {5, 5, 5};
        }
        positions.emplace_back(from.x, from.y, from.z);
        positions.emplace_back(to.x, from.y, from.z);
        positions.emplace_back(to.x, to.y, from.z);
        positions.emplace_back(from.x, to.y, from.z);

        positions.emplace_back(from.x, from.y, to.z);
        positions.emplace_back(to.x, from.y, to.z);
        positions.emplace_back(to.x, to.y, to.z);
        positions.emplace_back(from.x, to.y, to.z);
    }
}

std::pair<RE::NiPoint3, RE::NiPoint3> Math::LinAlg::Geometry::GetBoundingBox() const {
    auto min = RE::NiPoint3{0, 0, 0};
    auto max = RE::NiPoint3{0, 0, 0};

    for (auto i = 0; i < positions.size(); i++) {
        //const auto p1 = Rotate(positions[i] * scale, angle);
        const auto p1 = positions[i] * obj->GetScale();

        if (p1.x < min.x) {
            min.x = p1.x;
        }
        if (p1.x > max.x) {
            max.x = p1.x;
        }
        if (p1.y < min.y) {
            min.y = p1.y;
        }
        if (p1.y > max.y) {
            max.y = p1.y;
        }
        if (p1.z < min.z) {
            min.z = p1.z;
        }
        if (p1.z > max.z) {
            max.z = p1.z;
        }
    }

    return std::pair(min, max);
}