#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <intrin.h>
int _fltused = 0x9875;
int _purecall(void) {
    ExitProcess(1);
    return 0;
}
int atexit(void (*func)(void)) {
    (void)func;
    return 0;
}
void abort(void) {
    ExitProcess(1);
}
extern int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd);
void WinMainCRTStartup(void) {
    char* cmd = GetCommandLineA();
    if (*cmd == '"') {
        cmd++;
        while (*cmd && *cmd != '"') cmd++;
        if (*cmd == '"') cmd++;
    }
    else {
        while (*cmd > ' ') cmd++;
    }
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    STARTUPINFOA si;
    GetStartupInfoA(&si);
    int show = (si.dwFlags & STARTF_USESHOWWINDOW) ? si.wShowWindow : SW_SHOWDEFAULT;
    int ret = WinMain(GetModuleHandleA(NULL), NULL, cmd, show);
    ExitProcess(ret);
}
#pragma function(memcpy, memset, memmove, memcmp, memchr, strlen, strcpy, strcmp)
void* memcpy(void* dst, const void* src, size_t count) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    size_t words = count / sizeof(size_t);
    size_t tail = count % sizeof(size_t);
    size_t* dw = (size_t*)d;
    const size_t* sw = (const size_t*)s;
    while (words--) {
        *dw++ = *sw++;
    }
    d = (unsigned char*)dw;
    s = (const unsigned char*)sw;
    while (tail--) {
        *d++ = *s++;
    }
    return dst;
}
void* memset(void* dst, int val, size_t count) {
    unsigned char* d = (unsigned char*)dst;
    unsigned char v = (unsigned char)val;
    if (count >= sizeof(size_t)) {
        size_t word_val = v;
        word_val |= (word_val << 8);
        word_val |= (word_val << 16);
        word_val |= (word_val << 32);
        size_t words = count / sizeof(size_t);
        count %= sizeof(size_t);
        size_t* dw = (size_t*)d;
        while (words--) {
            *dw++ = word_val;
        }
        d = (unsigned char*)dw;
    }
    while (count--) {
        *d++ = v;
    }
    return dst;
}
void* memmove(void* dst, const void* src, size_t count) {
    char* d = (char*)dst;
    const char* s = (const char*)src;
    if (d <= s || d >= (s + count)) {
        while (count--) *d++ = *s++;
    }
    else {
        d += count - 1;
        s += count - 1;
        while (count--) *d-- = *s--;
    }
    return dst;
}
int memcmp(const void* buf1, const void* buf2, size_t count) {
    const unsigned char* p1 = (const unsigned char*)buf1;
    const unsigned char* p2 = (const unsigned char*)buf2;
    while (count--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}
void* memchr(const void* buf, int c, size_t count) {
    const unsigned char* p = (const unsigned char*)buf;
    while (count--) {
        if (*p == (unsigned char)c) return (void*)p;
        p++;
    }
    return NULL;
}
size_t strlen(const char* str) {
    const char* s = str;
    while (*s) s++;
    return s - str;
}
char* strcpy(char* dst, const char* src) {
    char* d = dst;
    while ((*d++ = *src++));
    return dst;
}
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
char* strchr(const char* s, int c) {
    while (*s != (char)c) {
        if (!*s++) return NULL;
    }
    return (char*)s;
}
char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char* h = haystack, * n = needle;
            while (*h && *n && (*h == *n)) {
                h++;
                n++;
            }
            if (!*n) return (char*)haystack;
        }
    }
    return NULL;
}
size_t strspn(const char* s, const char* accept) {
    const char* p = s;
    while (*p) {
        const char* a = accept;
        int found = 0;
        while (*a) { if (*p == *a++) { found = 1; break; } }
        if (!found) break;
        p++;
    }
    return p - s;
}
char* strpbrk(const char* s, const char* accept) {
    while (*s) {
        const char* a = accept;
        while (*a) { if (*s == *a++) return (char*)s; }
        s++;
    }
    return NULL;
}
int strcoll(const char* s1, const char* s2) {
    return strcmp(s1, s2);
}
unsigned long strtoul(const char* str, char** endptr, int base) {
    unsigned long res = 0;
    while (*str == ' ' || *str == '\t') str++;
    if (base == 0) {
        if (*str == '0') {
            str++;
            if (*str == 'x' || *str == 'X') { base = 16; str++; }
            else base = 8;
        }
        else base = 10;
    }
    while (*str) {
        int d;
        if (*str >= '0' && *str <= '9') d = *str - '0';
        else if (*str >= 'a' && *str <= 'f') d = *str - 'a' + 10;
        else if (*str >= 'A' && *str <= 'F') d = *str - 'A' + 10;
        else break;
        if (d >= base) break;
        res = res * base + d;
        str++;
    }
    if (endptr) *endptr = (char*)str;
    return res;
}
unsigned __int64 strtoull(const char* str, char** endptr, int base) {
    unsigned __int64 res = 0;
    while (*str == ' ' || *str == '\t') str++;
    if (base == 0) {
        if (*str == '0') {
            str++;
            if (*str == 'x' || *str == 'X') { base = 16; str++; }
            else base = 8;
        }
        else base = 10;
    }
    while (*str) {
        int d;
        if (*str >= '0' && *str <= '9') d = *str - '0';
        else if (*str >= 'a' && *str <= 'f') d = *str - 'a' + 10;
        else if (*str >= 'A' && *str <= 'F') d = *str - 'A' + 10;
        else break;
        if (d >= base) break;
        res = res * (unsigned __int64)base + (unsigned __int64)d;
        str++;
    }
    if (endptr) *endptr = (char*)str;
    return res;
}
__int64 _time64(__int64* outTime) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    unsigned __int64 t = (uli.QuadPart - 116444736000000000ULL) / 10000000ULL;
    if (outTime) *outTime = (__int64)t;
    return (__int64)t;
}
int clock(void) {
    return (int)GetTickCount64();
}
int isdigit(int c) {
    return c >= '0' && c <= '9';
}
int isalnum(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
int toupper(int c) {
    return (c >= 'a' && c <= 'z') ? c - 32 : c;
}
#pragma function(floor, floorf, ceil, ceilf, sqrt, sqrtf, fmod, fmodf, sin, sinf, cos, cosf, tan, tanf, atan2, asin, acos, exp, log, log10, pow)
double _dsign(double x) {
    return x < 0.0 ? -1.0 : (x > 0.0 ? 1.0 : 0.0);
}
float  _fdsign(float x) {
    return x < 0.0f ? -1.0f : (x > 0.0f ? 1.0f : 0.0f);
}
double copysign(double x, double y) {
    union {
        double d;
        unsigned __int64 u;
    } ux = {x}, uy = {y};
    ux.u = (ux.u & ~0x8000000000000000ULL) | (uy.u & 0x8000000000000000ULL);
    return ux.d;
}
float copysignf(float x, float y) {
    union {
        float f;
        unsigned int u;
    } ux = { x }, uy = { y };
    ux.u = (ux.u & ~0x80000000U) | (uy.u & 0x80000000U);
    return ux.f;
}
double trunc(double x) {
    return (double)((__int64)x);
}
float  truncf(float x) {
    return (float)((int)x);
}
double floor(double x) {
    double t = trunc(x);
    return (x < 0.0 && x != t) ? t - 1.0 : t;
}
float  floorf(float x) {
    float t = truncf(x);
    return (x < 0.0f && x != t) ? t - 1.0f : t;
}
double ceil(double x) {
    double t = trunc(x);
    return (x > 0.0 && x != t) ? t + 1.0 : t;
}
float  ceilf(float x) {
    float t = truncf(x);
    return (x > 0.0f && x != t) ? t + 1.0f : t;
}
double rint(double x) {
    return floor(x + 0.5);
}
float  rintf(float x) {
    return floorf(x + 0.5f);
}
double ldexp(double x, int exp_val) {
    double mult = 1.0;
    if (exp_val > 0) while (exp_val--) mult *= 2.0;
    else if (exp_val < 0) while (exp_val++) mult *= 0.5;
    return x * mult;
}
double frexp(double x, int* exp_ptr) {
    if (x == 0.0) {
        if (exp_ptr) *exp_ptr = 0;
        return 0.0;
    }
    int e = 0;
    double ax = x < 0.0 ? -x : x;
    while (ax >= 1.0) {
        ax *= 0.5;
        e++;
    }
    while (ax < 0.5) {
        ax *= 2.0;
        e--;
    }
    if (exp_ptr) *exp_ptr = e;
    return x < 0.0 ? -ax : ax;
}
double sqrt(double x) {
    if (x <= 0.0) return 0.0;
    __m128d v = _mm_set_sd(x);
    v = _mm_sqrt_sd(v, v);
    return _mm_cvtsd_f64(v);
}
float sqrtf(float x) {
    if (x <= 0.0f) return 0.0f;
    __m128 v = _mm_set_ss(x);
    v = _mm_sqrt_ss(v);
    return _mm_cvtss_f32(v);
}
double fmod(double x, double y) {
    if (y == 0.0) return 0.0;
    return x - trunc(x / y) * y;
}
float fmodf(float x, float y) {
    return (float)fmod((double)x, (double)y);
}
#define M_PI_VAL   3.1415926535897932384626433832795
#define M_TWO_PI   6.283185307179586476925286766559
#define M_HALF_PI  1.570796326794896619231321691640
#define M_LN2      0.693147180559945309417
#define M_INV_LN2  1.442695040888963407360
double sin(double x) {
    x = fmod(x, M_TWO_PI);
    if (x > M_PI_VAL) x -= M_TWO_PI;
    else if (x < -M_PI_VAL) x += M_TWO_PI;
    if (x > M_HALF_PI) x = M_PI_VAL - x;
    else if (x < -M_HALF_PI) x = -M_PI_VAL - x;
    double x2 = x * x;
    return x * (1.0 - x2 * (1.0 / 6.0 - x2 * (1.0 / 120.0 - x2 * (1.0 / 5040.0 - x2 * (1.0 / 362880.0)))));
}
float sinf(float x) {
    return (float)sin((double)x);
}
double cos(double x) {
    return sin(x + M_HALF_PI);
}
float cosf(float x) {
    return (float)cos((double)x);
}
double tan(double x) {
    double c = cos(x);
    if (c == 0.0) return (sin(x) >= 0.0) ? 1e300 : -1e300;
    return sin(x) / c;
}
float tanf(float x) {
    return (float)tan((double)x);
}
static double atan_approx(double x) {
    double x2 = x * x;
    return x * (0.999866 - x2 * (0.330299 - x2 * (0.180141 - x2 * (0.085133 - x2 * 0.020835))));
}
double atan2(double y, double x) {
    if (x == 0.0 && y == 0.0) return 0.0;
    double ax = (x < 0.0) ? -x : x;
    double ay = (y < 0.0) ? -y : y;
    double res;
    if (ax >= ay) {
        res = atan_approx(ay / ax);
    }
    else {
        res = M_HALF_PI - atan_approx(ax / ay);
    }
    if (x < 0.0) res = M_PI_VAL - res;
    if (y < 0.0) res = -res;
    return res;
}
double asin(double x) {
    if (x >= 1.0) return M_HALF_PI;
    if (x <= -1.0) return -M_HALF_PI;
    return atan2(x, sqrt(1.0 - x * x));
}
double acos(double x) {
    return M_HALF_PI - asin(x);
}
double exp(double x) {
    if (x > 709.78) return 1e300;
    if (x < -709.78) return 0.0;
    double z = x * M_INV_LN2;
    int k = (int)(z + (z >= 0.0 ? 0.5 : -0.5));
    double f = x - (double)k * M_LN2;
    double r = 1.0, term = 1.0;
    for (int i = 1; i <= 8; i++) {
        term *= f / (double)i;
        r += term;
    }
    return ldexp(r, k);
}
double log(double x) {
    if (x <= 0.0) return -1e300;
    int e = 0;
    double m = frexp(x, &e);
    if (m < 0.7071067811865475) {
        m *= 2.0;
        e--;
    }
    double y = (m - 1.0) / (m + 1.0);
    double y2 = y * y, r = y, term = y;
    for (int i = 3; i <= 15; i += 2) {
        term *= y2;
        r += term / (double)i;
    }
    return 2.0 * r + (double)e * M_LN2;
}
double log10(double x) {
    return log(x) * 0.43429448190325182765;
}
double pow(double x, double y) {
    if (y == 0.0) return 1.0;
    if (x == 0.0) return 0.0;
    if (x < 0.0) {
        __int64 iy = (__int64)y;
        if ((double)iy == y) {
            double res = exp(y * log(-x));
            return (iy & 1) ? -res : res;
        }
        return 0.0;
    }
    return exp(y * log(x));
}
double strtod(const char* str, char** endptr) {
    const char* p = str;
    double sign = 1.0;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '-') {
        sign = -1.0;
        p++;
    }
    else if (*p == '+') p++;
    double val = 0.0;
    int has_digits = 0;
    while (*p >= '0' && *p <= '9') {
        val = val * 10.0 + (*p - '0');
        p++;
        has_digits = 1;
    }
    if (!has_digits) {
        if (endptr) *endptr = (char*)str;
        return 0.0;
    }
    if (*p == '.') {
        p++;
        double frac = 1.0;
        while (*p >= '0' && *p <= '9') {
            frac *= 0.1;
            val += (*p - '0') * frac;
            p++;
        }
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        double exp_sign = 1.0;
        if (*p == '-') { exp_sign = -1.0; p++; }
        else if (*p == '+') p++;
        int exp_val = 0;
        while (*p >= '0' && *p <= '9') {
            exp_val = exp_val * 10 + (*p - '0');
            p++;
        }
        double mult = 1.0, base = 10.0;
        while (exp_val > 0) {
            if (exp_val & 1) mult *= base;
            base *= base;
            exp_val >>= 1;
        }
        if (exp_sign < 0.0) val /= mult;
        else val *= mult;
    }
    if (endptr) *endptr = (char*)p;
    return sign * val;
}
static char* mini_fmt_int(char* buf, unsigned __int64 val, int base, int upper, int width, int pad_zero, int sign, __int64 sval) {
    char tmp[32];
    int i = 0;
    int is_neg = 0;
    if (sign) {
        if (sval < 0) {
            is_neg = 1;
            val = (unsigned __int64)(-sval);
        }
        else {
            val = (unsigned __int64)sval;
        }
    }
    if (val == 0) tmp[i++] = '0';
    while (val > 0) {
        int r = (int)(val % base);
        tmp[i++] = (r < 10) ? ('0' + r) : ((upper ? 'A' : 'a') + (r - 10));
        val /= base;
    }
    if (is_neg && pad_zero) *buf++ = '-';
    int len = i + (is_neg && !pad_zero ? 1 : 0);
    while (width > len) {
        *buf++ = pad_zero ? '0' : ' ';
        width--;
    }
    if (is_neg && !pad_zero) *buf++ = '-';
    while (i > 0) *buf++ = tmp[--i];
    return buf;
}
int __cdecl __stdio_common_vsprintf(unsigned __int64 options, char* buf, size_t max_len, const char* format, void* locale, va_list valist) {
    (void)options; (void)locale;
    if (!buf || max_len == 0) return 0;
    char* out = buf;
    char* end = buf + max_len - 1;
    const char* p = format;
#define EMIT_CHAR(c) do {if (out < end) *out++ = (c);} while(0)
    while (*p) {
        if (*p != '%') {
            EMIT_CHAR(*p++);
            continue;
        }
        p++;
        int pad_zero = 0;
        int width = 0;
        if (*p == '0') {
            pad_zero = 1;
            p++;
        }
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }
        if (p[0] == 'I' && p[1] == '6' && p[2] == '4') p += 3;
        else if (p[0] == 'l' && p[1] == 'l') p += 2;
        else if (*p == 'l') p++;

        switch (*p) {
        case 'd': case 'i': {
            __int64 v = va_arg(valist, __int64);
            out = mini_fmt_int(out, 0, 10, 0, width, pad_zero, 1, v);
            break;
        }
        case 'u': {
            unsigned __int64 v = va_arg(valist, unsigned __int64);
            out = mini_fmt_int(out, v, 10, 0, width, pad_zero, 0, 0);
            break;
        }
        case 'x': {
            unsigned __int64 v = va_arg(valist, unsigned int);
            out = mini_fmt_int(out, v, 16, 0, width, pad_zero, 0, 0);
            break;
        }
        case 'X': {
            unsigned __int64 v = va_arg(valist, unsigned int);
            out = mini_fmt_int(out, v, 16, 1, width, pad_zero, 0, 0);
            break;
        }
        case 's': {
            const char* s = va_arg(valist, const char*);
            if (!s) s = "(null)";
            while (*s) *out++ = *s++;
            break;
        }
        case 'c': {
            *out++ = (char)va_arg(valist, int);
            break;
        }
        case 'g': case 'G': case 'e': case 'E':
        case 'f': {
            double v = va_arg(valist, double);
            if (v < 0.0) { *out++ = '-'; v = -v; }
            unsigned __int64 ip = (unsigned __int64)v;
            double frac = v - (double)ip;
            out = mini_fmt_int(out, ip, 10, 0, 0, 0, 0, 0);
            *out++ = '.';
            for (int d = 0; d < 6; d++) {
                frac *= 10.0;
                int dig = (int)frac;
                *out++ = '0' + dig;
                frac -= dig;
            }
            break;
        }
        case '%': {
            *out++ = '%';
            break;
        }
        default:
            *out++ = *p;
            break;
        }
        if (*p) p++;
    }
    *out = '\0';
    return (int)(out - buf);
}
int __cdecl __stdio_common_vsprintf_s(unsigned __int64 options, char* buf, size_t max_len, const char* format, void* locale, va_list valist) {
    return __stdio_common_vsprintf(options, buf, max_len, format, locale, valist);
}
EXCEPTION_DISPOSITION __C_specific_handler(
    struct _EXCEPTION_RECORD* ExceptionRecord,
    void* EstablisherFrame,
    struct _CONTEXT* ContextRecord,
    struct _DISPATCHER_CONTEXT* DispatcherContext
) {
    typedef EXCEPTION_DISPOSITION(*pfn_handler)(void*, void*, void*, void*);
    static pfn_handler real_handler = NULL;
    if (!real_handler) {
        real_handler = (pfn_handler)GetProcAddress(GetModuleHandleA("ntdll.dll"), "__C_specific_handler");
    }
    return real_handler(ExceptionRecord, EstablisherFrame, ContextRecord, DispatcherContext);
}