#!/bin/bash
mkdir -p Src/Shaders/Build
make -C Src/Shaders/
pushd .
cd Assets
xxd -i DevFont.fnt > DevFont.fnt.h
xxd -i DevFont.png > DevFont.png.h
popd
