// RUGBY 2005 WIDESCREEN FIX

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
int newSampleRate = 44100;

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
    auto pattern_check = hook::pattern("8b 44 24 04 3d 80 02 00 00");
    if (!pattern_check.empty()) {
        uintptr_t addr = (uintptr_t)pattern_check.get_first(0);
        uint8_t patch[] = { 0xB0, 0x01, 0xC3 }; //
        injector::WriteMemoryRaw(addr, patch, sizeof(patch), true);
        spdlog::info("[OK] Whitelist Disabled.");
    }
}

void Init()
{

    CIniReader iniReader("Rugby2005WidescreenFix.ini");
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

    auto pattern_ar = hook::pattern("C7 40 0C AB AA AA 3F");
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

    // FORCE HIGHER AUDIO SAMPLE RATE
    auto srate_addr = hook::pattern("b8 22 56 00 00");
    if (!srate_addr.empty()) {
        injector::WriteMemory<uintptr_t>(srate_addr.get_first(1), newSampleRate, true);
        spdlog::info("Forced higher sample rate");
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
