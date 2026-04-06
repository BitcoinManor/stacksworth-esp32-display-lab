# STACKSWORTH ESP32 Display Lab

A working collection of ESP32 + LCD display tests, drivers, and UI experiments used in STACKSWORTH hardware development.

This repository is designed to eliminate guesswork when working with displays and to provide a reusable foundation for all STACKSWORTH devices and Bitcoin-based hardware builds.

---

## 🎯 Purpose

Instead of rebuilding display setups from scratch every time, this repo serves as a central lab for:

* Rapid display bring-up (SPI / TFT / RGB)
* Pin mapping validation
* Hardware compatibility testing
* UI prototyping
* QR code rendering
* Bitcoin payment interfaces (LNbits / Bitcoin Switch)

---

## 🧪 Current Focus

* ST7796 4.0" SPI Display (480x320)
* ESP32-based display testing
* Basic rendering (colors, text, layout)

---

## ⚡ Getting Started

1. Navigate to a display folder:

   ```
   /displays/<display_name>/
   ```

2. Review wiring:

   ```
   wiring.md
   ```

3. Open the test sketch:

   ```
   <display_name>_test.ino
   ```

4. Upload to ESP32 and verify output

---

## 🧱 Repository Structure

```
stacksworth-esp32-display-lab/
│
├── README.md
│
├── displays/
│   ├── ST7796_4inch_SPI/
│   │   ├── ST7796_Color_Test/
│   │   │   └── ST7796_Color_Test.ino
│   │   ├── wiring.md
│   │   ├── notes.md
│   │
│   ├── ILI9341/
│   ├── ST7735/
│
├── common/
│   ├── pinouts/
│   ├── test_patterns/
│   ├── qr_display/
│
├── experiments/
│   ├── touch_tests/
│   ├── animations/
│   ├── bitcoin_ui/
│
└── assets/
    ├── images/
    ├── icons/
```

---

## 🧠 Philosophy

Build simple → prove it works → reuse forever.

No guessing. No rework.

---

## 🚀 Roadmap

* [ ] Basic color test (display bring-up)
* [ ] Text rendering
* [ ] Rotation + orientation testing
* [ ] QR code display
* [ ] Touch input testing
* [ ] LNbits / Bitcoin Switch UI integration
* [ ] NFC tap support
* [ ] Multi-product vending interface

---

## ⚡ STACKSWORTH

Where data comes to life.

This repository is part of the STACKSWORTH ecosystem of Bitcoin hardware, including:

* Matrix (LED display)
* Spark (touchscreen dashboard)
* Bitcoin vending systems
* NFC-enabled devices

---

## 🛠️ Notes

* Each display may require different drivers or libraries
* Pin mappings are defined inside test sketches whenever possible
* Working configurations should always be documented in `notes.md`

---

## 🤝 Contributing

Internal development repo for STACKSWORTH hardware experimentation.

Future contributions and collaborations welcome.

---

## 🔗 Bitcoin Manor

https://bitcoinmanor.com
