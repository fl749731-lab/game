#include "game/inventory.h"
#include "engine/core/log.h"

namespace Engine {

ItemDatabase& ItemDatabase::Get() {
    static ItemDatabase s_Instance;
    return s_Instance;
}

void ItemDatabase::Register(const ItemDef& def) {
    m_Items[def.ID] = def;
    m_NameIndex[def.Name] = def.ID;
    LOG_INFO("[物品库] 注册: #%u %s", def.ID, def.Name.c_str());
}

const ItemDef* ItemDatabase::Find(u32 id) const {
    auto it = m_Items.find(id);
    return it != m_Items.end() ? &it->second : nullptr;
}

const ItemDef* ItemDatabase::FindByName(const std::string& name) const {
    auto it = m_NameIndex.find(name);
    if (it == m_NameIndex.end()) return nullptr;
    return Find(it->second);
}

u32 InventoryComponent::AddItem(u32 itemID, u32 count) {
    if (count == 0 || itemID == 0) return 0;
    auto* def = ItemDatabase::Get().Find(itemID);
    u32 maxStack = def ? def->MaxStack : 99;
    u32 remaining = count;
    for (auto& slot : Slots) {
        if (remaining == 0) break;
        if (slot.ItemID == itemID && slot.Count < maxStack) {
            u32 toAdd = std::min(remaining, maxStack - slot.Count);
            slot.Count += toAdd;
            remaining -= toAdd;
        }
    }
    for (auto& slot : Slots) {
        if (remaining == 0) break;
        if (slot.IsEmpty()) {
            u32 toAdd = std::min(remaining, maxStack);
            slot.ItemID = itemID;
            slot.Count = toAdd;
            remaining -= toAdd;
        }
    }
    return remaining;
}

u32 InventoryComponent::RemoveItem(u32 itemID, u32 count) {
    u32 toRemove = count;
    for (auto& slot : Slots) {
        if (toRemove == 0) break;
        if (slot.ItemID == itemID) {
            if (slot.Count <= toRemove) {
                toRemove -= slot.Count;
                slot.Clear();
            } else {
                slot.Count -= toRemove;
                toRemove = 0;
            }
        }
    }
    return count - toRemove;
}

bool InventoryComponent::HasItem(u32 itemID, u32 count) const {
    return CountItem(itemID) >= count;
}

u32 InventoryComponent::CountItem(u32 itemID) const {
    u32 total = 0;
    for (auto& slot : Slots) if (slot.ItemID == itemID) total += slot.Count;
    return total;
}

void InventoryComponent::SwapSlots(u32 a, u32 b) {
    if (a >= Slots.size() || b >= Slots.size()) return;
    std::swap(Slots[a], Slots[b]);
}

} // namespace Engine
