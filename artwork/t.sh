#!/bin/bash
# 1. Create the iconset folder
mkdir ld.iconset

# 2. Generate every required size from your 1024x1024 source
sips -z 16 16     level_designer.png --out ld.iconset/icon_16x16.png
sips -z 32 32     level_designer.png --out ld.iconset/icon_16x16@2x.png
sips -z 32 32     level_designer.png --out ld.iconset/icon_32x32.png
sips -z 64 64     level_designer.png --out ld.iconset/icon_32x32@2x.png
sips -z 128 128   level_designer.png --out ld.iconset/icon_128x128.png
sips -z 256 256   level_designer.png --out ld.iconset/icon_128x128@2x.png
sips -z 256 256   level_designer.png --out ld.iconset/icon_256x256.png
sips -z 512 512   level_designer.png --out ld.iconset/icon_256x256@2x.png
sips -z 512 512   level_designer.png --out ld.iconset/icon_512x512.png
cp level_designer.png ld.iconset/icon_512x512@2x.png   # already 1024x1024

# 3. Now convert the iconset to .icns
iconutil --convert icns --output ld.icns ld.iconset
