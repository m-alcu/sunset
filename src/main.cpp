// Sunrise & Sunset explorer
//
// Shows sunrise/sunset times for a chosen city across a whole year, as a
// daylight chart and as a table. Times are computed in UTC with the NOAA
// solar-position algorithm and converted to the city's wall clock through
// the system tz database, so daylight-saving time (including southern
// hemisphere rules and historical rule changes) comes from tzdata.

#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Palette (dark chart surface). Series colors validated for contrast and CVD
// separation against #1a1a19.
// ---------------------------------------------------------------------------
static inline ImU32 RGBA(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }

static const ImU32 COL_PAGE     = RGBA(0x0d, 0x0d, 0x0d);
static const ImU32 COL_SURFACE  = RGBA(0x1a, 0x1a, 0x19);
static const ImU32 COL_GRID     = RGBA(0x2c, 0x2c, 0x2a);
static const ImU32 COL_AXIS     = RGBA(0x38, 0x38, 0x35);
static const ImU32 COL_MUTED    = RGBA(0x89, 0x87, 0x81);
static const ImU32 COL_INK2     = RGBA(0xc3, 0xc2, 0xb7);
static const ImU32 COL_INK      = RGBA(0xff, 0xff, 0xff);
static const ImU32 COL_RISE     = RGBA(0xc9, 0x85, 0x00);   // sunrise line
static const ImU32 COL_SET      = RGBA(0x90, 0x85, 0xe9);   // sunset line
static const ImU32 COL_DAYFILL  = RGBA(0xc9, 0x85, 0x00, 42); // daylight wash
static const ImU32 COL_DAYLEN   = RGBA(0x39, 0x87, 0xe5);   // day-length line
static const ImU32 COL_DSTMARK  = RGBA(0x89, 0x87, 0x81, 170);
static const ImU32 COL_TODAY    = RGBA(0xc3, 0xc2, 0xb7, 130);
static const ImU32 COL_ROW_DST  = RGBA(0xc9, 0x85, 0x00, 34);
static const ImU32 COL_PREWORK  = RGBA(0x19, 0x9e, 0x70);   // pre-work band
static const ImU32 COL_LUNCH    = RGBA(0xd5, 0x51, 0x81);   // lunch band
static const ImU32 COL_AFTNOON  = RGBA(0x00, 0x83, 0x00);   // afternoon band

// ---------------------------------------------------------------------------
// Cities
// ---------------------------------------------------------------------------
struct City {
    const char* name;
    const char* country;
    double lat, lon;          // degrees, north/east positive
    const char* tz;           // IANA zone name
};

static const City kCities[] = {
    {"Amsterdam","Netherlands",52.3676,4.9041,"Europe/Amsterdam"},
    {"Anchorage","USA",61.2181,-149.9003,"America/Anchorage"},
    {"Athens","Greece",37.9838,23.7275,"Europe/Athens"},
    {"Auckland","New Zealand",-36.8485,174.7633,"Pacific/Auckland"},
    {"Bangkok","Thailand",13.7563,100.5018,"Asia/Bangkok"},
    {"Barcelona","Spain",41.3874,2.1686,"Europe/Madrid"},
    {"Beijing","China",39.9042,116.4074,"Asia/Shanghai"},
    {"Berlin","Germany",52.5200,13.4050,"Europe/Berlin"},
    {"Bogota","Colombia",4.7110,-74.0721,"America/Bogota"},
    {"Buenos Aires","Argentina",-34.6037,-58.3816,"America/Argentina/Buenos_Aires"},
    {"Cairo","Egypt",30.0444,31.2357,"Africa/Cairo"},
    {"Chicago","USA",41.8781,-87.6298,"America/Chicago"},
    {"Delhi","India",28.7041,77.1025,"Asia/Kolkata"},
    {"Denver","USA",39.7392,-104.9903,"America/Denver"},
    {"Dubai","UAE",25.2048,55.2708,"Asia/Dubai"},
    {"Dublin","Ireland",53.3498,-6.2603,"Europe/Dublin"},
    {"Helsinki","Finland",60.1699,24.9384,"Europe/Helsinki"},
    {"Hong Kong","China",22.3193,114.1694,"Asia/Hong_Kong"},
    {"Honolulu","USA",21.3069,-157.8583,"Pacific/Honolulu"},
    {"Istanbul","Turkiye",41.0082,28.9784,"Europe/Istanbul"},
    {"Johannesburg","South Africa",-26.2041,28.0473,"Africa/Johannesburg"},
    {"Lagos","Nigeria",6.5244,3.3792,"Africa/Lagos"},
    {"Las Palmas","Spain",28.1235,-15.4363,"Atlantic/Canary"},
    {"Lima","Peru",-12.0464,-77.0428,"America/Lima"},
    {"Lisbon","Portugal",38.7223,-9.1393,"Europe/Lisbon"},
    {"London","UK",51.5074,-0.1278,"Europe/London"},
    {"Los Angeles","USA",34.0522,-118.2437,"America/Los_Angeles"},
    {"Madrid","Spain",40.4169,-3.7033,"Europe/Madrid"},
    {"Melbourne","Australia",-37.8136,144.9631,"Australia/Melbourne"},
    {"Mexico City","Mexico",19.4326,-99.1332,"America/Mexico_City"},
    {"Moscow","Russia",55.7558,37.6173,"Europe/Moscow"},
    {"Nairobi","Kenya",-1.2921,36.8219,"Africa/Nairobi"},
    {"New York","USA",40.7128,-74.0060,"America/New_York"},
    {"Oslo","Norway",59.9139,10.7522,"Europe/Oslo"},
    {"Paris","France",48.8566,2.3522,"Europe/Paris"},
    {"Perth","Australia",-31.9505,115.8605,"Australia/Perth"},
    {"Reykjavik","Iceland",64.1466,-21.9426,"Atlantic/Reykjavik"},
    {"Rome","Italy",41.9028,12.4964,"Europe/Rome"},
    {"Santiago","Chile",-33.4489,-70.6693,"America/Santiago"},
    {"Sao Paulo","Brazil",-23.5505,-46.6333,"America/Sao_Paulo"},
    {"Seoul","South Korea",37.5665,126.9780,"Asia/Seoul"},
    {"Singapore","Singapore",1.3521,103.8198,"Asia/Singapore"},
    {"Stockholm","Sweden",59.3293,18.0686,"Europe/Stockholm"},
    {"Sydney","Australia",-33.8688,151.2093,"Australia/Sydney"},
    {"Tokyo","Japan",35.6762,139.6503,"Asia/Tokyo"},
    {"Toronto","Canada",43.6532,-79.3832,"America/Toronto"},
    {"Tromso","Norway",69.6492,18.9553,"Europe/Oslo"},
    {"Ushuaia","Argentina",-54.8019,-68.3030,"America/Argentina/Ushuaia"},
    {"Vancouver","Canada",49.2827,-123.1207,"America/Vancouver"},
    {"Vienna","Austria",48.2082,16.3738,"Europe/Vienna"},
    {"Warsaw","Poland",52.2297,21.0122,"Europe/Warsaw"},
    {"Zurich","Switzerland",47.3769,8.5417,"Europe/Zurich"},
};
static const int kNumCities = (int)(sizeof(kCities) / sizeof(kCities[0]));

static const char* kMonthNames[12] = {"Jan","Feb","Mar","Apr","May","Jun",
                                      "Jul","Aug","Sep","Oct","Nov","Dec"};
static const char* kDowNames[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

static bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static int daysInMonth(int y, int m) { // m: 1..12
    static const int d[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return (m == 2 && isLeap(y)) ? 29 : d[m - 1];
}

// ---------------------------------------------------------------------------
// NOAA solar position (accuracy ~1-2 min for |lat| < 72)
// ---------------------------------------------------------------------------
enum DayKind { DAY_NORMAL = 0, DAY_POLAR_DAY, DAY_POLAR_NIGHT };

struct SolarUTC {
    DayKind kind;
    double riseMin, setMin, noonMin; // minutes from UTC midnight of the date (may be <0 or >1440)
    double dayLenMin;
};

static SolarUTC solarCalc(double latDeg, double lonDeg, int year, int doy) {
    const double rad = M_PI / 180.0;
    double nDays = isLeap(year) ? 366.0 : 365.0;
    double g = 2.0 * M_PI / nDays * (doy - 1 + 0.5); // fractional year at 12:00

    double eqtime = 229.18 * (0.000075 + 0.001868 * cos(g) - 0.032077 * sin(g)
                              - 0.014615 * cos(2 * g) - 0.040849 * sin(2 * g));
    double decl = 0.006918 - 0.399912 * cos(g) + 0.070257 * sin(g)
                - 0.006758 * cos(2 * g) + 0.000907 * sin(2 * g)
                - 0.002697 * cos(3 * g) + 0.00148 * sin(3 * g);

    double lat = latDeg * rad;
    double cosHa = cos(90.833 * rad) / (cos(lat) * cos(decl)) - tan(lat) * tan(decl);

    SolarUTC r{};
    r.noonMin = 720.0 - 4.0 * lonDeg - eqtime;
    if (cosHa > 1.0) {
        r.kind = DAY_POLAR_NIGHT; r.dayLenMin = 0.0;
    } else if (cosHa < -1.0) {
        r.kind = DAY_POLAR_DAY; r.dayLenMin = 1440.0;
    } else {
        double haDeg = acos(cosHa) / rad;
        r.kind = DAY_NORMAL;
        r.riseMin = 720.0 - 4.0 * (lonDeg + haDeg) - eqtime;
        r.setMin  = 720.0 - 4.0 * (lonDeg - haDeg) - eqtime;
        r.dayLenMin = 8.0 * haDeg;
    }
    return r;
}

// ---------------------------------------------------------------------------
// Year dataset: UTC events converted to city wall-clock via the tz database
// ---------------------------------------------------------------------------
struct DayRecord {
    int month, day, dow;      // local calendar date, day of week
    DayKind kind;
    float riseMin, setMin;    // local wall-clock minutes of day [0,1440)
    float noonMin;
    float dayLenMin;
    int utcOffsetSec;         // at solar noon
    bool dst;
    int dstDeltaMin;          // != 0 on days where the clock changed vs previous day
};

struct YearData {
    int cityIdx = -1, year = 0;
    std::vector<DayRecord> days;
    int longestIdx = 0, shortestIdx = 0;
    std::vector<int> dstChangeDays;  // indices into days
    int stdOffsetSec = 0, dstOffsetSec = 0;
    bool hasDst = false;
};

static void setTZ(const char* tz) { setenv("TZ", tz, 1); tzset(); }

static float localMinutesOfDay(time_t t, int* offsetSec, bool* dst) {
    struct tm lt;
    localtime_r(&t, &lt);
    if (offsetSec) *offsetSec = (int)lt.tm_gmtoff;
    if (dst) *dst = lt.tm_isdst > 0;
    return (float)(lt.tm_hour * 60 + lt.tm_min) + (float)lt.tm_sec / 60.0f;
}

static void computeYear(YearData& yd, int cityIdx, int year) {
    const City& c = kCities[cityIdx];
    setTZ(c.tz);

    yd.cityIdx = cityIdx; yd.year = year;
    yd.days.clear(); yd.dstChangeDays.clear();
    yd.hasDst = false;

    int nDays = isLeap(year) ? 366 : 365;
    yd.days.reserve(nDays);

    struct tm tm0{};
    tm0.tm_year = year - 1900; tm0.tm_mon = 0; tm0.tm_mday = 1;
    time_t utcMidnight = timegm(&tm0);

    int doy = 1, prevOffset = 0;
    yd.stdOffsetSec = 0; yd.dstOffsetSec = 0;
    for (int m = 1; m <= 12; m++) {
        for (int d = 1; d <= daysInMonth(year, m); d++, doy++) {
            SolarUTC s = solarCalc(c.lat, c.lon, year, doy);
            DayRecord r{};
            r.month = m; r.day = d;
            r.kind = s.kind;
            r.dayLenMin = (float)s.dayLenMin;

            time_t noonT = utcMidnight + (time_t)llround(s.noonMin * 60.0);
            struct tm lt;
            localtime_r(&noonT, &lt);
            r.dow = lt.tm_wday;
            r.utcOffsetSec = (int)lt.tm_gmtoff;
            r.dst = lt.tm_isdst > 0;
            r.noonMin = (float)(lt.tm_hour * 60 + lt.tm_min) + (float)lt.tm_sec / 60.0f;

            if (s.kind == DAY_NORMAL) {
                r.riseMin = localMinutesOfDay(utcMidnight + (time_t)llround(s.riseMin * 60.0), nullptr, nullptr);
                r.setMin  = localMinutesOfDay(utcMidnight + (time_t)llround(s.setMin * 60.0), nullptr, nullptr);
            }

            if (doy > 1 && r.utcOffsetSec != prevOffset) {
                r.dstDeltaMin = (r.utcOffsetSec - prevOffset) / 60;
                yd.dstChangeDays.push_back((int)yd.days.size());
            }
            prevOffset = r.utcOffsetSec;

            if (r.dst) { yd.hasDst = true; yd.dstOffsetSec = r.utcOffsetSec; }
            else yd.stdOffsetSec = r.utcOffsetSec;

            yd.days.push_back(r);
            utcMidnight += 86400;
        }
    }
    if (!yd.hasDst) yd.dstOffsetSec = yd.stdOffsetSec;

    yd.longestIdx = yd.shortestIdx = 0;
    for (int i = 1; i < (int)yd.days.size(); i++) {
        if (yd.days[i].dayLenMin > yd.days[yd.longestIdx].dayLenMin) yd.longestIdx = i;
        if (yd.days[i].dayLenMin < yd.days[yd.shortestIdx].dayLenMin) yd.shortestIdx = i;
    }
}

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------
static void fmtHM(char* buf, size_t n, float minutes) {
    int m = (int)lround(minutes);
    m = ((m % 1440) + 1440) % 1440;
    snprintf(buf, n, "%02d:%02d", m / 60, m % 60);
}
static void fmtDur(char* buf, size_t n, float minutes) {
    int m = (int)lround(minutes);
    snprintf(buf, n, "%dh %02dm", m / 60, m % 60);
}
static void fmtOffset(char* buf, size_t n, int offsetSec) {
    int s = offsetSec, sign = s < 0 ? -1 : 1; s = abs(s);
    snprintf(buf, n, "UTC%c%02d:%02d", sign < 0 ? '-' : '+', s / 3600, (s % 3600) / 60);
}

// ---------------------------------------------------------------------------
// Chart drawing
// ---------------------------------------------------------------------------
struct PlotRect { float x0, y0, x1, y1; float w() const { return x1 - x0; } float h() const { return y1 - y0; } };

// user-defined wall-clock period overlaid on the daylight chart
struct Band { bool enabled; int fromMin, toMin; const char* name; const char* key; ImU32 col; };

static Band g_bands[3] = {
    {true, 7 * 60, 9 * 60, "Pre-work", "prework", COL_PREWORK},
    {true, 13 * 60, 15 * 60, "Lunch", "lunch", COL_LUNCH},
    {true, 17 * 60, 19 * 60, "Afternoon", "afternoon", COL_AFTNOON},
};
static const int kNumBands = 3;
static const char* kConfigPath = "config.ini";

static void saveConfig() {
    FILE* f = fopen(kConfigPath, "w");
    if (!f) return;
    fprintf(f, "# Sunrise & Sunset - period bands shown on the chart (times as HH:MM)\n[bands]\n");
    for (const Band& b : g_bands)
        fprintf(f, "%s_enabled=%d\n%s_from=%02d:%02d\n%s_to=%02d:%02d\n",
                b.key, b.enabled ? 1 : 0,
                b.key, b.fromMin / 60, b.fromMin % 60,
                b.key, b.toMin / 60, b.toMin % 60);
    fclose(f);
}

static void loadConfig() {
    FILE* f = fopen(kConfigPath, "r");
    if (!f) { saveConfig(); return; } // first run: write the defaults
    char line[128];
    while (fgets(line, sizeof line, f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char* val = eq + 1;
        val[strcspn(val, "\r\n")] = 0;
        for (Band& b : g_bands) {
            char k[48];
            int h, m;
            snprintf(k, sizeof k, "%s_enabled", b.key);
            if (!strcmp(line, k)) { b.enabled = atoi(val) != 0; break; }
            snprintf(k, sizeof k, "%s_from", b.key);
            if (!strcmp(line, k) && sscanf(val, "%d:%d", &h, &m) == 2) { b.fromMin = h * 60 + m; break; }
            snprintf(k, sizeof k, "%s_to", b.key);
            if (!strcmp(line, k) && sscanf(val, "%d:%d", &h, &m) == 2) { b.toMin = h * 60 + m; break; }
        }
    }
    fclose(f);
    for (Band& b : g_bands) {
        if (b.fromMin < 0) b.fromMin = 0;
        if (b.fromMin > 1425) b.fromMin = 1425;
        if (b.toMin > 1440) b.toMin = 1440;
        if (b.toMin <= b.fromMin) b.toMin = b.fromMin + 15;
    }
}

static void drawDashedV(ImDrawList* dl, float x, float y0, float y1, ImU32 col) {
    for (float y = y0; y < y1; y += 8.0f)
        dl->AddLine(ImVec2(x, y), ImVec2(x, fminf(y + 4.0f, y1)), col, 1.0f);
}

static void drawDashedH(ImDrawList* dl, float y, float x0, float x1, ImU32 col) {
    for (float x = x0; x < x1; x += 8.0f)
        dl->AddLine(ImVec2(x, y), ImVec2(fminf(x + 4.0f, x1), y), col, 1.0f);
}

// month grid + labels shared by both charts
static void drawMonthAxis(ImDrawList* dl, const PlotRect& p, int year, bool labels) {
    int nDays = isLeap(year) ? 366 : 365;
    int acc = 0;
    for (int m = 0; m < 12; m++) {
        float x = p.x0 + p.w() * acc / nDays;
        if (m > 0) dl->AddLine(ImVec2(x, p.y0), ImVec2(x, p.y1), COL_GRID, 1.0f);
        if (labels) {
            float xm = p.x0 + p.w() * (acc + daysInMonth(year, m + 1) * 0.5f) / nDays;
            ImVec2 ts = ImGui::CalcTextSize(kMonthNames[m]);
            dl->AddText(ImVec2(xm - ts.x * 0.5f, p.y1 + 4.0f), COL_MUTED, kMonthNames[m]);
        }
        acc += daysInMonth(year, m + 1);
    }
    dl->AddRect(ImVec2(p.x0, p.y0), ImVec2(p.x1, p.y1), COL_AXIS, 0.0f, 0, 1.0f);
}

static int dayIndexToday(const YearData& yd) {
    time_t now = time(nullptr);
    setTZ(kCities[yd.cityIdx].tz);
    struct tm lt; localtime_r(&now, &lt);
    if (lt.tm_year + 1900 != yd.year) return -1;
    return lt.tm_yday;
}

static void tooltipForDay(const YearData& yd, int i) {
    const DayRecord& r = yd.days[i];
    char rise[8], set[8], noon[8], len[16], off[16];
    fmtHM(rise, sizeof rise, r.riseMin); fmtHM(set, sizeof set, r.setMin);
    fmtHM(noon, sizeof noon, r.noonMin); fmtDur(len, sizeof len, r.dayLenMin);
    fmtOffset(off, sizeof off, r.utcOffsetSec);
    ImGui::BeginTooltip();
    ImGui::TextColored(ImColor(COL_INK), "%s %d %s %d", kDowNames[r.dow], r.day, kMonthNames[r.month - 1], yd.year);
    if (r.kind == DAY_POLAR_DAY) ImGui::TextColored(ImColor(COL_INK2), "Polar day - sun never sets");
    else if (r.kind == DAY_POLAR_NIGHT) ImGui::TextColored(ImColor(COL_INK2), "Polar night - sun never rises");
    else {
        ImGui::TextColored(ImColor(COL_RISE), "*"); ImGui::SameLine(0, 4);
        ImGui::TextColored(ImColor(COL_INK2), "Sunrise %s", rise); ImGui::SameLine(0, 14);
        ImGui::TextColored(ImColor(COL_SET), "*"); ImGui::SameLine(0, 4);
        ImGui::TextColored(ImColor(COL_INK2), "Sunset %s", set);
        ImGui::TextColored(ImColor(COL_INK2), "Solar noon %s   Daylight %s", noon, len);
    }
    ImGui::TextColored(ImColor(COL_MUTED), "%s%s", off, r.dst ? " (summer time)" : "");
    ImGui::EndTooltip();
}

static void drawDaylightChart(const YearData& yd, float height, const Band* bands, int nBands) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 org = ImGui::GetCursorScreenPos();
    float availW = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("daylight_chart", ImVec2(availW, height));
    bool hovered = ImGui::IsItemHovered();

    PlotRect p{org.x + 44, org.y + 8, org.x + availW - 70, org.y + height - 22};
    int nDays = (int)yd.days.size();
    auto xOf = [&](float dayIdx) { return p.x0 + p.w() * dayIdx / nDays; };

    // y-range fitted to the data, snapped to 3h gridlines (e.g. 06:00-24:00
    // for Madrid), expanding for polar day or daylight crossing midnight
    float loMin = 1440.0f, hiMin = 0.0f;
    for (const DayRecord& r : yd.days) {
        if (r.kind == DAY_POLAR_DAY || (r.kind == DAY_NORMAL && r.setMin < r.riseMin)) {
            loMin = 0.0f; hiMin = 1440.0f;
        } else if (r.kind == DAY_NORMAL) {
            loMin = fminf(loMin, r.riseMin); hiMin = fmaxf(hiMin, r.setMin);
        }
    }
    if (loMin > hiMin) { loMin = 0.0f; hiMin = 1440.0f; } // all polar night
    for (int b = 0; b < nBands; b++)
        if (bands[b].enabled) {
            loMin = fminf(loMin, (float)bands[b].fromMin);
            hiMin = fmaxf(hiMin, (float)bands[b].toMin);
        }
    loMin = fmaxf(0.0f, floorf(loMin / 180.0f) * 180.0f);
    hiMin = fminf(1440.0f, ceilf(hiMin / 180.0f) * 180.0f);
    float range = hiMin - loMin;
    auto yOf = [&](float min) { return p.y1 - p.h() * (min - loMin) / range; };

    dl->AddRectFilled(ImVec2(p.x0, p.y0), ImVec2(p.x1, p.y1), COL_SURFACE, 3.0f);

    // hour grid, every 3h
    char buf[32];
    for (int h = (int)loMin / 60; h <= (int)hiMin / 60; h += 3) {
        float y = yOf(h * 60.0f);
        if (h * 60 > loMin && h * 60 < hiMin) dl->AddLine(ImVec2(p.x0, y), ImVec2(p.x1, y), COL_GRID, 1.0f);
        snprintf(buf, sizeof buf, "%02d", h);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(p.x0 - ts.x - 8, y - ts.y * 0.5f), COL_MUTED, buf);
    }
    drawMonthAxis(dl, p, yd.year, true);

    // daylight fill, one thin column per day (handles polar gaps naturally)
    for (int i = 0; i < nDays; i++) {
        const DayRecord& r = yd.days[i];
        float x0 = xOf((float)i), x1 = xOf((float)i + 1.0f);
        if (r.kind == DAY_POLAR_DAY)
            dl->AddRectFilled(ImVec2(x0, p.y0), ImVec2(x1, p.y1), COL_DAYFILL);
        else if (r.kind == DAY_NORMAL) {
            if (r.setMin >= r.riseMin)
                dl->AddRectFilled(ImVec2(x0, yOf(r.setMin)), ImVec2(x1, yOf(r.riseMin)), COL_DAYFILL);
            else { // daylight crosses local midnight
                dl->AddRectFilled(ImVec2(x0, yOf(r.setMin)), ImVec2(x1, p.y1), COL_DAYFILL);
                dl->AddRectFilled(ImVec2(x0, p.y0), ImVec2(x1, yOf(r.riseMin)), COL_DAYFILL);
            }
        }
    }

    // user period bands (pre-work, lunch): translucent fill + dashed bounds
    for (int b = 0; b < nBands; b++) {
        if (!bands[b].enabled) continue;
        float yTop = yOf((float)bands[b].toMin), yBot = yOf((float)bands[b].fromMin);
        ImU32 fill = (bands[b].col & 0x00ffffff) | (28u << 24);
        dl->AddRectFilled(ImVec2(p.x0, yTop), ImVec2(p.x1, yBot), fill);
        drawDashedH(dl, yTop, p.x0, p.x1, bands[b].col);
        drawDashedH(dl, yBot, p.x0, p.x1, bands[b].col);
        dl->AddLine(ImVec2(p.x0 + 6, yTop + 4), ImVec2(p.x0 + 6, yBot - 4), bands[b].col, 3.0f);
        dl->AddText(ImVec2(p.x0 + 12, (yTop + yBot - ImGui::GetTextLineHeight()) * 0.5f),
                    COL_INK2, bands[b].name);
    }

    // sunrise / sunset polylines, broken at DST jumps and polar gaps
    auto drawSeries = [&](bool sunset, ImU32 col) {
        std::vector<ImVec2> pts;
        float prev = -1e9f;
        for (int i = 0; i <= nDays; i++) {
            bool brk = (i == nDays) || yd.days[i].kind != DAY_NORMAL;
            float v = 0;
            if (!brk) {
                v = sunset ? yd.days[i].setMin : yd.days[i].riseMin;
                if (fabsf(v - prev) > 45.0f && !pts.empty()) brk = true; // DST jump / midnight wrap
            }
            if (brk) {
                if (pts.size() >= 2) dl->AddPolyline(pts.data(), (int)pts.size(), col, 0, 2.0f);
                pts.clear();
                if (i < nDays && yd.days[i].kind == DAY_NORMAL) {
                    v = sunset ? yd.days[i].setMin : yd.days[i].riseMin;
                    pts.push_back(ImVec2(xOf(i + 0.5f), yOf(v)));
                }
                prev = (i < nDays && yd.days[i].kind == DAY_NORMAL) ? v : -1e9f;
                continue;
            }
            pts.push_back(ImVec2(xOf(i + 0.5f), yOf(v)));
            prev = v;
        }
        if (pts.size() >= 2) dl->AddPolyline(pts.data(), (int)pts.size(), col, 0, 2.0f);
    };
    drawSeries(false, COL_RISE);
    drawSeries(true, COL_SET);

    // DST transition markers
    for (int i : yd.dstChangeDays) {
        float x = xOf((float)i);
        drawDashedV(dl, x, p.y0, p.y1, COL_DSTMARK);
        snprintf(buf, sizeof buf, "%+dh", yd.days[i].dstDeltaMin / 60);
        dl->AddText(ImVec2(x + 3, p.y0 + 2), COL_MUTED, buf);
    }

    // today marker
    int today = dayIndexToday(yd);
    if (today >= 0 && today < nDays) {
        float x = xOf(today + 0.5f);
        dl->AddLine(ImVec2(x, p.y0), ImVec2(x, p.y1), COL_TODAY, 1.0f);
        dl->AddText(ImVec2(x + 3, p.y1 - ImGui::GetTextLineHeight() - 2), COL_INK2, "today");
    }

    // direct labels at right edge (text in ink tokens, colored tick carries identity)
    auto edgeLabel = [&](const char* txt, float vMin, ImU32 col) {
        float y = yOf(vMin);
        dl->AddLine(ImVec2(p.x1 + 3, y), ImVec2(p.x1 + 13, y), col, 3.0f);
        dl->AddText(ImVec2(p.x1 + 17, y - ImGui::GetTextLineHeight() * 0.5f), COL_INK2, txt);
    };
    for (int i = nDays - 1; i >= 0; i--)
        if (yd.days[i].kind == DAY_NORMAL) {
            edgeLabel("Sunrise", yd.days[i].riseMin, COL_RISE);
            edgeLabel("Sunset", yd.days[i].setMin, COL_SET);
            break;
        }

    // crosshair + tooltip
    if (hovered) {
        ImVec2 mp = ImGui::GetMousePos();
        if (mp.x >= p.x0 && mp.x <= p.x1 && mp.y >= p.y0 && mp.y <= p.y1) {
            int i = (int)((mp.x - p.x0) / p.w() * nDays);
            if (i < 0) i = 0; if (i >= nDays) i = nDays - 1;
            float x = xOf(i + 0.5f);
            dl->AddLine(ImVec2(x, p.y0), ImVec2(x, p.y1), RGBA(0xc3, 0xc2, 0xb7, 90), 1.0f);
            const DayRecord& r = yd.days[i];
            if (r.kind == DAY_NORMAL) {
                dl->AddCircleFilled(ImVec2(x, yOf(r.riseMin)), 4.0f, COL_RISE);
                dl->AddCircle(ImVec2(x, yOf(r.riseMin)), 5.0f, COL_SURFACE, 0, 2.0f);
                dl->AddCircleFilled(ImVec2(x, yOf(r.setMin)), 4.0f, COL_SET);
                dl->AddCircle(ImVec2(x, yOf(r.setMin)), 5.0f, COL_SURFACE, 0, 2.0f);
            }
            tooltipForDay(yd, i);
        }
    }
}

static void drawDayLengthChart(const YearData& yd, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 org = ImGui::GetCursorScreenPos();
    float availW = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("daylen_chart", ImVec2(availW, height));
    bool hovered = ImGui::IsItemHovered();

    PlotRect p{org.x + 44, org.y + 8, org.x + availW - 70, org.y + height - 22};
    int nDays = (int)yd.days.size();
    auto xOf = [&](float dayIdx) { return p.x0 + p.w() * dayIdx / nDays; };
    auto yOf = [&](float min) { return p.y1 - p.h() * min / 1440.0f; };

    dl->AddRectFilled(ImVec2(p.x0, p.y0), ImVec2(p.x1, p.y1), COL_SURFACE, 3.0f);
    char buf[32];
    for (int h = 0; h <= 24; h += 6) {
        float y = yOf(h * 60.0f);
        if (h > 0 && h < 24) dl->AddLine(ImVec2(p.x0, y), ImVec2(p.x1, y), COL_GRID, 1.0f);
        snprintf(buf, sizeof buf, "%02dh", h);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(p.x0 - ts.x - 8, y - ts.y * 0.5f), COL_MUTED, buf);
    }
    drawMonthAxis(dl, p, yd.year, false);

    std::vector<ImVec2> pts;
    pts.reserve(nDays);
    for (int i = 0; i < nDays; i++)
        pts.push_back(ImVec2(xOf(i + 0.5f), yOf(yd.days[i].dayLenMin)));
    dl->AddPolyline(pts.data(), (int)pts.size(), COL_DAYLEN, 0, 2.0f);

    dl->AddText(ImVec2(p.x0 + 6, p.y0 + 2), COL_INK2, "Day length");

    if (hovered) {
        ImVec2 mp = ImGui::GetMousePos();
        if (mp.x >= p.x0 && mp.x <= p.x1 && mp.y >= p.y0 && mp.y <= p.y1) {
            int i = (int)((mp.x - p.x0) / p.w() * nDays);
            if (i < 0) i = 0; if (i >= nDays) i = nDays - 1;
            const DayRecord& r = yd.days[i];
            float x = xOf(i + 0.5f);
            dl->AddLine(ImVec2(x, p.y0), ImVec2(x, p.y1), RGBA(0xc3, 0xc2, 0xb7, 90), 1.0f);
            dl->AddCircleFilled(ImVec2(x, yOf(r.dayLenMin)), 4.0f, COL_DAYLEN);
            char len[16]; fmtDur(len, sizeof len, r.dayLenMin);
            ImGui::SetTooltip("%d %s - %s", r.day, kMonthNames[r.month - 1], len);
        }
    }
}

// ---------------------------------------------------------------------------
// Table view
// ---------------------------------------------------------------------------
static void drawTable(const YearData& yd, int monthFilter) { // 0 = all, 1..12
    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
    if (!ImGui::BeginTable("suntable", 8, flags)) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableSetupColumn("Sunrise");
    ImGui::TableSetupColumn("Sunset");
    ImGui::TableSetupColumn("Solar noon");
    ImGui::TableSetupColumn("Day length");
    ImGui::TableSetupColumn("vs prev");
    ImGui::TableSetupColumn("UTC offset");
    ImGui::TableSetupColumn("Notes", ImGuiTableColumnFlags_WidthStretch, 1.6f);
    ImGui::TableHeadersRow();

    char rise[8], set[8], noon[8], len[16], off[16];
    for (int i = 0; i < (int)yd.days.size(); i++) {
        const DayRecord& r = yd.days[i];
        if (monthFilter && r.month != monthFilter) continue;
        ImGui::TableNextRow();
        if (r.dstDeltaMin != 0)
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, COL_ROW_DST);

        ImGui::TableNextColumn();
        ImGui::TextColored(ImColor(COL_INK2), "%s %02d %s", kDowNames[r.dow], r.day, kMonthNames[r.month - 1]);

        if (r.kind == DAY_NORMAL) {
            fmtHM(rise, sizeof rise, r.riseMin); fmtHM(set, sizeof set, r.setMin);
        } else { strcpy(rise, "-"); strcpy(set, "-"); }
        fmtHM(noon, sizeof noon, r.noonMin);
        fmtDur(len, sizeof len, r.dayLenMin);
        fmtOffset(off, sizeof off, r.utcOffsetSec);

        ImGui::TableNextColumn(); ImGui::TextUnformatted(rise);
        ImGui::TableNextColumn(); ImGui::TextUnformatted(set);
        ImGui::TableNextColumn(); ImGui::TextUnformatted(noon);
        ImGui::TableNextColumn(); ImGui::TextUnformatted(len);

        ImGui::TableNextColumn();
        if (i > 0) {
            float d = r.dayLenMin - yd.days[i - 1].dayLenMin;
            int sec = (int)lround(fabsf(d) * 60.0f);
            ImGui::TextColored(ImColor(COL_MUTED), "%c%d:%02d", d < 0 ? '-' : '+', sec / 60, sec % 60);
        } else ImGui::TextColored(ImColor(COL_MUTED), "-");

        ImGui::TableNextColumn();
        ImGui::Text("%s%s", off, r.dst ? " S" : "");

        ImGui::TableNextColumn();
        if (r.dstDeltaMin > 0) ImGui::TextColored(ImColor(COL_RISE), "Clocks +1h (summer time begins)");
        else if (r.dstDeltaMin < 0) ImGui::TextColored(ImColor(COL_RISE), "Clocks -1h (summer time ends)");
        else if (r.kind == DAY_POLAR_DAY) ImGui::TextColored(ImColor(COL_MUTED), "Polar day");
        else if (r.kind == DAY_POLAR_NIGHT) ImGui::TextColored(ImColor(COL_MUTED), "Polar night");
        else if (i == yd.longestIdx) ImGui::TextColored(ImColor(COL_MUTED), "Longest day");
        else if (i == yd.shortestIdx) ImGui::TextColored(ImColor(COL_MUTED), "Shortest day");
    }
    ImGui::EndTable();
}

// ---------------------------------------------------------------------------
// Header widgets + main UI
// ---------------------------------------------------------------------------
static bool cityCombo(int* cityIdx) {
    static ImGuiTextFilter filter;
    bool changed = false;
    const City& cur = kCities[*cityIdx];
    char preview[96];
    snprintf(preview, sizeof preview, "%s, %s", cur.name, cur.country);
    ImGui::SetNextItemWidth(260);
    if (ImGui::BeginCombo("##city", preview, ImGuiComboFlags_HeightLarge)) {
        if (ImGui::IsWindowAppearing()) { filter.Clear(); ImGui::SetKeyboardFocusHere(); }
        ImGui::SetNextItemWidth(-FLT_MIN);
        filter.Draw("##cityfilter");
        for (int i = 0; i < kNumCities; i++) {
            char label[96];
            snprintf(label, sizeof label, "%s, %s", kCities[i].name, kCities[i].country);
            if (!filter.PassFilter(label)) continue;
            if (ImGui::Selectable(label, i == *cityIdx)) { *cityIdx = i; changed = true; }
        }
        ImGui::EndCombo();
    }
    return changed;
}

// slider showing minutes-of-day as HH:MM, stepped to 15 min
static bool timeSlider(const char* id, int* minutes) {
    char fmt[8];
    fmtHM(fmt, sizeof fmt, (float)*minutes);
    ImGui::SetNextItemWidth(100);
    bool ch = ImGui::SliderInt(id, minutes, 0, 1440, fmt, ImGuiSliderFlags_AlwaysClamp);
    if (ch) *minutes = (*minutes / 15) * 15;
    return ch;
}

static bool bandControls(Band& b) {
    bool ch = ImGui::Checkbox(b.name, &b.enabled);
    ImGui::SameLine();
    ImGui::BeginDisabled(!b.enabled);
    char id[32];
    snprintf(id, sizeof id, "##%s_from", b.key);
    ch |= timeSlider(id, &b.fromMin);
    ImGui::SameLine(0, 4); ImGui::TextColored(ImColor(COL_MUTED), "-"); ImGui::SameLine(0, 4);
    snprintf(id, sizeof id, "##%s_to", b.key);
    ch |= timeSlider(id, &b.toMin);
    ImGui::EndDisabled();
    if (b.fromMin > 1425) b.fromMin = 1425;
    if (b.toMin <= b.fromMin) b.toMin = b.fromMin + 15;
    return ch;
}

static void drawUI(YearData& yd, int* cityIdx, int* year) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    bool dirty = (yd.cityIdx != *cityIdx || yd.year != *year);

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImColor(COL_INK2), "City"); ImGui::SameLine();
    dirty |= cityCombo(cityIdx);
    ImGui::SameLine(0, 18);
    ImGui::TextColored(ImColor(COL_INK2), "Year"); ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputInt("##year", year)) {
        if (*year < 1900) *year = 1900;
        if (*year > 2100) *year = 2100;
        dirty = true;
    }
    if (dirty) computeYear(yd, *cityIdx, *year);

    // info strip
    const City& c = kCities[*cityIdx];
    char off1[16], off2[16], b1[32], b2[32];
    fmtOffset(off1, sizeof off1, yd.stdOffsetSec);
    fmtOffset(off2, sizeof off2, yd.dstOffsetSec);
    ImGui::TextColored(ImColor(COL_MUTED), "%.4f%c %.4f%c   %s   standard %s%s%s%s",
        fabs(c.lat), c.lat < 0 ? 'S' : 'N', fabs(c.lon), c.lon < 0 ? 'W' : 'E',
        c.tz, off1,
        yd.hasDst ? "   summer " : "   no summer time in ",
        yd.hasDst ? off2 : std::to_string(yd.year).c_str(), "");

    // DST transitions + extremes
    if (!yd.dstChangeDays.empty()) {
        std::string s = "Clock changes: ";
        for (size_t k = 0; k < yd.dstChangeDays.size(); k++) {
            const DayRecord& r = yd.days[yd.dstChangeDays[k]];
            char seg[64];
            snprintf(seg, sizeof seg, "%s%d %s (%+dh)", k ? ", " : "",
                     r.day, kMonthNames[r.month - 1], r.dstDeltaMin / 60);
            s += seg;
        }
        ImGui::TextColored(ImColor(COL_MUTED), "%s", s.c_str());
    }
    {
        const DayRecord& lo = yd.days[yd.shortestIdx];
        const DayRecord& hi = yd.days[yd.longestIdx];
        fmtDur(b1, sizeof b1, hi.dayLenMin); fmtDur(b2, sizeof b2, lo.dayLenMin);
        ImGui::TextColored(ImColor(COL_MUTED), "Longest day %d %s (%s)   Shortest day %d %s (%s)",
            hi.day, kMonthNames[hi.month - 1], b1, lo.day, kMonthNames[lo.month - 1], b2);
    }

    // today stat line
    int today = dayIndexToday(yd);
    if (today >= 0) {
        const DayRecord& r = yd.days[today];
        if (r.kind == DAY_NORMAL) {
            char rise[8], set[8], len[16];
            fmtHM(rise, sizeof rise, r.riseMin); fmtHM(set, sizeof set, r.setMin);
            fmtDur(len, sizeof len, r.dayLenMin);
            ImGui::TextColored(ImColor(COL_INK), "Today, %s %d %s:", kDowNames[r.dow], r.day, kMonthNames[r.month - 1]);
            ImGui::SameLine();
            ImGui::TextColored(ImColor(COL_RISE), "|"); ImGui::SameLine(0, 4);
            ImGui::TextColored(ImColor(COL_INK), "sunrise %s", rise); ImGui::SameLine(0, 14);
            ImGui::TextColored(ImColor(COL_SET), "|"); ImGui::SameLine(0, 4);
            ImGui::TextColored(ImColor(COL_INK), "sunset %s", set); ImGui::SameLine(0, 14);
            ImGui::TextColored(ImColor(COL_INK2), "daylight %s", len);
        }
    }
    ImGui::Spacing();

    if (ImGui::BeginTabBar("views")) {
        if (ImGui::BeginTabItem("Chart")) {
            bool bandsChanged = false;
            for (int b = 0; b < kNumBands; b++) {
                if (b > 0) ImGui::SameLine(0, 24);
                bandsChanged |= bandControls(g_bands[b]);
            }
            if (bandsChanged) saveConfig(); // persist user values as the new defaults

            // legend
            auto legendDot = [](ImU32 col, const char* txt) {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 pos = ImGui::GetCursorScreenPos();
                float h = ImGui::GetTextLineHeight();
                dl->AddRectFilled(ImVec2(pos.x, pos.y + h * 0.5f - 2), ImVec2(pos.x + 12, pos.y + h * 0.5f + 2), col, 2.0f);
                ImGui::Dummy(ImVec2(16, h)); ImGui::SameLine();
                ImGui::TextColored(ImColor(COL_INK2), "%s", txt); ImGui::SameLine(0, 18);
            };
            legendDot(COL_RISE, "Sunrise");
            legendDot(COL_SET, "Sunset");
            legendDot(COL_DAYFILL, "Daylight");
            legendDot(COL_DSTMARK, "Clock change");
            for (const Band& b : g_bands)
                if (b.enabled) legendDot(b.col, b.name);
            ImGui::NewLine();

            float avail = ImGui::GetContentRegionAvail().y;
            float miniH = 140.0f;
            float mainH = avail - miniH - 8.0f;
            if (mainH < 220.0f) mainH = 220.0f;
            drawDaylightChart(yd, mainH, g_bands, kNumBands);
            ImGui::Spacing();
            drawDayLengthChart(yd, miniH);
            ImGui::EndTabItem();
        }
        static bool wantTableTab = []{ const char* t = getenv("SUNSET_TAB"); return t && strcmp(t, "table") == 0; }();
        ImGuiTabItemFlags tabFlags = wantTableTab ? ImGuiTabItemFlags_SetSelected : 0;
        wantTableTab = false;
        if (ImGui::BeginTabItem("Table", nullptr, tabFlags)) {
            static int monthFilter = 0;
            ImGui::SetNextItemWidth(140);
            const char* items[13] = {"All months","January","February","March","April","May","June",
                                     "July","August","September","October","November","December"};
            ImGui::Combo("##monthfilter", &monthFilter, items, 13);
            ImGui::SameLine();
            ImGui::TextColored(ImColor(COL_MUTED), "S = summer time in effect. Highlighted rows change the clock.");
            drawTable(yd, monthFilter);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Style
// ---------------------------------------------------------------------------
static void applyStyle() {
    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 0.0f;
    st.FrameRounding = 4.0f;
    st.TabRounding = 4.0f;
    st.WindowPadding = ImVec2(14, 12);
    ImVec4* c = st.Colors;
    auto v4 = [](ImU32 u, float aMul = 1.0f) {
        ImVec4 v = ImGui::ColorConvertU32ToFloat4(u); v.w *= aMul; return v;
    };
    c[ImGuiCol_WindowBg]        = v4(COL_PAGE);
    c[ImGuiCol_Text]            = v4(COL_INK);
    c[ImGuiCol_TextDisabled]    = v4(COL_MUTED);
    c[ImGuiCol_FrameBg]         = v4(COL_SURFACE);
    c[ImGuiCol_FrameBgHovered]  = v4(COL_GRID);
    c[ImGuiCol_FrameBgActive]   = v4(COL_AXIS);
    c[ImGuiCol_PopupBg]         = v4(COL_SURFACE);
    c[ImGuiCol_Border]          = v4(COL_AXIS);
    c[ImGuiCol_Button]          = v4(COL_SURFACE);
    c[ImGuiCol_ButtonHovered]   = v4(COL_GRID);
    c[ImGuiCol_ButtonActive]    = v4(COL_AXIS);
    c[ImGuiCol_Header]          = v4(RGBA(0x39, 0x87, 0xe5, 60));
    c[ImGuiCol_HeaderHovered]   = v4(RGBA(0x39, 0x87, 0xe5, 90));
    c[ImGuiCol_HeaderActive]    = v4(RGBA(0x39, 0x87, 0xe5, 120));
    c[ImGuiCol_Tab]             = v4(COL_SURFACE);
    c[ImGuiCol_TabHovered]      = v4(RGBA(0x39, 0x87, 0xe5, 90));
    c[ImGuiCol_TabSelected]     = v4(RGBA(0x39, 0x87, 0xe5, 120));
    c[ImGuiCol_TableHeaderBg]   = v4(COL_SURFACE);
    c[ImGuiCol_TableBorderLight]= v4(COL_GRID);
    c[ImGuiCol_TableBorderStrong]=v4(COL_AXIS);
    c[ImGuiCol_TableRowBg]      = v4(RGBA(0, 0, 0, 0));
    c[ImGuiCol_TableRowBgAlt]   = v4(RGBA(0x1a, 0x1a, 0x19, 130));
    c[ImGuiCol_CheckMark]       = v4(COL_DAYLEN);
    c[ImGuiCol_SliderGrab]      = v4(COL_DAYLEN);
    c[ImGuiCol_ScrollbarBg]     = v4(COL_PAGE);
}

// ---------------------------------------------------------------------------
// Self-test: print key dates for a few cities so results can be checked
// against published almanac values, without needing a display.
// ---------------------------------------------------------------------------
static int selfTest(int year) {
    const char* names[] = {"Madrid", "New York", "Sydney", "Tromso", "Singapore"};
    for (const char* nm : names) {
        int idx = -1;
        for (int i = 0; i < kNumCities; i++)
            if (strcmp(kCities[i].name, nm) == 0) { idx = i; break; }
        YearData yd;
        computeYear(yd, idx, year);
        printf("== %s (%s) %d ==\n", nm, kCities[idx].tz, year);
        for (int i : yd.dstChangeDays) {
            const DayRecord& r = yd.days[i];
            printf("  clock change %02d %s: %+d min\n", r.day, kMonthNames[r.month - 1], r.dstDeltaMin);
        }
        int checks[] = {yd.shortestIdx, yd.longestIdx, 0};
        // also mid-June and mid-December fixed dates
        int doy = 0, extra[2] = {0, 0};
        for (int m = 1, acc = 0; m <= 12; acc += daysInMonth(year, m), m++) {
            if (m == 6) extra[0] = acc + 20;   // Jun 21
            if (m == 12) extra[1] = acc + 20;  // Dec 21
        }
        (void)doy; (void)checks;
        for (int i : {extra[0], extra[1]}) {
            const DayRecord& r = yd.days[i];
            char rise[8], set[8], len[16], off[16];
            fmtOffset(off, sizeof off, r.utcOffsetSec);
            fmtDur(len, sizeof len, r.dayLenMin);
            if (r.kind == DAY_NORMAL) {
                fmtHM(rise, sizeof rise, r.riseMin); fmtHM(set, sizeof set, r.setMin);
                printf("  %02d %s: rise %s  set %s  len %s  %s%s\n", r.day,
                       kMonthNames[r.month - 1], rise, set, len, off, r.dst ? " DST" : "");
            } else {
                printf("  %02d %s: %s  %s\n", r.day, kMonthNames[r.month - 1],
                       r.kind == DAY_POLAR_DAY ? "polar day" : "polar night", off);
            }
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    int year;
    {
        time_t now = time(nullptr);
        struct tm lt; localtime_r(&now, &lt);
        year = lt.tm_year + 1900;
    }
    if (argc > 1 && strcmp(argv[1], "--selftest") == 0)
        return selfTest(argc > 2 ? atoi(argv[2]) : year);

    loadConfig();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("Sunrise & Sunset", 1280, 800,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!window || !renderer) {
        fprintf(stderr, "SDL window/renderer failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    float scale = SDL_GetWindowDisplayScale(window);
    if (scale <= 0.0f) scale = 1.0f;
    const char* fontCandidates[] = {
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    };
    for (const char* f : fontCandidates) {
        FILE* fp = fopen(f, "rb");
        if (fp) { fclose(fp); io.Fonts->AddFontFromFileTTF(f, 17.0f * scale); break; }
    }
    applyStyle();
    ImGui::GetStyle().ScaleAllSizes(scale);

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    int cityIdx = 0;
    for (int i = 0; i < kNumCities; i++)
        if (strcmp(kCities[i].name, "Madrid") == 0) { cityIdx = i; break; }
    YearData yd;
    computeYear(yd, cityIdx, year);

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT) running = false;
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) { SDL_Delay(50); continue; }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        drawUI(yd, &cityIdx, &year);
        ImGui::Render();

        ImVec4 bg = ImGui::ColorConvertU32ToFloat4(COL_PAGE);
        SDL_SetRenderDrawColorFloat(renderer, bg.x, bg.y, bg.z, 1.0f);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
