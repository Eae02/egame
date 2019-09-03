#!/bin/bash
pushd .
cd Assets
xxd -i DevFont.fnt > DevFont.fnt.h
xxd -i DevFont.png > DevFont.png.h
popd
