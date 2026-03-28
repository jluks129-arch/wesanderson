#!/usr/bin/env python3
"""
Convert PNG images to LVGL 8.x C image arrays (LV_IMG_CF_TRUE_COLOR_ALPHA).

Usage: python3 scripts/png_to_lvgl.py
Outputs one .c file per image into components/ui_app/
"""

from PIL import Image
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR   = os.path.dirname(SCRIPT_DIR)
OUT_DIR    = os.path.join(ROOT_DIR, "components", "ui_app")

# (source png, output variable name)
IMAGES = [
    ("vitez.png",      "ui_img_vitez"),
    ("carobnjak.png",  "ui_img_carobnjak"),
    ("robot.png",      "ui_img_robot"),
    ("vila.png",       "ui_img_vila"),
    ("zmaj.png",       "ui_img_zmaj"),
    ("pas.png",        "ui_img_pas"),
]

# Target size matches the button dimensions in ui_app.c (BW x BH)
TARGET_W = 145
TARGET_H = 95


def convert(src_path: str, var_name: str) -> None:
    img = Image.open(src_path).convert("RGBA")
    img = img.resize((TARGET_W, TARGET_H), Image.LANCZOS)

    data = []
    for r, g, b, a in img.getdata():
        rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        data.append((rgb565 >> 8) & 0xFF) # high byte first (LV_COLOR_16_SWAP=y)
        data.append(rgb565 & 0xFF)        # low byte
        data.append(a)                     # alpha

    array_name = f"{var_name}_data"
    out_path = os.path.join(OUT_DIR, f"{var_name}.c")

    with open(out_path, "w") as f:
        f.write('#include "lvgl.h"\n\n')
        f.write(f"static const uint8_t {array_name}[] = {{\n")
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            f.write("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",\n")
        f.write("};\n\n")
        f.write(f"const lv_img_dsc_t {var_name} = {{\n")
        f.write(f"    .header.always_zero = 0,\n")
        f.write(f"    .header.w           = {TARGET_W},\n")
        f.write(f"    .header.h           = {TARGET_H},\n")
        f.write(f"    .data_size          = sizeof({array_name}),\n")
        f.write(f"    .header.cf          = LV_IMG_CF_TRUE_COLOR_ALPHA,\n")
        f.write(f"    .data               = {array_name},\n")
        f.write("};\n")

    kb = len(data) / 1024
    print(f"  {var_name}.c  ({kb:.1f} KB)")


def main():
    print(f"Converting {len(IMAGES)} images → {OUT_DIR}")
    for png_name, var_name in IMAGES:
        src = os.path.join(ROOT_DIR, "images", png_name)
        if not os.path.exists(src):
            print(f"  WARNING: {png_name} not found, skipping")
            continue
        convert(src, var_name)
    print("Done.")


if __name__ == "__main__":
    main()
