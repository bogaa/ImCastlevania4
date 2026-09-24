#include "PropertyPanel.h"

#include "EditorUndo.h"
#include "EventNames.h"
#include "imgui.h"
#include "SC4Core.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <list>
#include <set>
#include <string>
#include <vector>


namespace {

    struct PropertyUiState {
        int whip = 0;
        int subweapon = 0;
        int movement = 0;
        int checkpoint = 0;
        int cameraLock = 0;
        int nextLevelDirection = 0;
        int exitCheck = 0;
        int exitType = 0;
        int enemyToAdd = 0x07;
        std::string enemyAddStatus;
    };
    
    static PropertyUiState g_propertyState;
    
    // data tables level
    static constexpr unsigned LEVEL_TYPE = 0x868296;
    static constexpr unsigned LEVEL_BG_MOD = 0x85C736;
    static constexpr unsigned LEVEL_BG_PROPERTY_MASK_BASE = 0x85C7BE;
    static constexpr unsigned LEVEL_BG_SCROLL_BASE = 0x85C846;
    static constexpr unsigned LEVEL_TILE1_ANIMATION_POINTER_BASE = 0x85CA82;
    static constexpr unsigned LEVEL_TILE2_ANIMATION_POINTER_BASE = 0x85cb0a;
    static constexpr unsigned LEVEL_PALETTE_ANIMATION_POINTER_BASE = 0x86946f;
    static constexpr unsigned LEVEL_TIMER = 0x85BCF8;
    static constexpr unsigned LEVEL_DAMAGE_BUFF = 0x81A88F;
    static constexpr unsigned LEVEL_MUSIK = 0x8097C3;
    static constexpr unsigned LEVEL_CONTINUE = 0x81FBAC;
    static constexpr unsigned LEVEL_LOAD_DIRECTION = 0x80D8A3; 
    static constexpr unsigned LEVEL_ALWAYS_3SCRL = 0x85BD80;

    // data tables event   
    static constexpr unsigned SUBWEAPON_DAMAGE_BASE = 0x81A6F8;
    static constexpr unsigned EVENT_BREAKABLE_WALL_ITEM_BASE = 0x81A81A;
    static constexpr unsigned EVENT_HITBOX_BASE = 0x81AB00;
    static constexpr unsigned EVENT_HEALTH_BASE = 0x81AC00;
    static constexpr unsigned EVENT_HIT_ATTRIBUTE_BASE = 0x81AD00;  // 01 hurt, 04 whip hitable, 08 collect able also needs bit 01 set, 10 ??, 20 ??, 40 rossery, 80 noDespawn 
    static constexpr unsigned EVENT_DEATH_ANIMATION_BASE = 0x81AE00;
    static constexpr unsigned EVENT_DEATH_MOVBITS_BASE = 0x81AE80;
    static constexpr unsigned EVENT_DAMAGE_BASE = 0x81af00;
    static constexpr unsigned EVENT_SLOT_SIZE = 0x81AA80;
    static constexpr unsigned RING_CONVEYOR_X = 0x81fcbe;
    static constexpr unsigned RING_CONVEYOR_Y = 0x81fcc0;


    // expansion 
    static constexpr unsigned EXP_LEVEL_TRANSIT = 0xA0C000;         // AA BB    AA = level BB = checkpoint. 8 Enteries
    static constexpr unsigned EXP_EV15_Exit = 0xA68000;             // AB CC    A = Type, B = transitionID CC = CMP pos. 0x3F Entries  


    
    // routines
    // static constexpr unsigned TRIPLE_SHOT_PICKUP_JML = 0x80DFA3;
    // static constexpr unsigned AXE_STATE01_HOOK = 0x80BB05;
    // static constexpr unsigned KNIFE_STATE_POINTER = 0x80BA50;
    // static constexpr unsigned KNIFE_STATE_JML_STUB = 0x80FEDB;
    // static constexpr unsigned CLEAR_SELECTED_EVENT_SLOT_ALL = 0x808C59;
    // static constexpr unsigned LUNCH_SFX_FROM_ACCUM = 0x8085E3;
    // static constexpr unsigned READ_COLLISION_TABLE_7E4000 = 0x80CF86;
    // static constexpr unsigned MAKE_THIS_ENTITY_PLATFORM = 0x82C312;
    // static constexpr unsigned AXE_COUNTER = 0x80BB3A;
    // static constexpr unsigned AXE_ANIMATION = 0x80BB44;
    // static constexpr unsigned AXE_SPEED_MOVEMENT = 0x80BB11;
    // static constexpr unsigned CRUMBLE_BLOCK_BURN = 0x8290D1;

    static float ValueColumnWidth()
    {
        const float available = ImGui::GetContentRegionAvail().x;
        return available < 270.0f ? 104.0f : 128.0f;
    }

    static float LabelColumnWidth(float valueWidth)
    {
        const float available = ImGui::GetContentRegionAvail().x;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        return available > valueWidth + spacing ? available - valueWidth - spacing : available * 0.55f;
    }

    static void BeginPropertyRow(const char* label, float valueWidth)
    {
        ImGui::PushID(label);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(ImGui::GetCursorPosX() + LabelColumnWidth(valueWidth));
        ImGui::SetNextItemWidth(valueWidth);
    }

    static void EndPropertyRow()
    {
        ImGui::PopID();
    }

    static bool ComboRow(const char* label, int& value, const std::vector<std::string>& items)
    {
        const char* preview = items.empty() ? "" : items[static_cast<size_t>(value)].c_str();
        bool changed = false;
        const float valueWidth = ValueColumnWidth();
        BeginPropertyRow(label, valueWidth);
        if (ImGui::BeginCombo("##value", preview)) {
            for (int i = 0; i < static_cast<int>(items.size()); ++i) {
                const bool selected = value == i;
                if (ImGui::Selectable(items[static_cast<size_t>(i)].c_str(), selected)) {
                    value = i;
                    changed = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        EndPropertyRow();
        return changed;
    }

    static std::vector<std::string> NumberItems(int count)
    {
        std::vector<std::string> items;
        items.reserve(static_cast<size_t>(count));
        char text[16] = {};
        for (int i = 0; i < count; ++i) {
            std::snprintf(text, sizeof(text), "%d", i);
            items.emplace_back(text);
        }
        return items;
    }

    static void DrawNumberProperty(EditorState& state, const char* label, int byteCount, const std::vector<unsigned>& addresses, bool enabled = true)
    {
        RomSession& session = state.session;
        const bool canEdit = session.IsLoaded() && enabled && !addresses.empty() && addresses.front() != 0;
        int value = canEdit ? static_cast<int>(session.ReadRom(addresses.front(), byteCount)) : 0;
    
        ImGui::BeginDisabled(!canEdit);
        const float valueWidth = byteCount == 1 ? 96.0f : byteCount == 2 ? 112.0f : 124.0f;
        BeginPropertyRow(label, valueWidth);

        if (ImGui::InputInt("##value", &value, 1, 8, ImGuiInputTextFlags_AutoSelectAll)) {
            const unsigned mask = byteCount == 1 ? 0xFFu : byteCount == 2 ? 0xFFFFu : 0xFFFFFFFFu;
            session.WriteRomAll(addresses, byteCount, static_cast<unsigned>(value) & mask);
            state.levelRenderer.Invalidate();
        }
        if (ImGui::IsItemHovered() && !addresses.empty()) {
            if (addresses.size() == 1) {
                ImGui::SetTooltip("ROM address: %06X", addresses.front());
            } else {
                ImGui::SetTooltip("Writes %zu mirrored ROM addresses", addresses.size());
            }
        }
        EndPropertyRow();
        ImGui::EndDisabled();
    }

        static void DrawNumberPropertySliderHex(EditorState& state, const char* label, int byteCount, const std::vector<unsigned>& addresses, int minValue, int maxValue, bool enabled = true)
        {
            RomSession& session = state.session;
            const bool canEdit = session.IsLoaded() && enabled && !addresses.empty() && addresses.front() != 0;
            int value = canEdit ? static_cast<int>(session.ReadRom(addresses.front(), byteCount)) : minValue;
            value = std::clamp(value, minValue, maxValue);

            ImGui::BeginDisabled(!canEdit);
            const float valueWidth = byteCount == 1 ? 192.0f : byteCount == 2 ? 264.0f : 336.0f;
            BeginPropertyRow(label, valueWidth);

            const char* valueFormat = byteCount == 1 ? "%02X" : byteCount == 2 ? "%04X" : "%08X";
            if (ImGui::SliderInt("##value", &value, minValue, maxValue, valueFormat)) {
                const unsigned mask = byteCount == 1 ? 0xFFu : byteCount == 2 ? 0xFFFFu : 0xFFFFFFFFu;
                session.WriteRomAll(addresses, byteCount, static_cast<unsigned>(value) & mask);
                state.levelRenderer.Invalidate();
            }
            if (ImGui::IsItemHovered() && !addresses.empty()) {
                if (addresses.size() == 1) {
                    ImGui::SetTooltip("ROM address: %06X", addresses.front());
                }
                else {
                    ImGui::SetTooltip("Writes %zu mirrored ROM addresses", addresses.size());
                }
            }
            EndPropertyRow();
            ImGui::EndDisabled();
        }


    static void DrawNumberPropertySlider(EditorState& state, const char* label, int byteCount, const std::vector<unsigned>& addresses, int minValue, int maxValue, bool enabled = true)
    {
        RomSession& session = state.session;
        const bool canEdit = session.IsLoaded() && enabled && !addresses.empty() && addresses.front() != 0;
        int value = canEdit ? static_cast<int>(session.ReadRom(addresses.front(), byteCount)) : minValue;
        value = std::clamp(value, minValue, maxValue);

        ImGui::BeginDisabled(!canEdit);
        const float valueWidth = byteCount == 1 ? 192.0f : byteCount == 2 ? 264.0f : 336.0f;
        BeginPropertyRow(label, valueWidth);

        if (ImGui::SliderInt("##value", &value, minValue, maxValue)) {
            const unsigned mask = byteCount == 1 ? 0xFFu : byteCount == 2 ? 0xFFFFu : 0xFFFFFFFFu;
            session.WriteRomAll(addresses, byteCount, static_cast<unsigned>(value) & mask);
            state.levelRenderer.Invalidate();
        }
        if (ImGui::IsItemHovered() && !addresses.empty()) {
            if (addresses.size() == 1) {
                ImGui::SetTooltip("ROM address: %06X", addresses.front());
            } else {
                ImGui::SetTooltip("Writes %zu mirrored ROM addresses", addresses.size());
            }
        }
        EndPropertyRow();
        ImGui::EndDisabled();
    }

    static void DrawNumberPropertyCombo(EditorState& state, const char* label, int byteCount, const std::vector<unsigned>& addresses, int minValue, int maxValue, bool enabled = true)
    {
        RomSession& session = state.session;
        const bool canEdit = session.IsLoaded() && enabled && !addresses.empty() && addresses.front() != 0;
        int value = canEdit ? static_cast<int>(session.ReadRom(addresses.front(), byteCount)) : minValue;
        value = std::clamp(value, minValue, maxValue);
        int comboValue = value - minValue;
        const std::vector<std::string> items = NumberItems(maxValue - minValue + 1);

        ImGui::BeginDisabled(!canEdit);
        if (ComboRow(label, comboValue, items)) {
            value = comboValue + minValue;
            const unsigned mask = byteCount == 1 ? 0xFFu : byteCount == 2 ? 0xFFFFu : 0xFFFFFFFFu;
            session.WriteRomAll(addresses, byteCount, static_cast<unsigned>(value) & mask);
            state.levelRenderer.Invalidate();
        }
        if (ImGui::IsItemHovered() && !addresses.empty()) {
            if (addresses.size() == 1) {
                ImGui::SetTooltip("ROM address: %06X", addresses.front());
            } else {
                ImGui::SetTooltip("Writes %zu mirrored ROM addresses", addresses.size());
            }
        }
        ImGui::EndDisabled();
    }

    static void DrawBitfieldWordProperty(EditorState& state, const char* label, const std::vector<unsigned>& addresses, bool enabled = true)
    {
        RomSession& session = state.session;
        const bool canEdit = session.IsLoaded() && enabled && !addresses.empty() && addresses.front() != 0;
        const unsigned raw = canEdit ? session.ReadRom(addresses.front(), 2) & 0xFFFFu : 0;
        unsigned newValue = raw;

        ImGui::BeginDisabled(!canEdit);
        ImGui::PushID(label);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        ImGui::TextDisabled("%u (0x%04X)", raw, raw);

        for (unsigned bit = 0; bit < 16; ++bit) {
            if (bit != 0) {
                ImGui::SameLine();
            }

            bool enabledBit = (raw & (1u << bit)) != 0;
            ImGui::PushID(static_cast<int>(bit));
            if (ImGui::Checkbox("##bit", &enabledBit)) {
                if (enabledBit) {
                    newValue |= 1u << bit;
                } else {
                    newValue &= ~(1u << bit);
                }
            }
            ImGui::SameLine(0.0f, 2.0f);
            ImGui::Text("%u", bit);
            ImGui::PopID();
        }

        if (ImGui::IsItemHovered() && !addresses.empty()) {
            ImGui::SetTooltip("ROM address: %06X", addresses.front());
        }

        if (newValue != raw) {
            session.WriteRomAll(addresses, 2, newValue);
            state.levelRenderer.Invalidate();
        }

        ImGui::PopID();
        ImGui::EndDisabled();
    }

    static void DrawBitfieldByteProperty(EditorState& state, const char* label, const std::vector<unsigned>& addresses, bool enabled = true)
    {
        RomSession& session = state.session;
        const bool canEdit = session.IsLoaded() && enabled && !addresses.empty() && addresses.front() != 0;
        const unsigned raw = canEdit ? session.ReadRom(addresses.front(), 1) & 0xFFu : 0;
        unsigned newValue = raw;

        ImGui::BeginDisabled(!canEdit);
        ImGui::PushID(label);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        ImGui::TextDisabled("%u (0x%02X)", raw, raw);

        for (unsigned bit = 0; bit < 8; ++bit) {
            if (bit != 0) {
                ImGui::SameLine();
            }

            bool enabledBit = (raw & (1u << bit)) != 0;
            ImGui::PushID(static_cast<int>(bit));
            if (ImGui::Checkbox("##bit", &enabledBit)) {
                if (enabledBit) {
                    newValue |= 1u << bit;
                } else {
                    newValue &= ~(1u << bit);
                }
            }
            ImGui::SameLine(0.0f, 2.0f);
            ImGui::Text("%u", bit);
            ImGui::PopID();
        }

        if (ImGui::IsItemHovered() && !addresses.empty()) {
            if (addresses.size() == 1) {
                ImGui::SetTooltip("ROM address: %06X", addresses.front());
            } else {
                ImGui::SetTooltip("Writes %zu mirrored ROM addresses", addresses.size());
            }
        }

        if (newValue != raw) {
            session.WriteRomAll(addresses, 1, newValue);
            state.levelRenderer.Invalidate();
        }

        ImGui::PopID();
        ImGui::EndDisabled();
    }

    static void DrawFlaggedWordProperty(EditorState& state, const char* label, const char* flagLabel, const std::vector<unsigned>& addresses, unsigned flagMask, bool enabled = true)
    {
        RomSession& session = state.session;
        const bool canEdit = session.IsLoaded() && enabled && !addresses.empty() && addresses.front() != 0;
        const unsigned raw = canEdit ? session.ReadRom(addresses.front(), 2) : 0;
        int value = static_cast<int>(raw & ~flagMask & 0xFFFFu);
        bool flag = (raw & flagMask) != 0;
    
        ImGui::BeginDisabled(!canEdit);
        BeginPropertyRow(label, 88.0f);
        bool changed = false;
        if (ImGui::InputInt("##value", &value, 1, 4, ImGuiInputTextFlags_AutoSelectAll)) {
            changed = true;
        }
        if (ImGui::IsItemHovered() && !addresses.empty()) {
            ImGui::SetTooltip("ROM address: %06X, raw: %u", addresses.front(), raw & 0xFFFFu);
        }
        EndPropertyRow();
    
        BeginPropertyRow(flagLabel, 88.0f);
        if (ImGui::Checkbox("##flag", &flag)) {
            changed = true;
        }
        EndPropertyRow();
    
        if (changed) {
            const unsigned lowMask = (~flagMask) & 0xFFFFu;
            const unsigned newValue = (static_cast<unsigned>(value) & lowMask) | (flag ? flagMask : 0u);
            session.WriteRomAll(addresses, 2, newValue);
            state.levelRenderer.Invalidate();
        }
        ImGui::EndDisabled();
    }

    static void DrawInvertedNumberProperty(EditorState& state, const char* label, const std::vector<unsigned>& addresses, bool enabled = true)
    {
        RomSession& session = state.session;
        const bool canEdit = session.IsLoaded() && enabled && !addresses.empty() && addresses.front() != 0;
        int value = canEdit ? static_cast<int>((0xFFFFu - session.ReadRom(addresses.front(), 2)) & 0xFFFFu) : 0;
    
        ImGui::BeginDisabled(!canEdit);
        BeginPropertyRow(label, 88.0f);
       
        if (ImGui::InputInt("##value", &value, 1, 4, ImGuiInputTextFlags_AutoSelectAll)) {         
            session.WriteRomAll(addresses, 2, (0xFFFFu - static_cast<unsigned>(value)) & 0xFFFFu);
            state.levelRenderer.Invalidate();
        }
        if (ImGui::IsItemHovered() && !addresses.empty()) {
            if (addresses.size() == 1) {
                ImGui::SetTooltip("ROM address: %06X, stored as FFFF - value", addresses.front());
            } else {
                ImGui::SetTooltip("Writes %zu mirrored ROM addresses as FFFF - value", addresses.size());
            }
        }
        EndPropertyRow();
        ImGui::EndDisabled();
    }

    struct MovementProperty {
        const char* name;
        std::vector<unsigned> pixelAddresses;
        unsigned subpixelAddress;
        bool inverted;
        bool hasPixels;
    };

    static bool DrawEventNumberField(const char* label, unsigned& value, int byteCount)
    {
        int editValue = static_cast<int>(value);
        const float valueWidth = byteCount == 1 ? 64.0f : 88.0f;
        BeginPropertyRow(label, valueWidth);
        const bool changed = ImGui::InputInt("##value", &editValue, 1, 10, ImGuiInputTextFlags_AutoSelectAll);
        if (changed) {
            const unsigned mask = byteCount == 1 ? 0xFFu : 0xFFFFu;
            value = static_cast<unsigned>(editValue) & mask;
        }
        EndPropertyRow();
        return changed;
    }

    static EventInfo* SelectedEvent(EditorState& state)
    {
        if (!state.session.IsLoaded()) {
            return nullptr;
        }
    
        SC4Core::EventList& events = state.session.Core().eventTable;
        if (state.selectedEventIndex < 0 || state.selectedEventIndex >= static_cast<int>(events.size())) {
            state.selectedEventIndex = -1;
            return nullptr;
        }
    
        auto iter = events.begin();
        std::advance(iter, state.selectedEventIndex);
        return &*iter;
    }

    static int FindMatchingEventIndex(const SC4Core::EventList& events, const EventInfo& target)
    {
        int index = 0;
        int foundIndex = -1;
        for (const EventInfo& event : events) {
            if (event.match == target.match
                && event.type == target.type
                && event.xpos == target.xpos
                && event.ypos == target.ypos
                && event.eventId == target.eventId
                && event.eventSubId == target.eventSubId
                && event.spawnIndex == target.spawnIndex
                && event.unknown == target.unknown) {
                foundIndex = index;
            }
            ++index;
        }
        return foundIndex;
    }

    static void DrawComboProperty(EditorState& state, const char* label, int& value, const std::vector<std::string>& items, int byteCount, const std::vector<unsigned>& addresses, bool enabled = true)
    {
        RomSession& session = state.session;
        const bool canEdit = session.IsLoaded() && enabled && !addresses.empty() && addresses.front() != 0;
        if (canEdit) {
            value = static_cast<int>(session.ReadRom(addresses.front(), byteCount));
            if (value < 0 || value >= static_cast<int>(items.size())) {
                value = 0;
            }
        }
    
        ImGui::BeginDisabled(!canEdit);
        if (ComboRow(label, value, items)) {
            session.WriteRomAll(addresses, byteCount, static_cast<unsigned>(value));
            state.levelRenderer.Invalidate();
        }
        ImGui::EndDisabled();
    }

    static unsigned LevelAddress(unsigned base, const EditorState& state, unsigned stride = 1)
    {
        return base + static_cast<unsigned>(state.level) * stride;
    }

    static const char* EnemyName(SC4Core& core, unsigned id)
    {
        EventInfo event = {};
        event.type = EVENT_TYPE_ENEMY;
        event.eventId = static_cast<WORD>(id);
        return EventDisplayName(core, event);
    }

    static bool EnemyIdAlreadyListed(EditorState& state, unsigned enemyListAddress, unsigned enemyCount, unsigned id)
    {
        for (unsigned i = 0; i < enemyCount; ++i) {
            if (state.session.ReadRom(enemyListAddress + 1 + i, 1) == (id & 0xFFu)) {
                return true;
            }
        }
        return false;
    }

    static unsigned FindFreeBank86Address(EditorState& state, unsigned bytesNeeded)
    {
        SC4Core& core = state.session.Core();
        if (!core.rom || bytesNeeded == 0) {
            return 0;
        }
    
        const unsigned bankStart = SNESCore::snes2pc(0x860000);
        const unsigned bankEnd = SNESCore::snes2pc(0x870000);
        if (bankStart >= core.romSize || bankEnd > core.romSize || bankEnd <= bankStart || bytesNeeded > bankEnd - bankStart) {
            return 0;
        }
    
        for (unsigned pc = bankStart; pc + bytesNeeded <= bankEnd; ++pc) {
            bool freeRun = true;
            for (unsigned i = 0; i < bytesNeeded; ++i) {
                if (core.rom[pc + i] != 0xFF) {
                    freeRun = false;
                    pc += i;
                    break;
                }
            }
            if (freeRun) {
                return SNESCore::pc2snes(pc);
            }
        }
    
        return 0;
    }

    static bool AddEnemyToCurrentSet(EditorState& state, unsigned enemyListAddress, unsigned enemyCount, unsigned id)
    {
        const unsigned gfxPointerTableAddress = 0x868B45 + static_cast<unsigned>(state.level) * 2u;
        const unsigned gfxSetOffset = state.session.ReadRom(gfxPointerTableAddress, 2);
        const unsigned gfxListAddress = 0x860000 + gfxSetOffset;
        const unsigned gfxMode = state.session.ReadRom(gfxListAddress, 2);
        if (gfxMode != 0) {
            return false;
        }
    
        unsigned gfxEntryCount = 0;
        unsigned gfxReadAddress = gfxListAddress + 2;
        while (state.session.ReadRom(gfxReadAddress, 2) != 0xFFFF) {
            ++gfxEntryCount;
            gfxReadAddress += 5;
            if (gfxEntryCount > 0x40) {
                return false;
            }
        }
        const unsigned oldGfxListBytes = 2 + gfxEntryCount * 5 + 2;
        const unsigned newGfxListBytes = oldGfxListBytes + 5;
    
        unsigned slotNum = 0;
        for (unsigned i = 0; i < enemyCount; ++i) {
            const unsigned index = state.session.ReadRom(enemyListAddress + 1 + i, 1);
            slotNum += state.session.ReadRom(0x81AA80 + index, 1);
        }
    
        const unsigned spriteCount = state.session.ReadRom(0x81AA80 + (id & 0x7Fu), 1);
        const unsigned spriteDest = 0x6A00 + slotNum * 0x200;
        if (spriteCount == 0 || spriteDest + spriteCount * 0x200 > 0x8000) {
            return false;
        }
    
        const unsigned sourceLow = state.session.ReadRom(0x81A900 + (id & 0x7Fu) * 3u, 2);
        const unsigned sourceBank = state.session.ReadRom(0x81A900 + (id & 0x7Fu) * 3u + 2u, 1);
        const unsigned sourceAddress = sourceLow | (sourceBank << 16);
        if ((sourceAddress >> 16) == 0 || sourceLow == 0) {
            return false;
        }
    
        const unsigned newCount = enemyCount + 1;
        const unsigned newIdListBytes = newCount + 1;
        const unsigned newDataAddress = FindFreeBank86Address(state, newIdListBytes + newGfxListBytes);
        if ((newDataAddress >> 16) != 0x86) {
            return false;
        }
        const unsigned newListAddress = newDataAddress;
        const unsigned newGfxListAddress = newDataAddress + newIdListBytes;
    
        PushUndo(state);
        state.session.WriteRom(newListAddress, 1, newCount);
        for (unsigned i = 0; i < enemyCount; ++i) {
            state.session.WriteRom(newListAddress + 1 + i, 1, state.session.ReadRom(enemyListAddress + 1 + i, 1));
        }
        state.session.WriteRom(newListAddress + 1 + enemyCount, 1, id & 0xFFu);
        state.session.WriteRom(0x868BCD + static_cast<unsigned>(state.level) * 2u, 2, newListAddress & 0xFFFFu);
    
        for (unsigned i = 0; i < oldGfxListBytes - 2; ++i) {
            state.session.WriteRom(newGfxListAddress + i, 1, state.session.ReadRom(gfxListAddress + i, 1));
        }
        unsigned writeAddress = newGfxListAddress + oldGfxListBytes - 2;
        state.session.WriteRom(writeAddress, 2, spriteDest);
        state.session.WriteRom(writeAddress + 2, 2, sourceLow);
        state.session.WriteRom(writeAddress + 4, 1, sourceBank);
        state.session.WriteRom(writeAddress + 5, 2, 0xFFFF);
        state.session.WriteRom(gfxPointerTableAddress, 2, newGfxListAddress & 0xFFFFu);
    
        state.session.LoadCurrentLayer(state.showBackground);
        state.levelRenderer.Invalidate();
        return true;
    }

    static bool RemoveEnemyFromCurrentSet(EditorState& state, unsigned enemyListAddress, unsigned enemyCount, unsigned removeIndex)
    {
        if (enemyCount == 0 || removeIndex >= enemyCount) {
            return false;
        }
    
        const unsigned gfxSetOffset = state.session.ReadRom(0x868B45 + static_cast<unsigned>(state.level) * 2u, 2);
        const unsigned gfxListAddress = 0x860000 + gfxSetOffset;
        if (state.session.ReadRom(gfxListAddress, 2) != 0) {
            return false;
        }
    
        unsigned gfxEntryCount = 0;
        unsigned gfxReadAddress = gfxListAddress + 2;
        while (state.session.ReadRom(gfxReadAddress, 2) != 0xFFFF) {
            ++gfxEntryCount;
            gfxReadAddress += 5;
            if (gfxEntryCount > 0x40) {
                return false;
            }
        }
        if (removeIndex >= gfxEntryCount) {
            return false;
        }
    
        PushUndo(state);
        for (unsigned i = removeIndex; i + 1 < enemyCount; ++i) {
            state.session.WriteRom(enemyListAddress + 1 + i, 1, state.session.ReadRom(enemyListAddress + 2 + i, 1));
        }
        state.session.WriteRom(enemyListAddress, 1, enemyCount - 1);
        state.session.WriteRom(enemyListAddress + enemyCount, 1, 0xFF);
    
        const unsigned removedGfxAddress = gfxListAddress + 2 + removeIndex * 5;
        for (unsigned i = removeIndex; i + 1 < gfxEntryCount; ++i) {
            const unsigned src = gfxListAddress + 2 + (i + 1) * 5;
            const unsigned dst = gfxListAddress + 2 + i * 5;
            for (unsigned byte = 0; byte < 5; ++byte) {
                state.session.WriteRom(dst + byte, 1, state.session.ReadRom(src + byte, 1));
            }
        }
        const unsigned terminatorAddress = gfxListAddress + 2 + (gfxEntryCount - 1) * 5;
        state.session.WriteRom(terminatorAddress, 2, 0xFFFF);
        for (unsigned byte = 2; byte < 5; ++byte) {
            state.session.WriteRom(terminatorAddress + byte, 1, 0xFF);
        }
        (void)removedGfxAddress;
    
        state.session.LoadCurrentLayer(state.showBackground);
        state.levelRenderer.Invalidate();
        return true;
    }


    static void DrawCurrentLevelEnemies(EditorState& state)
    {
        if (!state.session.IsLoaded()) {
            return;
        }
    
        SC4Core& core = state.session.Core();
        const unsigned pointerTableAddress = LevelAddress(0x868BCD, state, 2);
        const unsigned enemySetOffset = state.session.ReadRom(pointerTableAddress, 2);
        const unsigned enemyListAddress = 0x860000 + enemySetOffset;
        unsigned enemyCount = state.session.ReadRom(enemyListAddress, 1);
        if (enemyCount > 0x40) {
            enemyCount = 0x40;
        }
    
        ImGui::Separator();
        ImGui::TextUnformatted("Enemie set for current level");
        ImGui::TextDisabled("Set ID pointer: $%06X -> $%06X", pointerTableAddress, enemyListAddress);
    
        if (enemyCount == 0) {
            ImGui::TextDisabled("No enemy IDs in this set.");
        } else if (ImGui::BeginTable("current-level-enemies", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 52.0f);
            ImGui::TableSetupColumn("Enemy");
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 64.0f);
            for (unsigned i = 0; i < enemyCount; ++i) {
                const unsigned id = state.session.ReadRom(enemyListAddress + 1 + i, 1);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("$%02X", id & 0xFF);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(EnemyName(core, id));
                ImGui::TableNextColumn();
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::SmallButton("Remove")) {
                    g_propertyState.enemyAddStatus = RemoveEnemyFromCurrentSet(state, enemyListAddress, enemyCount, i)
                        ? "Removed enemy availability and graphics load entry."
                        : "Could not remove enemy from this packed list.";
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    
        std::set<WORD> compatibleIds;
        core.GetActiveEnemyId(compatibleIds);
        ImGui::TextDisabled("%u listed, %d compatible via shared graphics", enemyCount, static_cast<int>(compatibleIds.size()));
    
        g_propertyState.enemyToAdd &= 0x7F;
        const unsigned addId = static_cast<unsigned>(g_propertyState.enemyToAdd);
        ImGui::SetNextItemWidth(72.0f);
        ImGui::InputInt("Enemy ID", &g_propertyState.enemyToAdd, 1, 16, ImGuiInputTextFlags_AutoSelectAll);
        g_propertyState.enemyToAdd &= 0x7F;
        const bool alreadyListed = EnemyIdAlreadyListed(state, enemyListAddress, enemyCount, addId);
        const bool compatible = compatibleIds.count(static_cast<WORD>(addId)) != 0;
        ImGui::BeginDisabled(alreadyListed);
        if (ImGui::Button("Add Available Enemy")) {
            g_propertyState.enemyAddStatus = AddEnemyToCurrentSet(state, enemyListAddress, enemyCount, addId)
                ? "Added enemy availability and graphics load entry."
                : "Could not add: no free bank $86 space, invalid graphics pointer, or no sprite VRAM slot room.";
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled("%s%s",
            EnemyName(core, addId),
            compatible ? " - compatible graphics" : " - new graphics entry will be added");
        if (alreadyListed) {
            ImGui::TextDisabled("This ID is in the current graphics assignment list.");
        } else {
            ImGui::TextDisabled("Copies and repoints both enemy ID and enemy graphics lists for this level.");
        }
        if (!g_propertyState.enemyAddStatus.empty()) {
            ImGui::TextDisabled("%s", g_propertyState.enemyAddStatus.c_str());
        }
    }

    static void DrawGeneralProperties(EditorState& state)
    {
        if (ImGui::CollapsingHeader("Global", ImGuiTreeNodeFlags_DefaultOpen)) {      
            DrawNumberProperty(state, "Lives", 2, { 0x8094DB });
            DrawNumberProperty(state, "Continue lives", 2, { 0x8CFD9B });
    
        
        }
    }

    //struct KnifePickupModeLocation {

    static void DrawSelectedEventProperties(EditorState& state)
    {
        SC4Core& core = state.session.Core();
        EventInfo* event = SelectedEvent(state);   
        
        if (event != nullptr && event->eventId != 0) {   // Can we read from event pointer. Else it could crash the editor.
        
            if (ImGui::CollapsingHeader("Selected Event", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (!event) {
                    ImGui::TextDisabled("Click an event marker in the level view.");
                    return;
                }
    
                ImGui::Text("%s", EventDisplayName(core, *event));
    
                if (ImGui::Button("Sort Events")) {
                    PushUndo(state);
                    const EventInfo selectedCopy = *event;
                    state.session.SortEvents();
                    state.selectedEventIndex = FindMatchingEventIndex(core.eventTable, selectedCopy);
                    state.levelRenderer.Invalidate();
                    return;
                }
               ImGui::SameLine();
               
               bool disabled = true;  // This probably just confuses and I never used it. 
               ImGui::BeginDisabled(disabled);
               if (ImGui::Button("Slot Events")) {
                   PushUndo(state);
                   const EventInfo selectedCopy = *event;
                   state.session.SlotEvents();
                   state.selectedEventIndex = FindMatchingEventIndex(core.eventTable, selectedCopy);
                   state.levelRenderer.Invalidate();
                   return;
                }
               ImGui::EndDisabled();
               
                ImGui::Separator();
    
                bool changed = false;
                const EventInfo beforeEdit = *event;
                int type = static_cast<int>(event->type);
                const std::vector<std::string> eventTypes = { "respawn", "Candle", "persist", "Unused" };
                changed |= ComboRow("Type", type, eventTypes);
                if (type < 0) {
                    type = 0;
                } else if (type > 3) {
                    type = 3;
                }
                
                event->type = (static_cast<BYTE>(type) & 0x03u);
                unsigned value = (event->xpos) & 0x3FFCu;                     // FIXME shows xpos properly.. we need to fix where it renders! May be it fixes it here too
                if (DrawEventNumberField("X", value, 2)) {
                    event->xpos = static_cast<WORD>(value);
                    if (type == 1) {
                        event->eventSubId = static_cast<WORD>((event->eventSubId & 0xF0u) | ((event->xpos >> 12) & 0x0Fu));
                    }
                    changed = true;
                }
                value = (event->ypos) & 0x3FFCu;
                if (DrawEventNumberField("Y", value, 2)) {
                    event->ypos = static_cast<WORD>(value);
                    if (type == 1) {
                        event->eventSubId = static_cast<WORD>((event->eventSubId & 0x0Fu) | ((event->ypos >> 8) & 0xF0u));
                    }
                    changed = true;
                }
                value = (event->eventId) & 0x00FFu;
                if (DrawEventNumberField("ID", value, 2)) {
                    event->eventId = static_cast<WORD>(value);
                    changed = true;
                }
                value = (event->eventSubId) & 0x00FFu;  
                    if (type == 1) {
                        if (DrawEventNumberField("Mask", value, 2)) {
                        event->eventSubId = static_cast<WORD>(value);
                        //    event->xpos = static_cast<WORD>((event->xpos & 0xC003u) | ((event->eventSubId & 0x0Fu) << 12)); FIXME I may have fixed syncing evnt bits encoding in a better way..
                        //    event->ypos = static_cast<WORD>((event->ypos & 0xC003u) | ((event->eventSubId & 0xF0u) << 8));
                        changed = true;
                        }
                        //      value = (event->eventSubId) & 0x00FFu;       //
                        //      ImGui::TextDisabled("HEX %X (0x4 = Quest, 0x8 = Background, 0xC both)", value);
                    }
                    else if (type != 1) {
                        if (DrawEventNumberField("SubID", value, 2)) {       
                        event->eventSubId = static_cast<WORD>(value);
                        changed = true;
                        }
                      //  ImGui::SetCursorPosX( + 20); // infoTEXT
                      //  ImGui::TextDisabled("24 to 36 Drops ItemID");
                    }
                
                value = (event->unknown) & 0x0003u;                          // will always be 3 never changes.. probably breaks things. 
                if (DrawEventNumberField("Unknown", value, 2)) {
                    event->unknown = static_cast<WORD>(value);
                    changed = true;
                }
                
                value = (event->match) & 0x00FFu;        
                if (type == 0) {        
                    if (DrawEventNumberField("Mask", value, 2)) {
                    event->match = static_cast<WORD>(value);
                    changed = true;                  
                    }
                    // value = (event->match) & 0x00FFu;                    FIXME make a working bit field. 
                    // DrawBitfieldByteProperty(state, "donno, donno, Quest, Background", {value & 0x00FF});
                    value = (event->match) & 0x00FFu;
                 //   ImGui::SetCursorPosX( + 20);  // infoTEXT
                 //   ImGui::TextDisabled("0x%X (0x4 = Quest, 0x8 = Background, 0xC both)", value);
                }
                else if (type >= 1) {                                       // candles and respawning events use index table at WRAM 0x1500
                    if (DrawEventNumberField("Spawn mask", value, 2)) {
                    event->match = static_cast<WORD>(value);
                    changed = true;
                    }
                }
                
                if (ImGui::CollapsingHeader("HEX", ImGuiTreeNodeFlags_OpenOnArrow)) {
                                                  
                    //ImGui::SetCursorPosX(+20);
                    value = (event->eventId) & 0x00FFu;
                    ImGui::TextDisabled("   ID $%X", value);
                    ImGui::SameLine(100.0f,0.0f);
                    value = (event->xpos) & 0x3FFCu;                                     
                    ImGui::TextDisabled("xPos $%X", value);
                    
                    //ImGui::SetCursorPosX(+20);
                    value = (event->eventSubId) & 0x00FFu;
                    ImGui::TextDisabled("  SubID $%X", value);
                    ImGui::SameLine(100.0f,0.0f);
                    value = (event->ypos) & 0x3FFCu;
                    ImGui::TextDisabled("yPos $%X", value);
                }
               
                ImGui::Separator();
                ImGui::TextDisabled("  Event %d of %d", state.selectedEventIndex + 1, static_cast<int>(core.eventTable.size()));
                if (ImGui::Button("Prev Event")) {
                    const int total = static_cast<int>(core.eventTable.size());
                    if (total > 0) {
                        if (state.selectedEventIndex < 0) {
                            state.selectedEventIndex = 0;                
                        }
                        else {
                            state.selectedEventIndex = (state.selectedEventIndex - 1) % total;
                        }
                        state.levelRenderer.Invalidate();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Next Event")) {
                    const int total = static_cast<int>(core.eventTable.size());
                    if (total > 0) {
                        if (state.selectedEventIndex < 0) {
                            state.selectedEventIndex = 0;
                            state.selectedEventIndex >= int(core.eventTable.size());
                        }
                        else {
                            state.selectedEventIndex = (state.selectedEventIndex + 1) % total;
                        }
                        state.levelRenderer.Invalidate();
                    }
                }
                ImGui::Separator();

                if (changed) {
                    const EventInfo afterEdit = *event;
                    *event = beforeEdit;
                    PushUndo(state);
                    *event = afterEdit;
                    state.session.SaveEvents();
                    state.levelRenderer.Invalidate();
                }        
            
            }
            
            if (event->eventId == 0x3) {
                
                
                    if (event->eventSubId == 0x1) {
                        DrawNumberPropertySliderHex(state, "P1 xPos   ", 2, { RING_CONVEYOR_X + event->eventSubId * 4}, 0x0, 0x07FF, true);
                        DrawNumberPropertySliderHex(state, "P1 yPos   ", 2, { RING_CONVEYOR_Y + event->eventSubId * 4}, 0x0, 0x07FF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81fcf2 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81fcf4 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81fcf6 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81fcf8 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81fcfa }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81fcfc }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81fcfe }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81fd00 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81fd02 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81fd04 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P3 Timer  ", 2, { 0x81fd06 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpdSub", 2, { 0x81fd08 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpd   ", 2, { 0x81fd0a }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpdSub", 2, { 0x81fd0c }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpd   ", 2, { 0x81fd0e }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P4 Timer  ", 2, { 0x81fd10 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpdSub", 2, { 0x81fd12 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpd   ", 2, { 0x81fd14 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpdSub", 2, { 0x81fd16 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpd   ", 2, { 0x81fd18 }, 0x0, 0xFFFF, true);
                    }

                    if (event->eventSubId == 0x2) {
                        DrawNumberPropertySliderHex(state, "P1 xPos   ", 2, { RING_CONVEYOR_X + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        DrawNumberPropertySliderHex(state, "P1 yPos   ", 2, { RING_CONVEYOR_Y + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81FD1C }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81FD1C }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81FD1E }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81FD20 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81FD22 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81FD24 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81FD26 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81FD28 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81FD2A }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81FD2C }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P3 Timer  ", 2, { 0x81FD2E }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpdSub", 2, { 0x81FD30 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpd   ", 2, { 0x81FD32 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpdSub", 2, { 0x81FD34 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpd   ", 2, { 0x81FD36 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P4 Timer  ", 2, { 0x81FD38 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpdSub", 2, { 0x81FD3A }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpd   ", 2, { 0x81FD3C }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpdSub", 2, { 0x81FD3E }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpd   ", 2, { 0x81FD40 }, 0x0, 0xFFFF, true);
                    }                                                               

                    if (event->eventSubId == 0x3) {
                        DrawNumberPropertySliderHex(state, "P1 xPos   ", 2, { RING_CONVEYOR_X + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        DrawNumberPropertySliderHex(state, "P1 yPos   ", 2, { RING_CONVEYOR_Y + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81FD50 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81FD52 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81FD54 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81FD56 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81FD58 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81FD5A }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81FD5C }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81FD5E }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81FD60 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81FD62 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P3 Timer  ", 2, { 0x81FD64 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpdSub", 2, { 0x81FD66 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpd   ", 2, { 0x81FD68 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpdSub", 2, { 0x81FD6A }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpd   ", 2, { 0x81FD6C }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P4 Timer  ", 2, { 0x81FD6E }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpdSub", 2, { 0x81FD70 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpd   ", 2, { 0x81FD72 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpdSub", 2, { 0x81FD74 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpd   ", 2, { 0x81FD76 }, 0x0, 0xFFFF, true);
                    }

                    if (event->eventSubId == 0x4) {
                        DrawNumberPropertySliderHex(state, "P1 xPos   ", 2, { RING_CONVEYOR_X + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        DrawNumberPropertySliderHex(state, "P1 yPos   ", 2, { RING_CONVEYOR_Y + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81FD7A }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81FD7C }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81FD7E }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81FD80 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81FD82 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81FD84 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81FD86 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81FD88 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81FD8A }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81FD8C }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P3 Timer  ", 2, { 0x81FD8E }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpdSub", 2, { 0x81FD90 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpd   ", 2, { 0x81FD92 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpdSub", 2, { 0x81FD94 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpd   ", 2, { 0x81FD96 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P4 Timer  ", 2, { 0x81FD98 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpdSub", 2, { 0x81FD9A }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpd   ", 2, { 0x81FD9C }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpdSub", 2, { 0x81FD9E }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpd   ", 2, { 0x81FDA0 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P5 Timer  ", 2, { 0x81FDA2 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 xSpdSub", 2, { 0x81FDA4 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 xSpd   ", 2, { 0x81FDA6 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 ySpdSub", 2, { 0x81FDA8 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 ySpd   ", 2, { 0x81FDAA }, 0x0, 0xFFFF, true);
                    }

                    if (event->eventSubId == 0x5) {
                        DrawNumberPropertySliderHex(state, "P1 xPos   ", 2, { RING_CONVEYOR_X + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        DrawNumberPropertySliderHex(state, "P1 yPos   ", 2, { RING_CONVEYOR_Y + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81FDAE }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81FDB0 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81FDB2 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81FDB4 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81FDB6 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81FDB8 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81FDBA }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81FDBC }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81FDBE }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81FDC0 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P3 Timer  ", 2, { 0x81FDC2 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpdSub", 2, { 0x81FDC4 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpd   ", 2, { 0x81FDC6 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpdSub", 2, { 0x81FDC8 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpd   ", 2, { 0x81FDCA }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P4 Timer  ", 2, { 0x81FDCC }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpdSub", 2, { 0x81FDCE }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpd   ", 2, { 0x81FDD0 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpdSub", 2, { 0x81FDD2 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpd   ", 2, { 0x81FDD4 }, 0x0, 0xFFFF, true);
                    }

                    if (event->eventSubId == 0x6) {
                        DrawNumberPropertySliderHex(state, "P1 xPos   ", 2, { RING_CONVEYOR_X + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        DrawNumberPropertySliderHex(state, "P1 yPos   ", 2, { RING_CONVEYOR_Y + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81FDD8 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81FDDA }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81FDDC }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81FDDE }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81FDE0 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81FDE2 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81FDE4 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81FDE6 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81FDE8 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81FDEA }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P3 Timer  ", 2, { 0x81FDEC }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpdSub", 2, { 0x81FDEE }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpd   ", 2, { 0x81FDF0 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpdSub", 2, { 0x81FDF2 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpd   ", 2, { 0x81FDF4 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P4 Timer  ", 2, { 0x81FDF6 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpdSub", 2, { 0x81FDF8 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpd   ", 2, { 0x81FDFA }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpdSub", 2, { 0x81FDFC }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpd   ", 2, { 0x81FDFE }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P5 Timer  ", 2, { 0x81FC00 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 xSpdSub", 2, { 0x81FC02 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 xSpd   ", 2, { 0x81FC06 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 ySpdSub", 2, { 0x81FC08 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 ySpd   ", 2, { 0x81FC0A }, 0x0, 0xFFFF, true);
                    }

                    if (event->eventSubId == 0x7) {
                        DrawNumberPropertySliderHex(state, "P1 xPos   ", 2, { RING_CONVEYOR_X + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        DrawNumberPropertySliderHex(state, "P1 yPos   ", 2, { RING_CONVEYOR_Y + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81FE0C }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81FE0E }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81FE10 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81FE12 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81FE14 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81FE16 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81FE18 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81FE1A }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81FE1C }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81FE1E }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P3 Timer  ", 2, { 0x81FE20 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpdSub", 2, { 0x81FE22 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpd   ", 2, { 0x81FE24 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpdSub", 2, { 0x81FE26 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpd   ", 2, { 0x81FE28 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P4 Timer  ", 2, { 0x81FE2A }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpdSub", 2, { 0x81FE2C }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpd   ", 2, { 0x81FE2E }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpdSub", 2, { 0x81FE30 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpd   ", 2, { 0x81FE32 }, 0x0, 0xFFFF, true);
                    }                                                             
            
                    if (event->eventSubId == 0x8) {
                        DrawNumberPropertySliderHex(state, "P1 xPos   ", 2, { RING_CONVEYOR_X + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        DrawNumberPropertySliderHex(state, "P1 yPos   ", 2, { RING_CONVEYOR_Y + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81FE36 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81FE38 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81FE3A }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81FE3C }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81FE3E }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81FE40 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81FE42 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81FE44 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81FE46 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81FE48 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P3 Timer  ", 2, { 0x81FE4A }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpdSub", 2, { 0x81FE4C }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpd   ", 2, { 0x81FE4E }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpdSub", 2, { 0x81FE50 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpd   ", 2, { 0x81FE52 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P4 Timer  ", 2, { 0x81FE54 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpdSub", 2, { 0x81FE56 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpd   ", 2, { 0x81FE58 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpdSub", 2, { 0x81FE5A }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpd   ", 2, { 0x81FE5C }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P5 Timer  ", 2, { 0x81FE5E }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 xSpdSub", 2, { 0x81FE60 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 xSpd   ", 2, { 0x81FE62 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 ySpdSub", 2, { 0x81FE64 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 ySpd   ", 2, { 0x81FE66 }, 0x0, 0xFFFF, true);
                    }                                                               
                  
                    if (event->eventSubId == 0x9) {
                        DrawNumberPropertySliderHex(state, "P1 xPos   ", 2, { RING_CONVEYOR_X + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        DrawNumberPropertySliderHex(state, "P1 yPos   ", 2, { RING_CONVEYOR_Y + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81FE6A }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81FE6C }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81FE6E }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81FE70 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81FE72 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81FE74 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81FE76 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81FE78 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81FE7A }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81FE7C }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P3 Timer  ", 2, { 0x81FE7E }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpdSub", 2, { 0x81FE80 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpd   ", 2, { 0x81FE82 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpdSub", 2, { 0x81FE84 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpd   ", 2, { 0x81FE86 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P4 Timer  ", 2, { 0x81FE88 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpdSub", 2, { 0x81FE8A }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpd   ", 2, { 0x81FE8C }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpdSub", 2, { 0x81FE8E }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpd   ", 2, { 0x81FE90 }, 0x0, 0xFFFF, true);
                    }

                    if (event->eventSubId == 0xA) {
                        DrawNumberPropertySliderHex(state, "P1 xPos   ", 2, { RING_CONVEYOR_X + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        DrawNumberPropertySliderHex(state, "P1 yPos   ", 2, { RING_CONVEYOR_Y + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81FE94 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81FE96 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81FE98 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81FE9A }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81FE9C }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81FE9E }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81FEA0 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81FEA2 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81FEA4 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81FEA6 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P3 Timer  ", 2, { 0x81FEA8 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpdSub", 2, { 0x81FEAA }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpd   ", 2, { 0x81FEAC }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpdSub", 2, { 0x81FEAE }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpd   ", 2, { 0x81FEB0 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P4 Timer  ", 2, { 0x81FEB2 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpdSub", 2, { 0x81FEB4 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpd   ", 2, { 0x81FEB6 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpdSub", 2, { 0x81FEB8 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpd   ", 2, { 0x81FEBA }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P5 Timer  ", 2, { 0x81FEBC }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 xSpdSub", 2, { 0x81FEBE }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 xSpd   ", 2, { 0x81FEC0 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 ySpdSub", 2, { 0x81FEC2 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 ySpd   ", 2, { 0x81FEC4 }, 0x0, 0xFFFF, true);
                    }                                                              

                    if (event->eventSubId == 0xB) {
                        DrawNumberPropertySliderHex(state, "P1 xPos   ", 2, { RING_CONVEYOR_X + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        DrawNumberPropertySliderHex(state, "P1 yPos   ", 2, { RING_CONVEYOR_Y + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81FEC8 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81FECA }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81FECC }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81FECE }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81FED0 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81FED2 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81FED4 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81FED6 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81FED8 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81FEDA }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P3 Timer  ", 2, { 0x81FEDC }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpdSub", 2, { 0x81FEDE }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpd   ", 2, { 0x81FEE0 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpdSub", 2, { 0x81FEE2 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpd   ", 2, { 0x81FEE4 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P4 Timer  ", 2, { 0x81FEE6 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpdSub", 2, { 0x81FEE8 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpd   ", 2, { 0x81FEEA }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpdSub", 2, { 0x81FEEC }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpd   ", 2, { 0x81FEEE }, 0x0, 0xFFFF, true);
                    }

                    if (event->eventSubId == 0xC) {
                        DrawNumberPropertySliderHex(state, "P1 xPos   ", 2, { RING_CONVEYOR_X + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        DrawNumberPropertySliderHex(state, "P1 yPos   ", 2, { RING_CONVEYOR_Y + event->eventSubId * 4 }, 0x0, 0x07FF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81FEF2 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81FEF4 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81FEF6 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81FEF8 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81FEFA }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81FEFC }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81FEFE }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81FF00 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81FF02 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81FF04 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P3 Timer  ", 2, { 0x81FF06 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpdSub", 2, { 0x81FF08 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 xSpd   ", 2, { 0x81FF0A }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpdSub", 2, { 0x81FF0C }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P3 ySpd   ", 2, { 0x81FF0E }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P4 Timer  ", 2, { 0x81FF10 }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpdSub", 2, { 0x81FF12 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 xSpd   ", 2, { 0x81FF14 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpdSub", 2, { 0x81FF16 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P4 ySpd   ", 2, { 0x81FF18 }, 0x0, 0xFFFF, true);
                        ImGui::Separator();
                        DrawNumberPropertySliderHex(state, "P5 Timer  ", 2, { 0x81FF1A }, 0x0, 0xFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 xSpdSub", 2, { 0x81FF1C }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 xSpd   ", 2, { 0x81FF1E }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 ySpdSub", 2, { 0x81FF20 }, 0x0, 0xFFFF, true);
                        DrawNumberPropertySliderHex(state, "P5 ySpd   ", 2, { 0x81FF22 }, 0x0, 0xFFFF, true);
                    }                                                             
            }

            if (event->eventId == 0x17) {

                if (event->eventSubId == 0x0) {
                    DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81c1d3 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81c1d5 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81c1d7 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81c1d9 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81c1db }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81c1dd }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81c1df }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81c1e1 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81c1e3 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81c1e5 }, 0x0, 0xFFFF, true);
                }

                if (event->eventSubId == 0x1) {
                    DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81c1e9 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81c1eb }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81c1ed }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81c1ef }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81c1f1 }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81c1f3 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81c1f5 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81c1f7 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81c1f9 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81c1fb }, 0x0, 0xFFFF, true);
                }

                if (event->eventSubId == 0x2) {
                    DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81c1ff }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81c201 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81c203 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81c205 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81c207 }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81c209 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81c20b }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81c20d }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81c20f }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81c211 }, 0x0, 0xFFFF, true);
                }

                if (event->eventSubId == 0x3) {
                    DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81c215 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81c217 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81c219 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81c21b }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81c21d }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81c21f }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81c221 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81c223 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81c225 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81c227 }, 0x0, 0xFFFF, true);
                }

                if (event->eventSubId == 0x4) {
                    DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81c22b }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81c22d }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81c22f }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81c231 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81c233 }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81c235 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81c237 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81c239 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81c23b }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81c23d }, 0x0, 0xFFFF, true);
                }

                if (event->eventSubId == 0x5) {
                    DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81c241 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81c243 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81c245 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81c247 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81c249 }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81c24b }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81c24d }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81c24f }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81c251 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81c253 }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P3 Timer  ", 2, { 0x81c255 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P3 xSpdSub", 2, { 0x81c257 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P3 xSpd   ", 2, { 0x81c259 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P3 ySpdSub", 2, { 0x81c25b }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P3 ySpd   ", 2, { 0x81c25d }, 0x0, 0xFFFF, true);
                }

                if (event->eventSubId == 0x6) {
                    DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81c261 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81c263 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81c265 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81c267 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81c269 }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81c26b }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81c26d }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81c26f }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81c271 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81c273 }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P3 Timer  ", 2, { 0x81c275 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P3 xSpdSub", 2, { 0x81c277 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P3 xSpd   ", 2, { 0x81c279 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P3 ySpdSub", 2, { 0x81c27b }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P3 ySpd   ", 2, { 0x81c27d }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P4 Timer  ", 2, { 0x81c27f }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P4 xSpdSub", 2, { 0x81c281 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P4 xSpd   ", 2, { 0x81c283 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P4 ySpdSub", 2, { 0x81c285 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P4 ySpd   ", 2, { 0x81c287 }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P5 FFFF ends table here", 2, { 0x81c289 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P5 xSpdSub", 2, { 0x81c28b }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P5 xSpd   ", 2, { 0x81c28d }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P5 ySpdSub", 2, { 0x81c28f }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P5 ySpd   ", 2, { 0x81c291 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P6 Timer  ", 2, { 0x81c293 }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P6 THE END!", 2, { 0x81c295 }, 0x0, 0xFFFF, true);
                }

                if (event->eventSubId == 0x7) {
                    DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81c297 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81c299 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81c29b }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81c29d }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81c29f }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81c2a1 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81c2a3 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81c2a5 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81c2a7 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81c2a9 }, 0x0, 0xFFFF, true);
                }

                if (event->eventSubId == 0x8) {
                    DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81c2ad }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81c2ae }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81c2b1 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81c2b3 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81c2b5 }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81c2b7 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81c2b9 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81c2bb }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81c2bd }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81c2bf }, 0x0, 0xFFFF, true);
                }

                if (event->eventSubId == 0x9) {
                    DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81c2c3 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81c2c5 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81c2c7 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81c2c9 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81c2cb }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81c2cd }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81c2cf }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81c2d1 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81c2d3 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81c2d5 }, 0x0, 0xFFFF, true);
                }
            }

            if (event->eventId == 0x2F) {
                if (ImGui::CollapsingHeader("Choose Droped Item ID", ImGuiTreeNodeFlags_DefaultOpen)) {
                   DrawNumberProperty(state, "Breakable Wall Item", 1, { EVENT_BREAKABLE_WALL_ITEM_BASE + ((event->eventSubId) & 0x0F) });
                    
                }
            }
            
            if (event->eventId == 0x62) {
                if (event->eventSubId == 0x0) {
                    DrawNumberPropertySliderHex(state, "P1 Timer", 2,   { 0x81fc1c }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81fc1e }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd", 2,    { 0x81fc20 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81fc22 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd", 2,    { 0x81fc24 }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer", 2,   { 0x81fc26 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81fc28 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd", 2,    { 0x81fc2a }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81fc2c }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd", 2,    { 0x81fc2e }, 0x0, 0xFFFF, true);
                }

                if (event->eventSubId == 0x1) {
                    DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81fc32 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81fc34 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81fc36 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81fc38 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81fc3a }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81fc3c }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81fc3e }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81fc40 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81fc42 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81fc44 }, 0x0, 0xFFFF, true);
                }

                if (event->eventSubId == 0x2) {
                    DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81fc48 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81fc4a }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81fc4c }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81fc4e }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81fc50 }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81fc52 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81fc54 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81fc56 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81fc58 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81fc5A }, 0x0, 0xFFFF, true);
                }

                if (event->eventSubId == 0x3) {
                    DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81fc5e }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81fc60 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81fc62 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81fc64 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81fc66 }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81fc68 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81fc6a }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81fc6c }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81fc6e }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81fc70 }, 0x0, 0xFFFF, true);
                }
                
                if (event->eventSubId == 0x4) {
                    DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81fc74 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81fc76 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81fc78 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81fc7a }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81fc7c }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81fc7e }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81fc80 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81fc82 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81fc84 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81fc86 }, 0x0, 0xFFFF, true);
                }

                if (event->eventSubId == 0x5) {
                    DrawNumberPropertySliderHex(state, "P1 Timer  ", 2, { 0x81fc8a }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpdSub", 2, { 0x81fc8c }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 xSpd   ", 2, { 0x81fc8e }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpdSub", 2, { 0x81fc90 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P1 ySpd   ", 2, { 0x81fc92 }, 0x0, 0xFFFF, true);
                    ImGui::Separator();
                    DrawNumberPropertySliderHex(state, "P2 Timer  ", 2, { 0x81fc94 }, 0x0, 0xFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpdSub", 2, { 0x81fc96 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 xSpd   ", 2, { 0x81fc98 }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpdSub", 2, { 0x81fc9a }, 0x0, 0xFFFF, true);
                    DrawNumberPropertySliderHex(state, "P2 ySpd   ", 2, { 0x81fc9c }, 0x0, 0xFFFF, true);
                }
                


            }
            
            const bool expanded = state.session.IsExpandedRom();
            if (!expanded) {
                ImGui::TextDisabled("These settings are available for expanded ROMs.");
            }
            ImGui::BeginDisabled(!expanded);

            if (event->eventId == 0x15) {
                if (ImGui::CollapsingHeader("Exit, Level Transition Editor", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static const std::vector<std::string> entrances = { "0", "1", "2", "3", "4", "5", "6", "7" };
                    static const std::vector<std::string> exitTypes = { "Init (DONT USE)", "Stairs Up", "Stairs Down", "Left", "Right" };
                    static const std::vector<std::string> exitChecks = NumberItems(0x40);

                    // Set exit entery to the subID that is selected.
                    g_propertyState.exitCheck = (static_cast<int>(event->eventSubId) & 0x3F);
                    bool disabled = true;
                    ImGui::BeginDisabled(disabled);
                    ComboRow("Exit SubID", g_propertyState.exitCheck, exitChecks);
                    const unsigned exitBase = EXP_EV15_Exit + 0x40 * 0x4 * static_cast<unsigned>(state.level) + 0x4 * static_cast<unsigned>(g_propertyState.exitCheck);
                    ImGui::EndDisabled();

                    DrawComboProperty(state, "Exit type", g_propertyState.exitType, exitTypes, 1, { exitBase + 0x0 }, expanded);   // DrawNumberProperty(state, "Exit type value", 1, { exitBase + 0x0 }, expanded);                  
                    DrawNumberProperty(state, "Exit cmp value X or Y", 2, { exitBase + 0x2 }, expanded);
                    DrawNumberPropertySlider(state, "Transition Selector", 1, { exitBase + 0x1 }, 0x0, 0x7, expanded); 
                    const int transitionSelector = static_cast<int>(state.session.ReadRom(exitBase + 0x1, 1));                    // cast selection for the table to edit.  
                    g_propertyState.nextLevelDirection = std::clamp(transitionSelector, 0, static_cast<int>(entrances.size()) - 1);
                    
                    ImGui::Separator();     // ImGui::TextDisabled("Level Transit Editor");  Not needed info for end users. 
                    ImGui::BeginDisabled(disabled);
                    ComboRow("Transition", g_propertyState.nextLevelDirection, entrances);
                    ImGui::EndDisabled();
                    
                    const unsigned transitionBase = EXP_LEVEL_TRANSIT + 0x10 * static_cast<unsigned>(state.level) + 0x2 * static_cast<unsigned>(g_propertyState.nextLevelDirection);
                    DrawNumberProperty(state, "Next Level", 1, { transitionBase + 0x0 }, expanded);              
                    //DrawNumberPropertyCombo(state, "Next Checkpoint", 1, { transitionBase + 0x1 }, 0x0, 0x7, expanded);
                    DrawNumberPropertySlider(state, "Next Checkpoint", 1, { transitionBase + 0x1 },0x0 ,0x7 , expanded);                     
                    
                }
            }

            if (event->eventId == 0x41) {
                if (ImGui::CollapsingHeader("Camlock Editor", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static const std::vector<std::string> cameraLocks = NumberItems(0x20);
                    g_propertyState.cameraLock = (static_cast<int>(event->eventSubId) & 0x1F);
                  //static const std::vector<std::string> directionValues = { "1",              "2", };
                  // static const std::vector<std::string> directionNames = { "Down Right", "Up Left"};
                  // dirAddress 55a,55e    cmpAddress 54a,54e     dirAddressFreeCam 12a6,12a8     cmpAddressFreeCam 1298,129a  X,Y
                  // static const std::vector<std::string> directionAddress = { "0xA0", "0xA2","0xA4", "0xA6" };
                  // static const std::vector<std::string> directionLabels = { "Left", "Right","Bottom", "Top" };
                    bool disabled = true;
                    ImGui::BeginDisabled(disabled);
                    ComboRow("Camera lock", g_propertyState.cameraLock, cameraLocks);
                    ImGui::EndDisabled();
                    
                    const unsigned lockBase = 0xA58000 + 0xC * 0x20 * static_cast<unsigned>(state.level) + 0xC * static_cast<unsigned>(g_propertyState.cameraLock);
                    DrawNumberProperty(state, "Lock direction", 2, { lockBase + 0x0 }, expanded);
                    DrawNumberProperty(state, "Lock dir addr", 2, { lockBase + 0x2 }, expanded);
                    DrawNumberProperty(state, "Lock cmp addr", 2, { lockBase + 0x4 }, expanded);
                    DrawNumberProperty(state, "Lock cmp value", 2, { lockBase + 0x6 }, expanded);
                    DrawNumberProperty(state, "Lock store value", 2, { lockBase + 0x8 }, expanded);
                    DrawNumberProperty(state, "Lock store addr", 2, { lockBase + 0xA }, expanded);
                }
            }
			
            
            ImGui::EndDisabled(); // end expanded ROM check

            if (ImGui::CollapsingHeader("Event Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
    
                DrawNumberProperty(state, "Hitbox X", 1, { EVENT_HITBOX_BASE + (event->eventId) * 2 });
                DrawNumberProperty(state, "Hitbox Y", 1, { EVENT_HITBOX_BASE + 1 + (event->eventId) * 2 });
                DrawNumberProperty(state, "Health", 2, { EVENT_HEALTH_BASE + (event->eventId) * 2 });
                DrawNumberProperty(state, "Damage", 1, { EVENT_DAMAGE_BASE + (event->eventId) });
   
                ImGui::TextDisabled("Some events overwrite there attributes in ther code.");
                DrawBitfieldByteProperty(state, "hurt whip subW pick ?? mask rosry noDesp", { EVENT_HIT_ATTRIBUTE_BASE + (event->eventId) * 2 });
                ImGui::Separator();
			//	ImGui::TextDisabled("Edit with cosion game might crash!");
                DrawNumberProperty(state, "!Slot Size", 1, { EVENT_SLOT_SIZE + (event->eventId) });
                DrawNumberProperty(state, "!Death spawnID (Flame)", 1, { EVENT_DEATH_ANIMATION_BASE + (event->eventId) }); 
                DrawNumberProperty(state, "!Death Movement Bits", 1, { EVENT_DEATH_MOVBITS_BASE + (event->eventId) });
                ImGui::Separator();

            }
        }
    }


    static void DrawPlayerProperties(EditorState& state)
    {
        if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen)) {
            ComboRow("Whip", g_propertyState.whip, { "Leather", "Chain0", "Chain1" });
            const unsigned whip = static_cast<unsigned>(g_propertyState.whip);
            DrawNumberProperty(state, "Whip length", 2, { 0x819261 + 2 * whip });
            DrawNumberProperty(state, "Whip full damage", 2, { 0x81A6EC + 4 * whip });
            DrawNumberProperty(state, "Whip partial damage", 2, { 0x81A6EC + 4 * whip + 2 });
    
            ComboRow("Subweapon", g_propertyState.subweapon, { "Knife", "Axe", "Holy Water", "Cross" });
            DrawNumberProperty(state, "Subweapon damage", 2, { SUBWEAPON_DAMAGE_BASE + 2 * (static_cast<unsigned>(g_propertyState.subweapon) + 1) });
    //        DrawKnifePlatformPickupProperty(state);   // function is moved to ..bkp/trash.txt and the top line is dublicated and documented out here
    //        DrawAxeBlockBreakerProperty(state);
    
            static const std::vector<MovementProperty> movements = {
                { "Walking Right", { 0x80A665 }, 0x80A65F, false, true },
                { "Walking Left", { 0x80A67E }, 0x80A678, true, true },
                { "Jumping Right", { 0x80A90B, 0x80A910 }, 0x80A916, false, true },
                { "Jumping Left", { 0x80A93C, 0x80A941 }, 0x80A947, true, true },
                { "Crouching Right", { 0x80A705 }, 0x80A6FF, false, true },
                { "Crouching Left", { 0x80A716 }, 0x80A710, true, true },
                { "Climbing Right", { 0x80A8C8 }, 0x80A8C2, false, true },
                { "Climbing Left", { 0x80A8A6 }, 0x80A8A0, true, true },
                { "Gravity", {}, 0x80A73E, false, false },
            };
            std::vector<std::string> movementNames;
            movementNames.reserve(movements.size());
            for (const MovementProperty& movement : movements) {
                movementNames.emplace_back(movement.name);
            }
            ComboRow("Movement", g_propertyState.movement, movementNames);
            if (g_propertyState.movement < 0 || g_propertyState.movement >= static_cast<int>(movements.size())) {
                g_propertyState.movement = 0;
            }
            const MovementProperty& movement = movements[static_cast<size_t>(g_propertyState.movement)];
            if (movement.inverted) {
                DrawInvertedNumberProperty(state, "Move pixels", movement.pixelAddresses, movement.hasPixels);
                DrawInvertedNumberProperty(state, "Move subpixels", { movement.subpixelAddress });
            } else {
                DrawNumberProperty(state, "Move pixels", 2, movement.pixelAddresses, movement.hasPixels);
                DrawNumberProperty(state, "Move subpixels", 2, { movement.subpixelAddress });
            }
        }
    }

    static void DrawLevelProperties(EditorState& state)
    {
        if (ImGui::CollapsingHeader("Level", ImGuiTreeNodeFlags_DefaultOpen)) {
            
           //static const std::vector<std::string> directionName = { "Right", "Left", "Down", "Up" };
           // int timerValue = SNESCore::snes2pc(0x85BCF8) * state.level + 2;
           // ImGui::Text("Timer %x", (timerValue) & 0x0FFFu);

            DrawNumberProperty(state, "Music", 1, { LevelAddress(LEVEL_MUSIK, state) });
            DrawNumberProperty(state, "Continue level", 1, { LevelAddress(LEVEL_CONTINUE, state) });
            DrawNumberProperty(state, "Level Type", 2, { LevelAddress(LEVEL_TYPE, state, 2) });   // FIXME ComboBox describe pluse level reload! Check if loadMOD7 rooms break rom!
            DrawNumberProperty(state, "Timer", 2, { LevelAddress(LEVEL_TIMER, state, 2) });       // FIXME This is already decimal in the rom 
            DrawNumberProperty(state, "Enemy Damage Buff", 1, { LevelAddress(LEVEL_DAMAGE_BUFF, state, 1) });            
            ImGui::Separator();
            DrawNumberProperty(state, "Layer Transperent Mask", 2, { LevelAddress(LEVEL_BG_PROPERTY_MASK_BASE, state, 2) });
            DrawFlaggedWordProperty(state, "Layer Scroll Modes", "Layer behavior flag", { LevelAddress(LEVEL_BG_SCROLL_BASE, state, 2) }, 0x8000);
            //DrawNumberProperty(state, "Event direction", 1, { LevelAddress(LEVEL_LOAD_DIRECTION, state) });       
            DrawNumberPropertyCombo(state, "Event load direction 0 Right | 1 Left | 2 Down | 3 Up", 1, { LevelAddress(LEVEL_LOAD_DIRECTION, state) }, 0x0, 0x3, true);
            ImGui::Separator();
            DrawNumberProperty(state, "!Always 3 cam behavior", 2, { LevelAddress(LEVEL_ALWAYS_3SCRL, state,2) });
            

            //reused or not properly implemented stuff..
           //const unsigned deathBase = state.session.Region() == 0 ? 0x81B395 : 0x81B369;
           //DrawNumberProperty(state, "Death level", 1, { LevelAddress(deathBase, state) }); // unexpanded death level??
           //DrawNumberProperty(state, "BG animation 0", 2, { LevelAddress(0x85CA82, state, 2) });
           //DrawNumberProperty(state, "BG animation 1", 2, { LevelAddress(0x85CB0A, state, 2) });
           //DrawNumberProperty(state, "Palette animation", 2, { LevelAddress(0x86946F, state, 2) });
           //DrawNumberProperty(state, "Enemy set ID", 2, { LevelAddress(0x868BCD, state, 2) });
           //DrawNumberProperty(state, "Enemy set", 2, { LevelAddress(0x868B45, state, 2) });
            
            DrawCurrentLevelEnemies(state);
        }
    }

    static void DrawExpandedProperties(EditorState& state)
    {
        const bool expanded = state.session.IsExpandedRom();
        if (ImGui::CollapsingHeader("Expanded ROM", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!expanded) {
                ImGui::TextDisabled("These settings are available for expanded ROMs.");
            }
    
            static const std::vector<std::string> entrances = { "0", "1", "2", "3", "4", "5", "6", "7" };
            ImGui::BeginDisabled(!expanded);
            ImGui::Separator();
            ImGui::TextDisabled("Entrance property for each checkpoint");
            ImGui::Separator();
    
            ComboRow("Checkpoint", g_propertyState.checkpoint, entrances);
            state.checkpoint = g_propertyState.checkpoint;  // also update current checkpoint view 
                        
            const unsigned entranceBase = 0xA78000 + 0x100 * static_cast<unsigned>(state.level) + 0x20 * static_cast<unsigned>(g_propertyState.checkpoint);
            DrawNumberProperty(state, "Death level", 1, { entranceBase + 0x1 }, expanded);
            DrawNumberProperty(state, "State0", 1, { entranceBase + 0x0 }, expanded);
            DrawNumberProperty(state, "State1", 2, { entranceBase + 0xE }, expanded);
            DrawNumberProperty(state, "X pos", 2, { entranceBase + 0x2 }, expanded);
            DrawNumberProperty(state, "Y pos", 2, { entranceBase + 0x4 }, expanded);
            ImGui::Separator();        
            //DrawNumberProperty(state, "Border left", 2, { entranceBase + 0x12 }, expanded);
            //DrawNumberProperty(state, "Border right", 2, { entranceBase + 0x14 }, expanded);
            //DrawNumberProperty(state, "Border top", 2, { entranceBase + 0x16 }, expanded);
            //DrawNumberProperty(state, "Border bottom", 2, { entranceBase + 0x18 }, expanded);
            DrawNumberPropertySlider(state, "Border left", 2, { entranceBase + 0x12 }, 0x0 ,0xE00,expanded);
            DrawNumberPropertySlider(state, "Border right", 2, { entranceBase + 0x14 }, 0x0, 0xE00, expanded);
            DrawNumberPropertySlider(state, "Border top", 2, { entranceBase + 0x16 }, 0x0, 0xE00, expanded);
            DrawNumberPropertySlider(state, "Border bottom", 2, { entranceBase + 0x18 }, 0x0, 0xE00, expanded);

            ImGui::Separator();
            DrawNumberProperty(state, "Cam0 X", 2, { entranceBase + 0x6 }, expanded);
            DrawNumberProperty(state, "Cam0 Y", 2, { entranceBase + 0x8 }, expanded);
            DrawNumberProperty(state, "Cam1 X", 2, { entranceBase + 0xA }, expanded);
            DrawNumberProperty(state, "Cam1 Y", 2, { entranceBase + 0xC }, expanded);
            ImGui::Separator();
            DrawNumberProperty(state, "Camera speed X", 2, { entranceBase + 0x1A }, expanded);
            DrawNumberProperty(state, "Camera speed Y", 2, { entranceBase + 0x1C }, expanded);
            ImGui::Separator();
            DrawNumberProperty(state, "Camera pointer", 2, { entranceBase + 0x1E }, expanded);
            ImGui::Separator();
    
            ImGui::Spacing();
            ImGui::Spacing();
            
            ImGui::EndDisabled();
        
        }
    }

} // namespace



void DrawGlobalPropertiesTab(EditorState& state)
{
    if (!state.session.IsLoaded()) {
        ImGui::TextUnformatted("Open a ROM to edit global properties.");
        return;
    }

    ImGui::BeginChild("global-properties-scroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    DrawGeneralProperties(state);
    DrawPlayerProperties(state);
    ImGui::EndChild();
}

void DrawLevelPropertiesTab(EditorState& state)
{
    if (!state.session.IsLoaded()) {
        ImGui::TextUnformatted("Open a ROM to edit level properties.");
        return;
    }

    ImGui::BeginChild("level-properties-scroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    DrawLevelProperties(state);
    DrawExpandedProperties(state);
    ImGui::EndChild();
}

void DrawSelectionTab(EditorState& state)
{
    if (!state.session.IsLoaded()) {
        ImGui::TextUnformatted("Open a ROM to edit selected items.");
        return;
    }

    ImGui::BeginChild("selection-scroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    DrawSelectedEventProperties(state);
    ImGui::EndChild();
}
