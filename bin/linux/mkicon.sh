#!/bin/bash
MYDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" > /dev/null && pwd )"
echo "[Desktop Entry]
Name=BBC BASIC
Comment=BBC BASIC for Linux
Icon=$MYDIR/bbc256x.png
Exec=\"$MYDIR/bbcsdl\"
Type=Application
Encoding=UTF-8
Terminal=false
Categories=None;" > "$HOME/Desktop/bbcsdl.desktop"
chmod +x "$HOME/Desktop/bbcsdl.desktop"

