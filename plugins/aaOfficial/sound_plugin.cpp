#define BUILDING_PLUGIN
#define _USE_MATH_DEFINES
#include "../lume_plugin.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#pragma comment(lib, "winmm.lib")
static LumeHostAPI* g_api = nullptr;
struct WavBuffer {
    std::vector<unsigned char> data;
};
static void writeWavHeader(std::vector<unsigned char>& buf,int sampleRate, int numSamples) {
    int dataSize = numSamples;
    int fileSize = 44 + dataSize;
    auto write16 = [&](unsigned short v) {
        buf.push_back(v & 0xFF);
        buf.push_back((v >> 8) & 0xFF);
    };
    auto write32 = [&](unsigned int v) {
        buf.push_back(v & 0xFF);
        buf.push_back((v >> 8) & 0xFF);
        buf.push_back((v >> 16) & 0xFF);
        buf.push_back((v >> 24) & 0xFF);
    };
    buf.push_back('R'); buf.push_back('I');
    buf.push_back('F'); buf.push_back('F');
    write32(fileSize - 8);
    buf.push_back('W'); buf.push_back('A');
    buf.push_back('V'); buf.push_back('E');
    buf.push_back('f'); buf.push_back('m');
    buf.push_back('t'); buf.push_back(' ');
    write32(16);
    write16(1);
    write16(1);
    write32(sampleRate);
    write32(sampleRate);
    write16(1);
    write16(8);
    buf.push_back('d'); buf.push_back('a');
    buf.push_back('t'); buf.push_back('a');
    write32(dataSize);
}
enum WaveType {
    WAVE_SQUARE,
    WAVE_SINE,
    WAVE_SAW,
    WAVE_TRIANGLE,
    WAVE_NOISE
};
static WaveType parseWaveType(const char* s) {
    if (!s) return WAVE_SQUARE;
    std::string t = s;
    if (t == "sine")     return WAVE_SINE;
    if (t == "saw")      return WAVE_SAW;
    if (t == "triangle") return WAVE_TRIANGLE;
    if (t == "noise")    return WAVE_NOISE;
    return WAVE_SQUARE;
}
static unsigned char generateSample(double phase, WaveType type,int volume) {
    double val = 0.0;
    switch (type) {
    case WAVE_SQUARE:
        val = (phase < M_PI) ? 1.0 : -1.0;
        break;
    case WAVE_SINE:
        val = sin(phase);
        break;
    case WAVE_SAW:
        val = 1.0 - (phase / M_PI);
        break;
    case WAVE_TRIANGLE:
        if (phase < M_PI)
            val = -1.0 + (2.0 * phase / M_PI);
        else
            val = 3.0 - (2.0 * phase / M_PI);
        break;
    case WAVE_NOISE:
        val = ((double)(rand() % 2000) - 1000.0) / 1000.0;
        break;
    }
    double scaled = val * (volume / 100.0);
    int sample = (int)(128 + scaled * 127);
    if (sample < 0)   sample = 0;
    if (sample > 255) sample = 255;
    return (unsigned char)sample;
}
static WavBuffer generateTone(double freq, int durationMs,int volume, WaveType type) {
    WavBuffer wav;
    int sampleRate = 22050;
    int numSamples = (sampleRate * durationMs) / 1000;
    writeWavHeader(wav.data, sampleRate, numSamples);
    double phaseInc = (2.0 * M_PI * freq) / sampleRate;
    double phase = 0.0;
    int attackSamples  = (int)(sampleRate * 0.005);
    int releaseSamples = (int)(sampleRate * 0.02);
    for (int i = 0; i < numSamples; i++) {
        double env = 1.0;
        if (i < attackSamples)
            env = (double)i / attackSamples;
        if (i > numSamples - releaseSamples)
            env = (double)(numSamples - i) / releaseSamples;
        int vol = (int)(volume * env);
        unsigned char s = generateSample(phase, type, vol);
        wav.data.push_back(s);
        phase += phaseInc;
        if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
    }
    return wav;
}
struct ToneSegment {
    double freq;
    int durationMs;
    int volume;
    WaveType type;
};
static WavBuffer generateMultiTone(const std::vector<ToneSegment>& tones) {
    WavBuffer wav;
    int sampleRate = 22050;
    int totalSamples = 0;
    for (auto& t : tones)
        totalSamples += (sampleRate * t.durationMs) / 1000;
    writeWavHeader(wav.data, sampleRate, totalSamples);
    for (auto& t : tones) {
        int numSamples = (sampleRate * t.durationMs) / 1000;
        double phaseInc = (2.0 * M_PI * t.freq) / sampleRate;
        double phase = 0.0;
        int attackSamples  = (int)(sampleRate * 0.003);
        int releaseSamples = (int)(sampleRate * 0.01);
        for (int i = 0; i < numSamples; i++) {
            if (t.freq < 1.0) {
                wav.data.push_back(128);
                continue;
            }
            double env = 1.0;
            if (i < attackSamples)
                env = (double)i / attackSamples;
            if (i > numSamples - releaseSamples)
                env = (double)(numSamples - i) / releaseSamples;
            int vol = (int)(t.volume * env);
            unsigned char s = generateSample(phase, t.type, vol);
            wav.data.push_back(s);
            phase += phaseInc;
            if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
        }
    }
    return wav;
}
static void playWavBuffer(const WavBuffer& wav) {
    PlaySoundA(nullptr, nullptr, 0);
    PlaySoundA((LPCSTR)wav.data.data(), nullptr,
        SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}
static void playWavBufferSync(const WavBuffer& wav) {
    PlaySoundA((LPCSTR)wav.data.data(), nullptr,
        SND_MEMORY | SND_SYNC | SND_NODEFAULT);
}
static double noteToFreq(const std::string& note, int octave) {
    int semitone = 0;
    if      (note == "C")  semitone = -9;
    else if (note == "C#" || note == "Db") semitone = -8;
    else if (note == "D")  semitone = -7;
    else if (note == "D#" || note == "Eb") semitone = -6;
    else if (note == "E")  semitone = -5;
    else if (note == "F")  semitone = -4;
    else if (note == "F#" || note == "Gb") semitone = -3;
    else if (note == "G")  semitone = -2;
    else if (note == "G#" || note == "Ab") semitone = -1;
    else if (note == "A")  semitone = 0;
    else if (note == "A#" || note == "Bb") semitone = 1;
    else if (note == "B")  semitone = 2;
    else return 0.0;
    int n = semitone + (octave - 4) * 12;
    return 440.0 * pow(2.0, n / 12.0);
}
static bool parseMelodyToken(const std::string& token,double* freq, int* divider) {
    *divider = 4;
    *freq = 0.0;
    if (token == "-" || token == "." || token == "R") {
        *freq = 0.0;
        return true;
    }
    std::string noteStr;
    int octave = 4;
    std::string durStr;
    size_t slash = token.find('/');
    std::string notePart = (slash != std::string::npos)
        ? token.substr(0, slash) : token;
    if (slash != std::string::npos)
        durStr = token.substr(slash + 1);
    if (notePart.empty()) return false;
    size_t i = 0;
    noteStr += notePart[i++];
    if (i < notePart.length() &&
        (notePart[i] == '#' || notePart[i] == 'b')) {
        noteStr += notePart[i++];
    }
    if (i < notePart.length()) {
        try { octave = std::stoi(notePart.substr(i)); }
        catch (...) { octave = 4; }
    }
    *freq = noteToFreq(noteStr, octave);
    if (!durStr.empty()) {
        try { *divider = std::stoi(durStr); }
        catch (...) { *divider = 4; }
    }
    return true;
}
static WavBuffer sfxCoin(int volume) {
    std::vector<ToneSegment> t;
    t.push_back({987.77, 60, volume, WAVE_SQUARE});
    t.push_back({1318.5, 120, volume, WAVE_SQUARE});
    return generateMultiTone(t);
}
static WavBuffer sfxJump(int volume) {
    std::vector<ToneSegment> t;
    for (int i = 0; i < 8; i++) {
        double f = 200 + i * 80;
        t.push_back({f, 20, volume, WAVE_SQUARE});
    }
    return generateMultiTone(t);
}
static WavBuffer sfxHit(int volume) {
    std::vector<ToneSegment> t;
    t.push_back({200, 30, volume, WAVE_NOISE});
    t.push_back({150, 30, volume, WAVE_NOISE});
    t.push_back({100, 50, volume, WAVE_NOISE});
    t.push_back({60,  80, volume, WAVE_NOISE});
    return generateMultiTone(t);
}
static WavBuffer sfxPowerup(int volume) {
    std::vector<ToneSegment> t;
    for (int i = 0; i < 12; i++) {
        double f = 300 + i * 50;
        t.push_back({f, 35, volume, WAVE_SQUARE});
    }
    t.push_back({900, 150, volume, WAVE_SQUARE});
    return generateMultiTone(t);
}
static WavBuffer sfxGameover(int volume) {
    std::vector<ToneSegment> t;
    t.push_back({523.25, 200, volume, WAVE_SQUARE});
    t.push_back({493.88, 200, volume, WAVE_SQUARE});
    t.push_back({466.16, 200, volume, WAVE_SQUARE});
    t.push_back({440.00, 400, volume, WAVE_SQUARE});
    t.push_back({0, 100, 0, WAVE_SQUARE});
    t.push_back({349.23, 200, volume, WAVE_SQUARE});
    t.push_back({261.63, 500, volume, WAVE_SQUARE});
    return generateMultiTone(t);
}
static WavBuffer sfxLaser(int volume) {
    std::vector<ToneSegment> t;
    for (int i = 0; i < 10; i++) {
        double f = 1500 - i * 120;
        t.push_back({f, 15, volume, WAVE_SAW});
    }
    return generateMultiTone(t);
}
static WavBuffer sfxExplosion(int volume) {
    std::vector<ToneSegment> t;
    for (int i = 0; i < 15; i++) {
        int v = volume - i * 5;
        if (v < 5) v = 5;
        t.push_back({80.0 - i * 3, 30, v, WAVE_NOISE});
    }
    return generateMultiTone(t);
}
static WavBuffer sfxBeepUp(int volume) {
    std::vector<ToneSegment> t;
    t.push_back({440, 80, volume, WAVE_SQUARE});
    t.push_back({660, 80, volume, WAVE_SQUARE});
    t.push_back({880, 120, volume, WAVE_SQUARE});
    return generateMultiTone(t);
}
static WavBuffer sfxBeepDown(int volume) {
    std::vector<ToneSegment> t;
    t.push_back({880, 80, volume, WAVE_SQUARE});
    t.push_back({660, 80, volume, WAVE_SQUARE});
    t.push_back({440, 120, volume, WAVE_SQUARE});
    return generateMultiTone(t);
}
static WavBuffer sfxSelect(int volume) {
    std::vector<ToneSegment> t;
    t.push_back({600, 40, volume, WAVE_SQUARE});
    t.push_back({800, 60, volume, WAVE_SQUARE});
    return generateMultiTone(t);
}
static WavBuffer sfxError(int volume) {
    std::vector<ToneSegment> t;
    t.push_back({200, 150, volume, WAVE_SQUARE});
    t.push_back({0,   50,  0,      WAVE_SQUARE});
    t.push_back({200, 150, volume, WAVE_SQUARE});
    return generateMultiTone(t);
}
static WavBuffer sfx1up(int volume) {
    std::vector<ToneSegment> t;
    t.push_back({523.25, 80, volume, WAVE_SQUARE});
    t.push_back({659.25, 80, volume, WAVE_SQUARE});
    t.push_back({783.99, 80, volume, WAVE_SQUARE});
    t.push_back({1046.5, 120, volume, WAVE_SQUARE});
    t.push_back({783.99, 80, volume, WAVE_SQUARE});
    t.push_back({1046.5, 200, volume, WAVE_SQUARE});
    return generateMultiTone(t);
}
static int l_beep(lua_State* L) {
    int freq = (int)luaL_checkinteger(L, 1);
    int dur  = (int)luaL_checkinteger(L, 2);
    Beep(freq, dur);
    return 0;
}
static int l_playTone(lua_State* L) {
    double freq = (double)luaL_checknumber(L, 1);
    int dur     = (int)luaL_checkinteger(L, 2);
    int vol     = (int)luaL_optinteger(L, 3, 80);
    const char* wt = luaL_optstring(L, 4, "square");
    WavBuffer wav = generateTone(freq, dur, vol, parseWaveType(wt));
    playWavBuffer(wav);
    return 0;
}
static int l_playNote(lua_State* L) {
    const char* note = luaL_checkstring(L, 1);
    int octave = (int)luaL_checkinteger(L, 2);
    int dur    = (int)luaL_checkinteger(L, 3);
    int vol    = (int)luaL_optinteger(L, 4, 80);
    const char* wt = luaL_optstring(L, 5, "square");
    double freq = noteToFreq(note, octave);
    if (freq > 0) {
        WavBuffer wav = generateTone(freq, dur, vol, parseWaveType(wt));
        playWavBuffer(wav);
    }
    return 0;
}
static int l_playMelody(lua_State* L) {
    const char* melody = luaL_checkstring(L, 1);
    int tempo = (int)luaL_optinteger(L, 2, 120);
    int vol   = (int)luaL_optinteger(L, 3, 80);
    const char* wt = luaL_optstring(L, 4, "square");
    WaveType type = parseWaveType(wt);
    double wholeNote = (4.0 * 60000.0) / tempo;
    std::vector<ToneSegment> tones;
    std::istringstream ss(melody);
    std::string token;
    while (ss >> token) {
        double freq;
        int divider;
        if (parseMelodyToken(token, &freq, &divider)) {
            int dur = (int)(wholeNote / divider);
            tones.push_back({freq, dur, vol, type});
        }
    }
    if (!tones.empty()) {
        WavBuffer wav = generateMultiTone(tones);
        playWavBuffer(wav);
    }
    return 0;
}
static int l_playSfx(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int vol = (int)luaL_optinteger(L, 2, 80);
    std::string sfx = name;
    WavBuffer wav;
    if      (sfx == "coin")      wav = sfxCoin(vol);
    else if (sfx == "jump")      wav = sfxJump(vol);
    else if (sfx == "hit")       wav = sfxHit(vol);
    else if (sfx == "powerup")   wav = sfxPowerup(vol);
    else if (sfx == "gameover")  wav = sfxGameover(vol);
    else if (sfx == "laser")     wav = sfxLaser(vol);
    else if (sfx == "explosion") wav = sfxExplosion(vol);
    else if (sfx == "beep_up")   wav = sfxBeepUp(vol);
    else if (sfx == "beep_down") wav = sfxBeepDown(vol);
    else if (sfx == "select")    wav = sfxSelect(vol);
    else if (sfx == "error")     wav = sfxError(vol);
    else if (sfx == "1up")       wav = sfx1up(vol);
    else {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Unknown SFX name");
        return 2;
    }
    playWavBuffer(wav);
    lua_pushboolean(L, 1);
    return 1;
}
static int l_stopSound(lua_State* L) {
    PlaySoundA(nullptr, nullptr, 0);
    return 0;
}
static int l_getNoteFreq(lua_State* L) {
    const char* note = luaL_checkstring(L, 1);
    int octave = (int)luaL_checkinteger(L, 2);
    double freq = noteToFreq(note, octave);
    lua_pushnumber(L, freq);
    return 1;
}
extern "C" __declspec(dllexport)
int lume_plugin_init(lua_State* L, LumeHostAPI* api) {
    g_api = api;
    srand((unsigned)GetTickCount());
    lua_register(L, "beep",         l_beep);
    lua_register(L, "playTone",     l_playTone);
    lua_register(L, "playNote",     l_playNote);
    lua_register(L, "playMelody",   l_playMelody);
    lua_register(L, "playSfx",      l_playSfx);
    lua_register(L, "stopSound",    l_stopSound);
    lua_register(L, "getNoteFreq",  l_getNoteFreq);
    OutputDebugStringA("[Plugin] sound_plugin loaded: "
        "beep, playTone, playNote, playMelody, playSfx\n");
    return 0;
}