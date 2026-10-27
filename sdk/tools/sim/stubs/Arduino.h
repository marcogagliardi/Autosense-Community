#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <stdarg.h>
typedef uint8_t byte;
typedef bool boolean;
#define PROGMEM
#define PSTR(s) (s)
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define HEX 16
#define DEC 10
#define INPUT 0
#define OUTPUT 1
#define HIGH 1
#define LOW 0
#ifdef ESP32
#define A0 36
#define A3 39
#else
#define A0 17
#endif
class __FlashStringHelper { char c; };
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))
#ifndef constrain
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#endif
inline void yield() {}
inline void noInterrupts() {}
inline void interrupts() {}
inline size_t strlen_P(const char *s) { return strlen(s); }
inline int strcmp_P(const char *a, const char *b) { return strcmp(a, b); }
inline int strncmp_P(const char *a, const char *b, size_t n) { return strncmp(a, b, n); }
inline void *memcpy_P(void *d, const void *s, size_t n) { return memcpy(d, s, n); }
inline uint32_t getCpuFrequencyMhz() { return 240; }
inline void delay(unsigned long) {}
extern unsigned long g_millis;
inline unsigned long millis() { return g_millis; }
inline unsigned long micros() { return 0; }
extern int g_adc;
inline int analogRead(int) { return g_adc; }
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return 0; }
inline char *dtostrf(double v, signed char w, unsigned char p, char *s) { snprintf(s, 32, "%*.*f", w, p, v); return s; }
class String {
public:
    std::string s;
    String() {}
    String(const char *c) { if (c) s = c; }
    String(const __FlashStringHelper *c) { if (c) s = (const char *)c; }
    String(const String &o) : s(o.s) {}
    String(char c) { s = std::string(1, c); }
    String(int v, int base = 10) { char b[40]; snprintf(b, 40, base == 16 ? "%x" : "%d", v); s = b; }
    String(unsigned v, int base = 10) { char b[40]; snprintf(b, 40, base == 16 ? "%x" : "%u", v); s = b; }
    String(long v, int base = 10) { char b[40]; snprintf(b, 40, base == 16 ? "%lx" : "%ld", v); s = b; }
    String(unsigned long v, int base = 10) { char b[40]; snprintf(b, 40, base == 16 ? "%lx" : "%lu", v); s = b; }
    String(float v, int d = 2) { char b[64]; snprintf(b, 64, "%.*f", d, v); s = b; }
    String(double v, int d = 2) { char b[64]; snprintf(b, 64, "%.*f", d, v); s = b; }
    const char *c_str() const { return s.c_str(); }
    size_t length() const { return s.size(); }
    bool reserve(size_t n) { s.reserve(n); return true; }
    char operator[](size_t i) const { return s[i]; }
    char &operator[](size_t i) { return s[i]; }
    String &operator=(const String &o) { s = o.s; return *this; }
    String &operator=(const char *c) { s = c ? c : ""; return *this; }
    String &operator+=(const String &o) { s += o.s; return *this; }
    String &operator+=(const char *c) { s += c; return *this; }
    String &operator+=(char c) { s += c; return *this; }
    String &operator+=(const __FlashStringHelper *c) { s += (const char *)c; return *this; }
    String operator+(const String &o) const { String r(*this); r.s += o.s; return r; }
    template <class T, class = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    String operator+(T v) const { String r(*this); r.s += String(v).s; return r; }
    String operator+(const char *o) const { String r(*this); r.s += o; return r; }
    friend String operator+(const char *a, const String &b) { String r(a); r.s += b.s; return r; }
    friend String operator+(const __FlashStringHelper *a, const String &b) { String r(a); r.s += b.s; return r; }
    bool operator==(const String &o) const { return s == o.s; }
    bool operator==(const char *o) const { return s == o; }
    bool operator!=(const String &o) const { return s != o.s; }
    bool operator!=(const char *o) const { return s != o; }
    bool operator<(const String &o) const { return s < o.s; }
    bool equals(const char *o) const { return s == o; }
    bool equalsIgnoreCase(const String &o) const { return strcasecmp(s.c_str(), o.s.c_str()) == 0; }
    bool startsWith(const String &p) const { return s.rfind(p.s, 0) == 0; }
    bool endsWith(const String &p) const { return s.size() >= p.s.size() && s.compare(s.size() - p.s.size(), p.s.size(), p.s) == 0; }
    int indexOf(char c, size_t from = 0) const { auto p = s.find(c, from); return p == std::string::npos ? -1 : (int)p; }
    int indexOf(const String &c, size_t from = 0) const { auto p = s.find(c.s, from); return p == std::string::npos ? -1 : (int)p; }
    int lastIndexOf(char c) const { auto p = s.rfind(c); return p == std::string::npos ? -1 : (int)p; }
    String substring(size_t a, size_t b = (size_t)-1) const { return String(s.substr(a, b == (size_t)-1 ? std::string::npos : b - a).c_str()); }
    void toLowerCase() { for (auto &c : s) c = tolower(c); }
    void toUpperCase() { for (auto &c : s) c = toupper(c); }
    void trim() { size_t a = s.find_first_not_of(" \t\r\n"); if (a == std::string::npos) { s.clear(); return; } size_t b = s.find_last_not_of(" \t\r\n"); s = s.substr(a, b - a + 1); }
    void replace(const String &a, const String &b) { size_t p = 0; while ((p = s.find(a.s, p)) != std::string::npos) { s.replace(p, a.s.size(), b.s); p += b.s.size(); } }
    void toCharArray(char *b, unsigned n) const { strncpy(b, s.c_str(), n); if (n) b[n-1] = 0; }
    long toInt() const { return atol(s.c_str()); }
    float toFloat() const { return (float)atof(s.c_str()); }
    double toDouble() const { return atof(s.c_str()); }
    bool isEmpty() const { return s.empty(); }
    void remove(unsigned idx, unsigned cnt = 0xFFFFFFFF) { if (idx < s.size()) s.erase(idx, cnt); }
    bool concat(const char *c, unsigned n) { s.append(c, n); return true; }
    bool concat(char c) { s += c; return true; }
    bool concat(const char *c) { s += c; return true; }
    bool concat(const String &c) { s += c.s; return true; }
};
extern bool g_serialEcho;
class Print;
class Printable { public: virtual size_t printTo(Print &) const = 0; virtual ~Printable() {} };
class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *b, size_t n) { size_t k = 0; while (n--) k += write(*b++); return k; }
    size_t write(const char *s) { return write((const uint8_t *)s, strlen(s)); }
    size_t print(const String &s) { return write(s.c_str()); }
    size_t print(const char *s) { return write(s); }
    size_t print(const Printable &p) { return p.printTo(*this); }
    size_t print(char c) { return write((uint8_t)c); }
    size_t print(int v, int = 10) { return write(String(v).c_str()); }
    size_t print(unsigned v, int = 10) { return write(String(v).c_str()); }
    size_t print(long v, int = 10) { return write(String(v).c_str()); }
    size_t print(unsigned long v, int = 10) { return write(String(v).c_str()); }
    size_t print(double v, int d = 2) { return write(String(v, d).c_str()); }
    size_t print(const __FlashStringHelper *s) { return write((const char *)s); }
    template <class T> size_t println(T v) { return print(v) + write((uint8_t)'\n'); }
    size_t println(double v, int d) { return print(v, d) + write((uint8_t)'\n'); }
    size_t println(int v, int b) { return print(v, b) + write((uint8_t)'\n'); }
    size_t println() { return write((uint8_t)'\n'); }
    size_t printf(const char *f, ...) __attribute__((format(printf, 2, 3))) { va_list ap; va_start(ap, f); size_t n = vprintf(f, ap); va_end(ap); return n; }
};

class Stream : public Print {
public:
    virtual int available() = 0; virtual int read() = 0; virtual int peek() = 0;
    size_t readBytes(char *, size_t) { return 0; }
};
class HardwareSerial : public Stream {
public:
    void begin(unsigned long) {}
    size_t write(uint8_t c) override { if (g_serialEcho) putchar(c); return 1; }
    int available() override { return 0; } int read() override { return -1; } int peek() override { return -1; }
};
extern HardwareSerial Serial;
#include <ESP.h>
