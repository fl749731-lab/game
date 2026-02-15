#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Engine {

// ── 物品类型 ──────────────────────────────────────────────

enum class ItemCategory : u8 {
    None = 0,
    Tool,
    Seed,
    Crop,
    Food,
    Resource,
    Fish,
    Gift,
    Misc,
};

// ── 物品定义 ──────────────────────────────────────────────

struct ItemDef {
    u32           ID = 0;
    std::string   Name;
    std::string   Description;
    std::string   IconTexture;
    u32           IconIndex = 0;
    ItemCategory  Category = ItemCategory::None;
    u32           MaxStack = 99;
    u32           SellPrice = 0;
    u32           BuyPrice = 0;
    f32           StaminaRestore = 0;
};

// ── 背包中的物品槽 ────────────────────────────────────────

struct ItemSlot {
    u32  ItemID = 0;
    u32  Count  = 0;
    bool IsEmpty() const { return ItemID == 0 || Count == 0; }
    void Clear() { ItemID = 0; Count = 0; }
};

// ── 物品数据库 (全局单例) ─────────────────────────────────

class ItemDatabase {
public:
    static ItemDatabase& Get();
    void Register(const ItemDef& def);
    const ItemDef* Find(u32 id) const;
    const ItemDef* FindByName(const std::string& name) const;
    const std::unordered_map<u32, ItemDef>& GetAll() const { return m_Items; }

private:
    std::unordered_map<u32, ItemDef> m_Items;
    std::unordered_map<std::string, u32> m_NameIndex;
};

// ── 背包组件 ──────────────────────────────────────────────

struct InventoryComponent : public Component {
    std::vector<ItemSlot> Slots;
    u32 HotbarSize  = 10;
    u32 SelectedSlot = 0;
    u32 Gold = 500;

    void Init(u32 totalSlots = 36) { Slots.resize(totalSlots); }

    u32 AddItem(u32 itemID, u32 count);
    u32 RemoveItem(u32 itemID, u32 count);
    bool HasItem(u32 itemID, u32 count = 1) const;
    u32 CountItem(u32 itemID) const;
    const ItemSlot& GetSelectedItem() const { return Slots[SelectedSlot]; }
    ItemSlot& GetSelectedItem() { return Slots[SelectedSlot]; }
    void SwapSlots(u32 a, u32 b);
};

} // namespace Engine
