SC=glslangValidator
FLAGS=-V -l

SHADERS=Sprite.fs Sprite.vs Gizmo.fs Gizmo.vs BRDFIntegration.cs SPFMapGenerator.cs IrradianceMapGenerator.cs

all: $(addsuffix .h, $(SHADERS))

%.vs.h : %.vs.glsl
	$(SC) $(FLAGS) --vn $(subst .,_,$<) -S vert $< -o $@

%.fs.h : %.fs.glsl
	$(SC) $(FLAGS) --vn $(subst .,_,$<) -S frag $< -o $@

%.cs.h : %.cs.glsl
	$(SC) $(FLAGS) --vn $(subst .,_,$<) -S comp $< -o $@
