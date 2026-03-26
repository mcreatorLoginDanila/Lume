#define BUILDING_PLUGIN
#include "../lume_plugin.h"
#define M_PI 3.14159265358979323846
LumeHostAPI* g_api = nullptr;
void* my_alloc(SIZE_T size) { return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size); }
void* my_realloc(void* ptr, SIZE_T size) { return ptr ? HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ptr, size) : my_alloc(size); }
void my_free(void* ptr) { if (ptr) HeapFree(GetProcessHeap(), 0, ptr); }
int my_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
int my_atoi(const char* str) {
    int res = 0, sign = 1;
    if (*str == '-') { sign = -1; str++; }
    while (*str >= '0' && *str <= '9') { res = res * 10 + (*str - '0'); str++; }
    return res * sign;
}
static unsigned int g_seed = 12345;
int my_rand() { g_seed = g_seed * 1103515245 + 12345; return (int)((g_seed / 65536) % 32768); }
double fast_sin(double x) {
    while (x > M_PI) x -= 2.0 * M_PI;
    while (x < -M_PI) x += 2.0 * M_PI;
    double sin_x = 1.27323954 * x - 0.405284735 * x * (x < 0 ? -x : x);
    return 0.225 * (sin_x * (sin_x < 0 ? -sin_x : sin_x) - sin_x) + sin_x;
}
struct WavBuffer {
    unsigned char* data;
    int size;
    int capacity;
};
static WavBuffer g_activeWav = { nullptr, 0, 0 };
void wav_reset(WavBuffer* wb) { wb->size = 0; }
void wav_push(WavBuffer* wb, unsigned char v) {
    if (wb->size >= wb->capacity) {
        wb->capacity = (wb->capacity == 0) ? 8192 : wb->capacity * 2;
        wb->data = (unsigned char*)my_realloc(wb->data, wb->capacity);
    }
    wb->data[wb->size++] = v;
}
void writeWavHeader(WavBuffer* wb, int sampleRate, int numSamples) {
    int fileSize = 36 + numSamples;
    auto write16 = [&](unsigned short v) { wav_push(wb, v & 0xFF); wav_push(wb, (v >> 8) & 0xFF); };
    auto write32 = [&](unsigned int v) {
        wav_push(wb, v & 0xFF); wav_push(wb, (v >> 8) & 0xFF);
        wav_push(wb, (v >> 16) & 0xFF); wav_push(wb, (v >> 24) & 0xFF);
        };
    wav_push(wb, 'R'); wav_push(wb, 'I'); wav_push(wb, 'F'); wav_push(wb, 'F');
    write32(fileSize);
    wav_push(wb, 'W'); wav_push(wb, 'A'); wav_push(wb, 'V'); wav_push(wb, 'E');
    wav_push(wb, 'f'); wav_push(wb, 'm'); wav_push(wb, 't'); wav_push(wb, ' ');
    write32(16); write16(1); write16(1); write32(sampleRate); write32(sampleRate);
    write16(1); write16(8);
    wav_push(wb, 'd'); wav_push(wb, 'a'); wav_push(wb, 't'); wav_push(wb, 'a');
    write32(numSamples);
}
enum WaveType { WAVE_SQUARE, WAVE_SINE, WAVE_SAW, WAVE_TRIANGLE, WAVE_NOISE };
WaveType parseWaveType(const char* s) {
    if (!s) return WAVE_SQUARE;
    if (my_strcmp(s, "sine") == 0) return WAVE_SINE;
    if (my_strcmp(s, "saw") == 0) return WAVE_SAW;
    if (my_strcmp(s, "triangle") == 0) return WAVE_TRIANGLE;
    if (my_strcmp(s, "noise") == 0) return WAVE_NOISE;
    return WAVE_SQUARE;
}
unsigned char generateSample(double phase, WaveType type, int volume) {
    double val = 0.0;
    switch (type) {
    case WAVE_SQUARE:   val = (phase < M_PI) ? 1.0 : -1.0; break;
    case WAVE_SINE:     val = fast_sin(phase); break;
    case WAVE_SAW:      val = 1.0 - (phase / M_PI); break;
    case WAVE_TRIANGLE: val = (phase < M_PI) ? (-1.0 + (2.0 * phase / M_PI)) : (3.0 - (2.0 * phase / M_PI)); break;
    case WAVE_NOISE:    val = ((double)(my_rand() % 2000) - 1000.0) / 1000.0; break;
    }
    int sample = (int)(128 + (val * (volume / 100.0)) * 127);
    return (unsigned char)(sample < 0 ? 0 : (sample > 255 ? 255 : sample));
}
struct ToneSegment { double freq; int durationMs; int volume; WaveType type; };
void generateMultiTone(WavBuffer* wb, ToneSegment* tones, int toneCount) {
    wav_reset(wb);
    int sampleRate = 22050;
    int totalSamples = 0;
    for (int i = 0; i < toneCount; i++) totalSamples += (sampleRate * tones[i].durationMs) / 1000;
    writeWavHeader(wb, sampleRate, totalSamples);
    for (int t = 0; t < toneCount; t++) {
        int numSamples = (sampleRate * tones[t].durationMs) / 1000;
        double phaseInc = (2.0 * M_PI * tones[t].freq) / sampleRate;
        double phase = 0.0;
        int att = (int)(sampleRate * 0.005);
        int rel = (int)(sampleRate * 0.02);
        for (int i = 0; i < numSamples; i++) {
            if (tones[t].freq < 1.0) { wav_push(wb, 128); continue; }
            double env = 1.0;
            if (i < att) env = (double)i / att;
            if (i > numSamples - rel) env = (double)(numSamples - i) / rel;
            wav_push(wb, generateSample(phase, tones[t].type, (int)(tones[t].volume * env)));
            phase += phaseInc;
            if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
        }
    }
}
void playCurrentWav() {
    PlaySoundA(nullptr, nullptr, 0);
    if (g_activeWav.size > 0) {
        PlaySoundA((LPCSTR)g_activeWav.data, nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    }
}
static double g_baseFreqs[12] = { 261.63, 277.18, 293.66, 311.13, 329.63, 349.23, 369.99, 392.00, 415.30, 440.00, 466.16, 493.88 };
double noteToFreq(const char* noteStr, int octave) {
    if (!noteStr || !noteStr[0]) return 0.0;
    int idx = 0;
    char n = noteStr[0], acc = noteStr[1];
    if (n == 'C') idx = 0; else if (n == 'D') idx = 2; else if (n == 'E') idx = 4;
    else if (n == 'F') idx = 5; else if (n == 'G') idx = 7; else if (n == 'A') idx = 9;
    else if (n == 'B') idx = 11; else return 0.0;
    if (acc == '#' || acc == 'b') {
        if (acc == '#') idx++; else idx--;
        if (idx < 0) idx = 11;
        if (idx > 11) idx = 0;
    }
    double freq = g_baseFreqs[idx];
    if (octave > 4) for (int i = 0; i < octave - 4; i++) freq *= 2.0;
    if (octave < 4) for (int i = 0; i < 4 - octave; i++) freq /= 2.0;
    return freq;
}
static int l_beep(lua_State* L) {
    Beep((DWORD)luaL_checkinteger(L, 1), (DWORD)luaL_checkinteger(L, 2));
    return 0;
}
static int l_stopSound(lua_State* L) {
    PlaySoundA(nullptr, nullptr, 0);
    return 0;
}
static int l_getNoteFreq(lua_State* L) {
    lua_pushnumber(L, noteToFreq(luaL_checkstring(L, 1), (int)luaL_checkinteger(L, 2)));
    return 1;
}
static int l_playTone(lua_State* L) {
    PlaySoundA(nullptr, nullptr, 0);
    ToneSegment t;
    t.freq = luaL_checknumber(L, 1);
    t.durationMs = (int)luaL_checkinteger(L, 2);
    t.volume = (int)luaL_optinteger(L, 3, 80);
    t.type = parseWaveType(luaL_optstring(L, 4, "square"));
    generateMultiTone(&g_activeWav, &t, 1);
    playCurrentWav();
    return 0;
}
static int l_playNote(lua_State* L) {
    PlaySoundA(nullptr, nullptr, 0);
    ToneSegment t;
    t.freq = noteToFreq(luaL_checkstring(L, 1), (int)luaL_checkinteger(L, 2));
    t.durationMs = (int)luaL_checkinteger(L, 3);
    t.volume = (int)luaL_optinteger(L, 4, 80);
    t.type = parseWaveType(luaL_optstring(L, 5, "square"));
    if (t.freq > 0) {
        generateMultiTone(&g_activeWav, &t, 1);
        playCurrentWav();
    }
    return 0;
}
static int l_playMelody(lua_State* L) {
    PlaySoundA(nullptr, nullptr, 0);
    const char* melody = luaL_checkstring(L, 1);
    int tempo = (int)luaL_optinteger(L, 2, 120);
    int vol = (int)luaL_optinteger(L, 3, 80);
    WaveType type = parseWaveType(luaL_optstring(L, 4, "square"));
    double wholeNote = (4.0 * 60000.0) / tempo;
    int toneCap = 32;
    int toneCount = 0;
    ToneSegment* tones = (ToneSegment*)my_alloc(toneCap * sizeof(ToneSegment));
    const char* ptr = melody;
    while (*ptr) {
        while (*ptr == ' ' || *ptr == '\n' || *ptr == '\t' || *ptr == '\r') ptr++;
        if (!*ptr) break;
        const char* start = ptr;
        while (*ptr && *ptr != ' ' && *ptr != '\n' && *ptr != '\t' && *ptr != '\r') ptr++;
        int len = (int)(ptr - start);
        double freq = 0.0;
        int divider = 4;
        if (len == 1 && (start[0] == '-' || start[0] == '.' || start[0] == 'R')) {
            freq = 0.0;
        }
        else {
            int slash_idx = -1;
            for (int i = 0; i < len; i++) if (start[i] == '/') { slash_idx = i; break; }
            int note_len = (slash_idx == -1) ? len : slash_idx;
            char noteStr[16] = { 0 };
            for (int i = 0; i < note_len && i < 15; i++) noteStr[i] = start[i];
            int i = 1;
            if (noteStr[1] == '#' || noteStr[1] == 'b') i = 2;
            int octave = 4;
            if (noteStr[i] != '\0') octave = my_atoi(noteStr + i);
            freq = noteToFreq(noteStr, octave);
            if (slash_idx != -1 && (slash_idx + 1) < len) {
                char divStr[16] = { 0 };
                int d_start = slash_idx + 1;
                for (int j = d_start; j < len && (j - d_start) < 15; j++) divStr[j - d_start] = start[j];
                divider = my_atoi(divStr);
                if (divider <= 0) divider = 4;
            }
        }
        if (toneCount >= toneCap) {
            toneCap *= 2;
            tones = (ToneSegment*)my_realloc(tones, toneCap * sizeof(ToneSegment));
        }
        tones[toneCount].freq = freq;
        tones[toneCount].durationMs = (int)(wholeNote / divider);
        tones[toneCount].volume = vol;
        tones[toneCount].type = type;
        toneCount++;
    }
    if (toneCount > 0) {
        generateMultiTone(&g_activeWav, tones, toneCount);
        playCurrentWav();
    }
    my_free(tones);
    return 0;
}
static int l_playSfx(lua_State* L) {
    PlaySoundA(nullptr, nullptr, 0);
    const char* name = luaL_checkstring(L, 1);
    int v = (int)luaL_optinteger(L, 2, 80);
    ToneSegment ts[32]; int count = 0;
    if (my_strcmp(name, "coin") == 0) {
        ts[0] = { 987.77, 60, v, WAVE_SQUARE }; ts[1] = { 1318.5, 120, v, WAVE_SQUARE }; count = 2;
    }
    else if (my_strcmp(name, "jump") == 0) {
        for (int i = 0; i < 8; i++) ts[i] = { 200.0 + i * 80, 20, v, WAVE_SQUARE }; count = 8;
    }
    else if (my_strcmp(name, "hit") == 0) {
        ts[0] = { 200,30,v,WAVE_NOISE }; ts[1] = { 150,30,v,WAVE_NOISE }; ts[2] = { 100,50,v,WAVE_NOISE }; ts[3] = { 60,80,v,WAVE_NOISE }; count = 4;
    }
    else if (my_strcmp(name, "powerup") == 0) {
        for (int i = 0; i < 12; i++) ts[i] = { 300.0 + i * 50, 35, v, WAVE_SQUARE }; ts[12] = { 900.0, 150, v, WAVE_SQUARE }; count = 13;
    }
    else if (my_strcmp(name, "gameover") == 0) {
        ts[0] = { 523.25,200,v,WAVE_SQUARE }; ts[1] = { 493.88,200,v,WAVE_SQUARE }; ts[2] = { 466.16,200,v,WAVE_SQUARE };
        ts[3] = { 440.00,400,v,WAVE_SQUARE }; ts[4] = { 0,100,0,WAVE_SQUARE }; ts[5] = { 349.23,200,v,WAVE_SQUARE }; ts[6] = { 261.63,500,v,WAVE_SQUARE }; count = 7;
    }
    else if (my_strcmp(name, "laser") == 0) {
        for (int i = 0; i < 10; i++) ts[i] = { 1500.0 - i * 120, 15, v, WAVE_SAW }; count = 10;
    }
    else if (my_strcmp(name, "explosion") == 0) {
        for (int i = 0; i < 15; i++) { int vol = v - i * 5; ts[i] = { 80.0 - i * 3, 30, vol < 5 ? 5 : vol, WAVE_NOISE }; } count = 15;
    }
    else if (my_strcmp(name, "beep_up") == 0) {
        ts[0] = { 440,80,v,WAVE_SQUARE }; ts[1] = { 660,80,v,WAVE_SQUARE }; ts[2] = { 880,120,v,WAVE_SQUARE }; count = 3;
    }
    else if (my_strcmp(name, "beep_down") == 0) {
        ts[0] = { 880,80,v,WAVE_SQUARE }; ts[1] = { 660,80,v,WAVE_SQUARE }; ts[2] = { 440,120,v,WAVE_SQUARE }; count = 3;
    }
    else if (my_strcmp(name, "select") == 0) {
        ts[0] = { 600,40,v,WAVE_SQUARE }; ts[1] = { 800,60,v,WAVE_SQUARE }; count = 2;
    }
    else if (my_strcmp(name, "error") == 0) {
        ts[0] = { 200,150,v,WAVE_SQUARE }; ts[1] = { 0,50,0,WAVE_SQUARE }; ts[2] = { 200,150,v,WAVE_SQUARE }; count = 3;
    }
    else if (my_strcmp(name, "1up") == 0) {
        ts[0] = { 523.25,80,v,WAVE_SQUARE }; ts[1] = { 659.25,80,v,WAVE_SQUARE }; ts[2] = { 783.99,80,v,WAVE_SQUARE };
        ts[3] = { 1046.5,120,v,WAVE_SQUARE }; ts[4] = { 783.99,80,v,WAVE_SQUARE }; ts[5] = { 1046.5,200,v,WAVE_SQUARE }; count = 6;
    }
    else {
        lua_pushboolean(L, 0); lua_pushstring(L, "Unknown SFX"); return 2;
    }
    generateMultiTone(&g_activeWav, ts, count);
    playCurrentWav();
    lua_pushboolean(L, 1);
    return 1;
}
extern "C" LUME_PLUGIN_EXPORT int lume_plugin_init(lua_State* L, LumeHostAPI* api) {
    g_api = api;
    g_seed = GetTickCount();
    lua_register(L, "beep", l_beep);
    lua_register(L, "playTone", l_playTone);
    lua_register(L, "playNote", l_playNote);
    lua_register(L, "playMelody", l_playMelody);
    lua_register(L, "playSfx", l_playSfx);
    lua_register(L, "stopSound", l_stopSound);
    lua_register(L, "getNoteFreq", l_getNoteFreq);
    OutputDebugStringA("[Plugin] Sound Plugin (Full Version, No-CRT) Loaded!\n");
    return 0;
}