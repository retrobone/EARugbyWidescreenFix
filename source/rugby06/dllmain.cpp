// RUGBY 06 WIDESCREEN FIX

#include "stdafx.h"
#include <spdlog/spdlog.h>
#include "injector/injector.hpp"
#include "injector/assembly.hpp"
#include "injector/hooking.hpp"
#include "Hooking.Patterns.h"

using namespace injector;

// GLOBALS
float fNewHUDWidth = 853.33333f;
float fHUDOffsetX = -106.66666f;
float fNewAspect = 1.7777777f;

DWORD jmpBack_HUD_1 = 0;
DWORD jmpBack_HUD_2 = 0;

void __declspec(naked) Hook_HUD_Unified_1() {
    __asm {
        push 0x43F00000
        push[fNewHUDWidth]
        push 0
        push[fHUDOffsetX]
        jmp[jmpBack_HUD_1]
    }
}

void __declspec(naked) Hook_HUD_Unified_2() {
    __asm {
        push 0x43F00000
        push[fNewHUDWidth]
        push 0
        push[fHUDOffsetX]
        jmp[jmpBack_HUD_2]
    }
}

void ResHack() {
    spdlog::info("--- STARTING CRITICAL PATCHES ---");

    // DISABLE WHITELIST FOR RESOLUTIONS
    auto pattern_check = hook::pattern("55 8B EC 81 7D 08 80 02 00 00");
    if (!pattern_check.empty()) {
        uintptr_t addr = (uintptr_t)pattern_check.get_first(0);
        uint8_t patch[] = { 0xB0, 0x01, 0xC3 }; //
        injector::WriteMemoryRaw(addr, patch, sizeof(patch), true);
        spdlog::info("[OK] Whitelist Disabled.");
    }

    // BYPASS MAX RESOLUTION LIMIT
    auto pattern_loop = hook::pattern("C7 45 E4 40 06 00 00 C7 45 D8 B0 04 00 00");
    if (!pattern_loop.empty()) {

        uintptr_t max_width = (uintptr_t)pattern_loop.get_first(3);
        injector::WriteMemory(max_width, 65535, true);

        uintptr_t max_height = (uintptr_t)pattern_loop.get_first(10);
        injector::WriteMemory(max_height, 65535, true);
        spdlog::info("Maximum resolution limit removed");
    }
    else {
        spdlog::error("[FAIL] Width Limit pattern not found. 1080p will not show.");
    }

    // REMOVE BUCKET MATH
    auto pattern_math = hook::pattern("99 B9 C8 00 00 00 F7 F9 83 E8 03");
    if (!pattern_math.empty()) {
        uintptr_t match = (uintptr_t)pattern_math.get_first(0);

        bool foundJump = false;
        for (int i = 0; i < 40; i++) {
            uint8_t* p = (uint8_t*)(match + i);
            if (p[0] == 0x0F && p[1] == 0x85) {
                injector::MakeNOP(p, 6, true);
                spdlog::info("[OK] Bucket Math Check ABOLISHED at {:X}", (uintptr_t)p);
                foundJump = true;
                break;
            }
        }
        if (!foundJump) spdlog::error("[FAIL] Found Math, but missed the Jump.");
    }
    else {
        spdlog::error("[FAIL] Bucket Math pattern not found.");
    }

    // INCREASE RESOLUTION LIST LIMIT
    auto pattern_count = hook::pattern("83 F8 32 0F 8D");
    if (!pattern_count.empty()) {
        injector::WriteMemory<uint8_t>(pattern_count.get_first(2), 0x7F, true);
        spdlog::info("[OK] List Limit increased.");
    }
}

void Init()
{

    CIniReader iniReader("Rugby06WidescreenFix.ini");
    int ResX = iniReader.ReadInteger("MAIN", "ResX", 0);
    int ResY = iniReader.ReadInteger("MAIN", "ResY", 0);
    if (!ResX || !ResY) std::tie(ResX, ResY) = GetDesktopRes();

    float aspectRatio = (float)ResX / (float)ResY;

    ResHack();

    if (aspectRatio <= 1.334f) return;

    fNewHUDWidth = 480.0f * aspectRatio;
    fHUDOffsetX = (640.0f - fNewHUDWidth) / 2.0f;
    fNewAspect = aspectRatio;

    spdlog::info("Aspect: {:.4f} | HUD Width: {:.4f} | Offset: {:.4f}", fNewAspect, fNewHUDWidth, fHUDOffsetX);

    auto pattern_ar = hook::pattern("C7 41 0C AB AA AA 3F");
    if (!pattern_ar.empty()) {
        injector::WriteMemory(pattern_ar.get_first(3), fNewAspect, true);
        spdlog::info("AR Patched");
    }

    auto pattern_hud = hook::pattern("68 00 00 F0 43 68 00 00 20 44 6A 00 6A 00");
    if (pattern_hud.size() > 0) {
        uintptr_t addr1 = (uintptr_t)pattern_hud.get(0).get<void>(0);
        jmpBack_HUD_1 = addr1 + 14;
        injector::MakeJMP(addr1, Hook_HUD_Unified_1, true);
        injector::MakeNOP(addr1 + 5, 9, true);

        if (pattern_hud.size() > 1) {
            uintptr_t addr2 = (uintptr_t)pattern_hud.get(1).get<void>(0);
            if (addr2 != addr1) {
                jmpBack_HUD_2 = addr2 + 14;
                injector::MakeJMP(addr2, Hook_HUD_Unified_2, true);
                injector::MakeNOP(addr2 + 5, 9, true);
            }
        }
        spdlog::info("HUD Patched");
    }
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        Init();
    }
    return TRUE;
}
