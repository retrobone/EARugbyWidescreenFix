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
DWORD jmpBack_Aspect = 0;


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

void Init()
{
    CIniReader iniReader("Rugby06WidescreenFix.ini");
    int ResX = iniReader.ReadInteger("MAIN", "ResX", 0);
    int ResY = iniReader.ReadInteger("MAIN", "ResY", 0);
    if (!ResX || !ResY) std::tie(ResX, ResY) = GetDesktopRes();

    float aspectRatio = (float)ResX / (float)ResY;
    if (aspectRatio <= 1.334f) return;

    fNewHUDWidth = 480.0f * aspectRatio;
    fHUDOffsetX = (640.0f - fNewHUDWidth) / 2.0f;
    fNewAspect = aspectRatio;

    spdlog::info("Resolution: {}x{} - Aspect: {:.4f}", ResX, ResY, fNewAspect);
    spdlog::info("HUD Width: {:.4f} - Offset: {:.4f}", fNewHUDWidth, fHUDOffsetX);

    //ASPECT RATIO
    auto pattern_ar = hook::pattern("C7 41 0C AB AA AA 3F");
    if (!pattern_ar.empty()) {
        uintptr_t addr = (uintptr_t)pattern_ar.get_first(0);

        injector::WriteMemory(addr + 3, fNewAspect, true);

        spdlog::info("Patched 3D Aspect Ratio (Immediate Value) at: {:X}", addr);
    }
    else {
        spdlog::error("Failed to find 3D Aspect Ratio pattern");
    }
	//HUD 
    auto pattern_hud = hook::pattern("68 00 00 F0 43 68 00 00 20 44 6A 00 6A 00");

    if (pattern_hud.size() > 0) {

        //Frontend
        uintptr_t addr1 = (uintptr_t)pattern_hud.get(0).get<void>(0);
        jmpBack_HUD_1 = addr1 + 14; // Skip 14 bytes
        injector::MakeJMP(addr1, Hook_HUD_Unified_1, true);
        injector::MakeNOP(addr1 + 5, 9, true);
        spdlog::info("Hooked HUD 1 at: {:X}", addr1);

        //In-Game
        if (pattern_hud.size() > 1) {
            uintptr_t addr2 = (uintptr_t)pattern_hud.get(1).get<void>(0);

            if (addr2 != addr1) {
                jmpBack_HUD_2 = addr2 + 14;
                injector::MakeJMP(addr2, Hook_HUD_Unified_2, true);
                injector::MakeNOP(addr2 + 5, 9, true);
                spdlog::info("Hooked HUD 2 at: {:X}", addr2);
            }
        }
    }
    else {
        spdlog::error("Failed to find HUD patterns (Check Hex string)");
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
