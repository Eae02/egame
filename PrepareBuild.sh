#!/bin/bash
pushd .
cd Assets
xxd -i DevFont.fnt > DevFont.fnt.h
xxd -i DevFont.png > DevFont.png.h
xxd -i Loading.png > Loading.png.h
popd
