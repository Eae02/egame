#!/bin/bash
g++ -std=gnu++17 -DGLM_FORCE_RADIANS -DGLM_FORCE_CTOR_INIT -DGLM_ENABLE_EXPERIMENTAL -isystem Deps/glm/glm -g -pedantic -fPIC Inc/Common.hpp -o Inc/Common.hpp.gch
