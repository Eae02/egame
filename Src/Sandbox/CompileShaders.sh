#!/bin/bash
glslangValidator Main.frag -G --vn SpvFS -o Main.frag.h
glslangValidator Main.vert -G --vn SpvVS -o Main.vert.h
