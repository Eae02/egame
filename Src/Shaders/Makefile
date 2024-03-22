SC=glslangValidator
FLAGS=-V -l

SHADERS=Sprite.fs Sprite.vs Gizmo.fs Gizmo.vs Bloom.vs BloomBrightPass.fs BloomBlurX.fs BloomBlurY.fs \
Fullscreen_TCFlip.vs Fullscreen_TCNoFlip.vs Fullscreen_TCNone.vs ImGui.fs ImGui.vs Inc/Deferred.glh Inc/EGame.glh

all: $(addprefix Build/, $(addsuffix .h, $(SHADERS)))

Build:
	mkdir -p Build
Build/Inc:
	mkdir -p Build/Inc

Build/%.vs.h : %.vs.glsl | Build
	$(SC) $(FLAGS) --vn $(subst .,_,$<) -S vert $< -o $@

Build/%.fs.h : %.fs.glsl | Build
	$(SC) $(FLAGS) --vn $(subst .,_,$<) -S frag $< -o $@

Build/%.cs.h : %.cs.glsl | Build
	$(SC) $(FLAGS) --vn $(subst .,_,$<) -S comp $< -o $@

Build/Inc/%.glh.h : Inc/%.glh | Build/Inc
	xxd -i $< > $@
