# Sunrise & Sunset

Desktop app (SDL3 + Dear ImGui) that shows sunrise and sunset times for a
chosen city across a whole year, as a daylight chart and as a day-by-day table.

![sunrise-sunset - view](assets/Screen.png)

- Solar times are computed with the NOAA solar-position algorithm (accuracy
  about ±1–2 minutes) in UTC.
- Conversion to the city's wall clock goes through the system tz database
  (IANA zones), so daylight-saving time — including southern-hemisphere rules
  and historical rule changes — is always correct for the selected year.
- Clock-change days are marked on the chart (dashed lines) and highlighted in
  the table; polar day/night at high latitudes is handled.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/sunset
```

## Usage

- Pick a city (type in the combo to filter) and a year (1900–2100).
- **Chart** tab: daylight band with sunrise/sunset curves (local wall-clock
  time) and a day-length chart below. Hover for exact values per day.
- Three configurable period bands (pre-work, lunch, afternoon) overlay the
  chart to show which parts of those periods have daylight through the year.
  Toggle and adjust them above the chart; values persist in `config.ini`
  (created next to the working directory on first run, editable by hand).
- **Table** tab: every day with sunrise, sunset, solar noon, day length,
  change vs the previous day, and UTC offset; filter by month.
- `./build/sunset --selftest [year]` prints solstice values and clock-change
  dates for a few cities, for checking against published almanac data.
