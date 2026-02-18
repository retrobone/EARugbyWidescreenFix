//  RUGBY 08 WIDESCREEN FIX

#include "stdafx.h"
#include <spdlog/spdlog.h>
#include <psapi.h>

#pragma comment(lib, "Psapi.lib")
#include "injector/injector.hpp"
#include "injector/assembly.hpp"
#include "injector/hooking.hpp"
#include "Hooking.Patterns.h"

using namespace injector;

//  GLOBALS
float fNewHUDWidth = 853.33333f;
float fHUDOffsetX = -106.66666f;
float fNewAspect = 1.7777777f;
int   iNewHUDWidthInt = 853;

// Mouse Limits
float fMouseLimitX = 853.33333f;
float fMouseLimitY = 480.0f;


DWORD jmpBack_2D_Unified = 0;

void __declspec(naked) Hook_HUD_Unified() {
    __asm {
        push[fNewHUDWidth]
        push 0     
        push[fHUDOffsetX]

        jmp[jmpBack_2D_Unified]
    }
}

void Init()
{
    CIniReader iniReader("Rugby08WidescreenFix.ini");
    int ResX = iniReader.ReadInteger("MAIN", "ResX", 0);
    int ResY = iniReader.ReadInteger("MAIN", "ResY", 0);

    // Auto-detect
    if (!ResX || !ResY)
        std::tie(ResX, ResY) = GetDesktopRes();



    // Calculate Aspect Ratio
    float aspectRatio = (float)ResX / (float)ResY;

    // Standard (4:3) Fallback
    if (aspectRatio <= 1.334f) {
        spdlog::info("Standard 4:3 detected. Skipping patches.");
        return;
    }

    // HUD
    fNewHUDWidth = 480.0f * aspectRatio;
    fHUDOffsetX = (640.0f - fNewHUDWidth) / 2.0f;
    fNewAspect = aspectRatio;
    iNewHUDWidthInt = (int)fNewHUDWidth;

    fMouseLimitX = fNewHUDWidth;
    fVideoQuadWidth = fNewHUDWidth;

    MODULEINFO modInfo = { 0 };
    GetModuleInformation(GetCurrentProcess(), GetModuleHandle(NULL), &modInfo, sizeof(MODULEINFO));
    uintptr_t start = (uintptr_t)modInfo.lpBaseOfDll;
    uintptr_t end = start + modInfo.SizeOfImage;

    spdlog::info("Widescreen Detected: {:.2f} ({}x{})", aspectRatio, ResX, ResY);

    // Aspect Ratio
    auto pattern_ar = hook::pattern("C7 41 0C AB AA AA 3F");
    if (!pattern_ar.empty()) {
        uintptr_t address_of_float = (uintptr_t)pattern_ar.get_first(3);
        injector::WriteMemory(address_of_float, fNewAspect, true);
        spdlog::info("Patched Stack Aspect Ratio at: {:X}", address_of_float);
    }
    else {
        spdlog::error("Failed to find 3D Aspect Ratio pattern");
    }

    // --- 2D Unified Hook ---
    auto pattern_hud = hook::pattern("68 00 00 F0 43 68 00 00 20 44 6A 00 6A 00");
    if (!pattern_hud.empty()) {
        uintptr_t matchAddr = (uintptr_t)pattern_hud.get_first(0);
        uintptr_t hookAddr = matchAddr + 5;

        jmpBack_2D_Unified = hookAddr + 9;

        injector::MakeJMP(hookAddr, Hook_HUD_Unified, true);
        injector::MakeNOP(hookAddr + 5, 4, true);

        spdlog::info("Hooked 2D HUD Logic at: {:X}", hookAddr);
    }
    else {
        spdlog::error("2D Hook Failed: Pattern not found.");
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
