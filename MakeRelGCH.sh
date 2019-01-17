#!/bin/bash
g++ -std=gnu++17 -g -pedantic -fPIC -DGLM_FORCE_RADIANS -DGLM_FORCE_CTOR_INIT -DGLM_ENABLE_EXPERIMENTAL -isystem Deps/glm Src/EGame/EG.hpp -o Src/EGame/EG.hpp.gch
