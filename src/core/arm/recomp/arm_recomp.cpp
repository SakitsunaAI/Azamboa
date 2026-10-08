// Copyright 2014-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma GCC optimize("O3,unroll-loops,omit-frame-pointer,inline-functions,fast-math")

#include <dlfcn.h>
#include <string>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/arm/recomp/arm_recomp.h"
#include "core/arm/exclusive_monitor.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/svc.h"
#include "core/memory.h"
#include "core/loader/loader.h"

namespace Core {

ARM_Recomp::ARM_Recomp(Core::System& system_, Memory::MemorySystem& memory_, u32 core_id_,
                       std::shared_ptr<Core::Timing::Timer> timer_)
    : ARM_Interface(core_id_, timer_), system(system_), memory(memory_) {
    fallback = std::make_unique<ARM_DynCom>(system, memory, USER32MODE, core_id_, timer_);
    SetPageTable(memory.GetCurrentPageTable());
    InitContext();
    LoadLibrary();
}

ARM_Recomp::~ARM_Recomp() {
    if (lib_handle) {
        dlclose(lib_handle);
    }
}

void ARM_Recomp::InitContext() {
    host_vtable.read8 = &ARM_Recomp::HostRead8;
    host_vtable.read16 = &ARM_Recomp::HostRead16;
    host_vtable.read32 = &ARM_Recomp::HostRead32;
    host_vtable.write8 = &ARM_Recomp::HostWrite8;
    host_vtable.write16 = &ARM_Recomp::HostWrite16;
    host_vtable.write32 = &ARM_Recomp::HostWrite32;
    host_vtable.interpret = &ARM_Recomp::HostInterpret;
    host_vtable.lookup = &ARM_Recomp::HostLookup;

    ctx.host = &host_vtable;
    ctx.user = this;
    ctx.vfp = vfp_regs;
    ctx.fpscr = &fpscr;
}

void ARM_Recomp::LoadLibrary() {
    std::string lib_path = "recomp.so";
#ifdef __APPLE__
    lib_path = "librecomp.dylib";
#elif defined(_WIN32)
    lib_path = "recomp.dll";
#endif

    lib_handle = dlopen(lib_path.c_str(), RTLD_NOW);
    if (lib_handle) {
        origins = reinterpret_cast<Origins*>(dlsym(lib_handle, "origins"));
        LOG_INFO(Core_ARM11, "Loaded recomp library: {}", lib_path);
    } else {
        LOG_WARNING(Core_ARM11, "Failed to load recomp library {}: {}", lib_path, dlerror());
    }
}

void ARM_Recomp::CopyToContext() {
    for (int i = 0; i < 15; i++) {
        ctx.r[i] = fallback->GetReg(i);
    }
    ctx.r[15] = fallback->GetPC();

    u32 cpsr = fallback->GetCPSR();
    ctx.n = (cpsr >> 31) & 1;
    ctx.z = (cpsr >> 30) & 1;
    ctx.c = (cpsr >> 29) & 1;
    ctx.v = (cpsr >> 28) & 1;
    ctx.q = (cpsr >> 27) & 1;
    ctx.ge = (cpsr >> 16) & 0xF;
    ctx.thumb = (cpsr >> 5) & 1;

    for (int i = 0; i < 64; i++) {
        ctx.vfp[i] = fallback->GetVFPReg(i);
    }
    ctx.fpscr[0] = fallback->GetVFPSystemReg(VFP_FPSCR);
    ctx.tls = cp15_thread_uro;

    if (current_page_table) {
        ctx.read_pages = const_cast<uint8_t**>(reinterpret_cast<const uint8_t* const*>(current_page_table->GetPointerArray().data()));
        ctx.write_pages = const_cast<uint8_t**>(reinterpret_cast<const uint8_t* const*>(current_page_table->GetPointerArray().data()));
    } else {
        ctx.read_pages = nullptr;
        ctx.write_pages = nullptr;
    }
}

void ARM_Recomp::CopyFromContext() {
    for (int i = 0; i < 15; i++) {
        fallback->SetReg(i, ctx.r[i]);
    }
    fallback->SetPC(ctx.r[15]);

    u32 cpsr = fallback->GetCPSR();
    cpsr = (cpsr & ~0xF80F0020) | 
           (ctx.n << 31) | 
           (ctx.z << 30) | 
           (ctx.c << 29) | 
           (ctx.v << 28) | 
           (ctx.q << 27) | 
           (ctx.ge << 16) | 
           (ctx.thumb << 5);
    fallback->SetCPSR(cpsr);

    for (int i = 0; i < 64; i++) {
        fallback->SetVFPReg(i, ctx.vfp[i]);
    }
    fallback->SetVFPSystemReg(VFP_FPSCR, ctx.fpscr[0]);
}

void ARM_Recomp::Run() {
    if (break_flag) [[unlikely]] {
        return;
    }

    if (!lib_handle || !origins) {
        fallback->Run();
        return;
    }

    CopyToContext();

    while (true) {
        ctx.budget = GetTimer().GetDowncount();
        if (ctx.budget <= 0) {
            break;
        }

        Code code = HostLookup(&ctx, ctx.r[15]);
        if (code) {
            ctx.exit = EXIT_NONE;
            code(&ctx);

            if (ctx.exit == EXIT_SVC) {
                CopyFromContext();
                Kernel::SVCContext svc_context(system);
                svc_context.CallSVC(ctx.svc);
                CopyToContext();
            } else if (ctx.exit == EXIT_BUDGET) {
                break;
            } else if (ctx.exit == EXIT_UNWIND) {
                // Return control
            }
        } else {
            CopyFromContext();
            fallback->Step();
            CopyToContext();
        }
        
        s64 executed = GetTimer().GetDowncount() - ctx.budget;
        GetTimer().AddTicks(executed > 0 ? executed : 1);
        
        if (break_flag || ctx.exit == EXIT_BUDGET || ctx.budget <= 0) {
            break;
        }
    }

    CopyFromContext();
}

void ARM_Recomp::Step() {
    if (break_flag) [[unlikely]] {
        return;
    }
    fallback->Step();
}

void ARM_Recomp::SetPC(u32 pc) {
    fallback->SetPC(pc);
}

u32 ARM_Recomp::GetPC() const {
    return fallback->GetPC();
}

u32 ARM_Recomp::GetReg(int index) const {
    return fallback->GetReg(index);
}

void ARM_Recomp::SetReg(int index, u32 value) {
    fallback->SetReg(index, value);
}

u32 ARM_Recomp::GetVFPReg(int index) const {
    return fallback->GetVFPReg(index);
}

void ARM_Recomp::SetVFPReg(int index, u32 value) {
    fallback->SetVFPReg(index, value);
}

u32 ARM_Recomp::GetVFPSystemReg(VFPSystemRegister reg) const {
    return fallback->GetVFPSystemReg(reg);
}

void ARM_Recomp::SetVFPSystemReg(VFPSystemRegister reg, u32 value) {
    fallback->SetVFPSystemReg(reg, value);
}

u32 ARM_Recomp::GetCPSR() const {
    return fallback->GetCPSR();
}

void ARM_Recomp::SetCPSR(u32 cpsr) {
    fallback->SetCPSR(cpsr);
}

u32 ARM_Recomp::GetCP15Register(CP15Register reg) const {
    switch (reg) {
    case CP15_THREAD_UPRW:
        return cp15_thread_uprw;
    case CP15_THREAD_URO:
        return cp15_thread_uro;
    default:
        UNREACHABLE_MSG("Unknown CP15 register: {}", reg);
    }
    return 0;
}

void ARM_Recomp::SetCP15Register(CP15Register reg, u32 value) {
    switch (reg) {
    case CP15_THREAD_UPRW:
        cp15_thread_uprw = value;
        return;
    case CP15_THREAD_URO:
        cp15_thread_uro = value;
        return;
    default:
        UNREACHABLE_MSG("Unknown CP15 register: {}", reg);
    }
}

void ARM_Recomp::SaveContext(ThreadContext& ctx) {
    fallback->SaveContext(ctx);
}

void ARM_Recomp::LoadContext(const ThreadContext& ctx) {
    fallback->LoadContext(ctx);
}

void ARM_Recomp::PrepareReschedule() {
    fallback->PrepareReschedule();
    ctx.budget = 0;
    ctx.exit = EXIT_BUDGET;
}

void ARM_Recomp::ClearInstructionCache() {
    fallback->ClearInstructionCache();
}

void ARM_Recomp::InvalidateCacheRange(u32 start_address, std::size_t length) {
    fallback->InvalidateCacheRange(start_address, length);
}

void ARM_Recomp::ClearExclusiveState() {
    fallback->ClearExclusiveState();
}

std::shared_ptr<Memory::PageTable> ARM_Recomp::GetPageTable() const {
    return current_page_table;
}

void ARM_Recomp::SetPageTable(const std::shared_ptr<Memory::PageTable>& page_table) {
    current_page_table = page_table;
    fallback->SetPageTable(page_table);
    if (current_page_table) {
        ctx.read_pages = const_cast<uint8_t**>(reinterpret_cast<const uint8_t* const*>(current_page_table->GetPointerArray().data()));
        ctx.write_pages = const_cast<uint8_t**>(reinterpret_cast<const uint8_t* const*>(current_page_table->GetPointerArray().data()));
    } else {
        ctx.read_pages = nullptr;
        ctx.write_pages = nullptr;
    }
}

uint8_t ARM_Recomp::HostRead8(Context* context, uint32_t address) {
    return static_cast<ARM_Recomp*>(context->user)->memory.Read8(address);
}

uint16_t ARM_Recomp::HostRead16(Context* context, uint32_t address) {
    return static_cast<ARM_Recomp*>(context->user)->memory.Read16(address);
}

uint32_t ARM_Recomp::HostRead32(Context* context, uint32_t address) {
    return static_cast<ARM_Recomp*>(context->user)->memory.Read32(address);
}

void ARM_Recomp::HostWrite8(Context* context, uint32_t address, uint8_t value) {
    static_cast<ARM_Recomp*>(context->user)->memory.Write8(address, value);
}

void ARM_Recomp::HostWrite16(Context* context, uint32_t address, uint16_t value) {
    static_cast<ARM_Recomp*>(context->user)->memory.Write16(address, value);
}

void ARM_Recomp::HostWrite32(Context* context, uint32_t address, uint32_t value) {
    static_cast<ARM_Recomp*>(context->user)->memory.Write32(address, value);
}

void ARM_Recomp::HostInterpret(Context* context, uint32_t address, uint32_t opcode) {
    ARM_Recomp* this_ptr = static_cast<ARM_Recomp*>(context->user);
    this_ptr->CopyFromContext();
    this_ptr->fallback->Step();
    this_ptr->CopyToContext();
}

Code ARM_Recomp::HostLookup(Context* context, uint32_t address) {
    ARM_Recomp* this_ptr = static_cast<ARM_Recomp*>(context->user);
    if (!this_ptr->origins) return nullptr;
    
    Origins* origins = this_ptr->origins;
    // Fast binary search on pieces
    int32_t low = 0;
    int32_t high = origins->piece_count - 1;
    
    while (low <= high) {
        int32_t mid = low + (high - low) / 2;
        const Piece& piece = origins->pieces[mid];
        
        if (address >= piece.start && address < piece.end) {
            // Found a piece that contains this address.
            // But we need to know if it's the start of a function.
            // For now, return nullptr to just fallback and do it properly later.
            // Zakuro has proper lookup logic. We can do a simpler one.
            break;
        } else if (address < piece.start) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    
    // We didn't fully implement HostLookup yet, this needs more logic to find the exact function origin
    // For now we will return nullptr which makes it fallback. 
    return nullptr;
}

} // namespace Core

