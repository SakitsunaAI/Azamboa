// Copyright 2014-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "common/common_types.h"
#include "core/arm/arm_interface.h"
#include "core/arm/dyncom/arm_dyncom.h"
#include "core/arm/recomp/recomp.h"

namespace Memory {
struct PageTable;
class MemorySystem;
} // namespace Memory

namespace Core {
class System;
class ExclusiveMonitor;

class ARM_Recomp final : public ARM_Interface {
public:
    explicit ARM_Recomp(Core::System& system_, Memory::MemorySystem& memory_, u32 core_id_,
                        std::shared_ptr<Core::Timing::Timer> timer_);
    ~ARM_Recomp() override;

    void Run() override;
    void Step() override;

    void SetPC(u32 pc) override;
    u32 GetPC() const override;
    u32 GetReg(int index) const override;
    void SetReg(int index, u32 value) override;
    u32 GetVFPReg(int index) const override;
    void SetVFPReg(int index, u32 value) override;
    u32 GetVFPSystemReg(VFPSystemRegister reg) const override;
    void SetVFPSystemReg(VFPSystemRegister reg, u32 value) override;
    u32 GetCPSR() const override;
    void SetCPSR(u32 cpsr) override;
    u32 GetCP15Register(CP15Register reg) const override;
    void SetCP15Register(CP15Register reg, u32 value) override;

    void SaveContext(ThreadContext& ctx) override;
    void LoadContext(const ThreadContext& ctx) override;

    void PrepareReschedule() override;

    void ClearInstructionCache() override;
    void InvalidateCacheRange(u32 start_address, std::size_t length) override;
    void ClearExclusiveState() override;
    void SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) override;

    bool HasSingleInstructionBreakAccuracy() override {
        return false;
    }

protected:
    std::shared_ptr<Memory::PageTable> GetPageTable() const override;

private:
    static uint8_t HostRead8(Context* ctx, uint32_t address);
    static uint16_t HostRead16(Context* ctx, uint32_t address);
    static uint32_t HostRead32(Context* ctx, uint32_t address);
    static void HostWrite8(Context* ctx, uint32_t address, uint8_t value);
    static void HostWrite16(Context* ctx, uint32_t address, uint16_t value);
    static void HostWrite32(Context* ctx, uint32_t address, uint32_t value);
    static void HostInterpret(Context* ctx, uint32_t address, uint32_t opcode);
    static Code HostLookup(Context* ctx, uint32_t address);

    void InitContext();
    void LoadLibrary();
    void CopyToContext();
    void CopyFromContext();

    Core::System& system;
    Memory::MemorySystem& memory;

    Context ctx{};
    Host host_vtable{};
    uint32_t vfp_regs[64]{};
    uint32_t fpscr{};
    
    std::shared_ptr<Memory::PageTable> current_page_table = nullptr;
    std::unique_ptr<ARM_DynCom> fallback;
    
    void* lib_handle = nullptr;
    Origins* origins = nullptr;
    
    // CP15 state
    u32 cp15_thread_uro = 0;
    u32 cp15_thread_uprw = 0;
};

} // namespace Core

