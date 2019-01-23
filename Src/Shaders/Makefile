SC=glslangValidator
FLAGS=-V

SHADERS=Sprite.fs Sprite.vs

all: $(addsuffix .h, $(SHADERS))

%.vs.h : %.vs.glsl
	$(SC) $(FLAGS) --vn $(subst .,_,$<) -S vert $< -o $@

%.fs.h : %.fs.glsl
	$(SC) $(FLAGS) --vn $(subst .,_,$<) -S frag $< -o $@
