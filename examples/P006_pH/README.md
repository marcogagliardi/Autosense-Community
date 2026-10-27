# P006 - pH probe with two-point calibration (analog)

The reference for a plugin **with a calibration**. This is the plugin the AutoSense firmware ships.
It needs no library, so it is also the quickest way to see the harness working
(an unconnected analog pin just reads noise).

What it shows:
- An analog pin that must be ADC1 on ESP32 (`.adc1()`), settings grouped under a heading (`.group("Calibration")`).
- Importing a value stored by firmware 1.x (`.legacy("A_index", ...)`).
- Temperature compensation with another plugin's value (`ctx.latest("liquid_temperature")`).
- A calibration described once (`d.calibration(...)`) that becomes the portal wizard, the
  `/api/cal/*` endpoints and the MQTT hub capability `calibrate_ph`:
  - `.live("voltage").stable(5, 0.005f)`: the value the user watches and when it counts as stable
  - two `.step(...)`s (low and high buffer) with their hub labels
  - `.field(...)`s: the status values shown in the wizard and published to the hub
- The four hooks: `calibrationStart` (reset), `calibrationTick` (sample about once a second),
  `calibrationCapture` (store a point), `calibrationFinish` (validate, `ctx.setNum` + `ctx.save`).

Try the calibration in the harness:

```
cal P006
capture        (when the value says STABLE; repeat for the second buffer)
save
```

No `lib_deps` needed.
