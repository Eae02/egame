SC=glslangValidator
FLAGS=-V -l

SHADERS=Sprite.fs Sprite.vs Gizmo.fs Gizmo.vs BloomBlur.cs BloomBrightPass.cs BloomUpscale.cs \
BRDFIntegration.cs SPFMapGenerator.cs IrradianceMapGenerator.cs

all: $(addprefix Build/, $(addsuffix .h, $(SHADERS)))

Build/%.vs.h : %.vs.glsl
	$(SC) $(FLAGS) --vn $(subst .,_,$<) -S vert $< -o $@

Build/%.fs.h : %.fs.glsl
	$(SC) $(FLAGS) --vn $(subst .,_,$<) -S frag $< -o $@

Build/%.cs.h : %.cs.glsl
	$(SC) $(FLAGS) --vn $(subst .,_,$<) -S comp $< -o $@
