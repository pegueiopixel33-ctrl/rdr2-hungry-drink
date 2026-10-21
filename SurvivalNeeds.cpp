// ============================================================
//  SurvivalNeeds - Fome e Sede para RDR2 (SINGLE PLAYER)
//  Requer: ScriptHookRDR2 (SDK: main.h, natives.h, nativeCaller.h)
//  NÃO use no Red Dead Online (risco de ban).
//
//  AVISO: os hashes de natives mudam entre versões do jogo.
//  Confira os marcados com [CONFIRMAR] em https://alloc8or.re/rdr3/nativedb/
// ============================================================
#include "main.h"      // do SDK do ScriptHookRDR2
#include <string>
#include <fstream>
#include <map>
#include <algorithm>

// ------------------ CONFIG (ajuste o balanceamento aqui) ------------------
struct Config {
    float hungerDrainPerMin = 1.6f;   // % por minuto parado  -> ~62 min até zerar
    float thirstDrainPerMin = 2.2f;   // % por minuto parado  -> ~45 min até zerar
    float runMultiplier     = 1.8f;   // correndo gasta mais
    float sprintMultiplier  = 2.6f;   // galopando/sprint gasta ainda mais
    float rideMultiplier    = 1.2f;   // a cavalo
    float starveHpPerSec    = 0.5f;   // dano por segundo com fome/sede em 0
    int   saveEverySec      = 30;
} CFG;

// ------------------ ITENS (nome do item -> efeito) ------------------
// Os nomes são hashados com joaat. [CONFIRMAR] os nomes exatos no
// banco de itens (ex.: consumable_*), cada versão pode variar.
struct Effect { float hunger; float thirst; };
static std::map<std::string, Effect> ITEMS = {
    // ---------- COMIDAS (fome, sede) ----------
    // Nomes candidatos: se algum nao existir no jogo, e ignorado sem problema.
    {"CONSUMABLE_BREAD",            { 18.f,  0.f}},
    {"CONSUMABLE_BISCUIT",          { 12.f,  0.f}},
    {"CONSUMABLE_APPLE",            {  8.f,  3.f}},
    {"CONSUMABLE_PEACH",            {  8.f,  4.f}},
    {"CONSUMABLE_PEAR",             {  8.f,  4.f}},
    {"CONSUMABLE_ORANGE",           {  8.f,  5.f}},
    {"CONSUMABLE_CORN",             { 12.f,  0.f}},
    {"CONSUMABLE_CARROT",           {  8.f,  0.f}},
    {"CONSUMABLE_CANNED_BEANS",     { 30.f,  0.f}},
    {"CONSUMABLE_CANNED_PEACHES",   { 22.f,  5.f}},
    {"CONSUMABLE_CANNED_PEARS",     { 22.f,  5.f}},
    {"CONSUMABLE_CANNED_APRICOTS",  { 22.f,  5.f}},
    {"CONSUMABLE_CANNED_TOMATOES",  { 22.f,  5.f}},
    {"CONSUMABLE_CANNED_SALMON",    { 30.f,  0.f}},
    {"CONSUMABLE_CANNED_CORNED_BEEF",{ 32.f, 0.f}},
    {"CONSUMABLE_CHOCOLATE_BAR",    { 12.f,  0.f}},
    {"CONSUMABLE_CANDY",            {  5.f,  0.f}},
    {"CONSUMABLE_JERKY",            { 15.f, -3.f}},
    {"CONSUMABLE_STEAK",            { 40.f,  0.f}},
    {"CONSUMABLE_MEAT_STEAK",       { 40.f,  0.f}},
    {"CONSUMABLE_STEW",             { 35.f,  8.f}},
    {"CONSUMABLE_EGG",              { 10.f,  0.f}},
    {"CONSUMABLE_OATMEAL",          { 20.f,  5.f}},
    // ---------- BEBIDAS (fome, sede) ----------
    {"CONSUMABLE_WATER",            {  0.f, 35.f}},
    {"CONSUMABLE_MILK",             {  5.f, 25.f}},
    {"CONSUMABLE_COFFEE",           {  0.f, 15.f}},
    {"CONSUMABLE_TEA",              {  0.f, 15.f}},
    {"CONSUMABLE_BEER",             {  2.f, 15.f}},
    {"CONSUMABLE_WHISKEY",          {  0.f, 12.f}},
    {"CONSUMABLE_RUM",              {  0.f, 10.f}},
    {"CONSUMABLE_BRANDY",           {  0.f, 10.f}},
    {"CONSUMABLE_WINE",             {  0.f, 12.f}},
    {"CONSUMABLE_MOONSHINE",        {  0.f,  8.f}},
    {"CONSUMABLE_ABSINTHE",         {  0.f,  8.f}},
    {"CONSUMABLE_TONIC",            {  0.f,  8.f}},
};

// ------------------ ESTADO ------------------
static float g_hunger = 80.f;   // 0..100
static float g_thirst = 80.f;
static std::map<Hash, int> g_lastCount; // contagem anterior de cada item
static const char* SAVE_FILE = "SurvivalNeeds.ini";

// ------------------ SAVE / LOAD ------------------
static void Save() {
    std::ofstream f(SAVE_FILE);
    if (f) f << g_hunger << "\n" << g_thirst << "\n";
}
static void Load() {
    std::ifstream f(SAVE_FILE);
    if (f) { f >> g_hunger >> g_thirst; }
    g_hunger = std::clamp(g_hunger, 0.f, 100.f);
    g_thirst = std::clamp(g_thirst, 0.f, 100.f);
}

// ------------------ WRAPPERS DE NATIVES ------------------
// [CONFIRMAR] os nomes/hashes abaixo na versão atual do SDK.

static int GetItemCount(Hash item) {
    // Contagem do item no inventário do jogador.
    // Natives de inventário (INVENTORY::_INVENTORY_GET_INVENTORY_ITEM_COUNT...)
    // variam; confira no nativedb e preencha aqui. [CONFIRMAR]
    // Exemplo (ajuste o hash!):
    //   return invoke<int>(0x0000000000000000 /*[CONFIRMAR]*/, PLAYER::PLAYER_PED_ID(), item);
    return 0;
}

static void DrainHealth(Ped ped, float amount) {
    int hp = ENTITY::GET_ENTITY_HEALTH(ped);
    if (hp > 5) ENTITY::SET_ENTITY_HEALTH(ped, hp - (int)std::max(1.f, amount), 0);
}

// ------------------ HUD ------------------
static void DrawBar(float x, float y, float w, float h, float pct,
                    int r, int g, int b) {
    // fundo
    GRAPHICS::DRAW_RECT(x, y, w, h, 0, 0, 0, 160, 0, 0);
    // preenchimento
    float fw = w * (pct / 100.f);
    GRAPHICS::DRAW_RECT(x - (w - fw) / 2.f, y, fw, h * 0.8f, r, g, b, 220, 0, 0);
}

static void DrawHud() {
    // canto inferior esquerdo; ajuste X/Y se sobrepor o mapa
    DrawBar(0.075f, 0.935f, 0.10f, 0.012f, g_hunger, 230, 140, 30);  // fome = laranja
    DrawBar(0.075f, 0.955f, 0.10f, 0.012f, g_thirst,  50, 150, 230); // sede = azul
}

// ------------------ LÓGICA ------------------
static float ActivityMultiplier(Ped ped) {
    if (PED::IS_PED_ON_MOUNT(ped))       return CFG.rideMultiplier;
    if (PED::IS_PED_SPRINTING(ped))      return CFG.sprintMultiplier;
    if (PED::IS_PED_RUNNING(ped))        return CFG.runMultiplier;
    return 1.0f;
}

static void LogConsumed(const std::string& name) {
    std::ofstream f("SurvivalNeeds_log.txt", std::ios::app);
    if (f) f << "consumido: " << name << "\n";
}

static void OnConsumed(const Effect& e) {
    g_hunger = std::clamp(g_hunger + e.hunger, 0.f, 100.f);
    g_thirst = std::clamp(g_thirst + e.thirst, 0.f, 100.f);
}

// Detecta consumo: se a contagem de um item no inventário diminuiu,
// o jogador usou/comeu/bebeu aquele item (funciona para itens
// do inventário e para bebidas compradas no saloon que entram nele).
static void CheckConsumption() {
    for (auto& [name, effect] : ITEMS) {
        Hash h = MISC::GET_HASH_KEY(name.c_str());
        int now = GetItemCount(h);
        auto it = g_lastCount.find(h);
        if (it != g_lastCount.end() && now < it->second)
            { OnConsumed(effect); LogConsumed(name); }
        g_lastCount[h] = now;
    }
}

static void Tick(float dt) {
    Ped ped = PLAYER::PLAYER_PED_ID();
    if (ENTITY::IS_ENTITY_DEAD(ped)) return;

    float mult = ActivityMultiplier(ped);
    g_hunger = std::max(0.f, g_hunger - CFG.hungerDrainPerMin / 60.f * dt * mult);
    g_thirst = std::max(0.f, g_thirst - CFG.thirstDrainPerMin / 60.f * dt * mult);

    // Penalidade quando zera
    static float acc = 0.f;
    if (g_hunger <= 0.f || g_thirst <= 0.f) {
        acc += dt;
        if (acc >= 1.f) { DrainHealth(ped, CFG.starveHpPerSec); acc = 0.f; }
    }
}

// ------------------ LOOP PRINCIPAL ------------------
void ScriptMain() {
    Load();
    DWORD last = GetTickCount();
    float saveTimer = 0.f;

    while (true) {
        DWORD nowT = GetTickCount();
        float dt = (nowT - last) / 1000.f;
        last = nowT;

        Tick(dt);
        CheckConsumption();
        DrawHud();

        saveTimer += dt;
        if (saveTimer >= CFG.saveEverySec) { Save(); saveTimer = 0.f; }

        // Debug: F9 enche as barras
        if (GetAsyncKeyState(VK_F9) & 1) { g_hunger = g_thirst = 100.f; }

        WAIT(0);
    }
}
