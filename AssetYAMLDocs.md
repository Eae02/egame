## Texture Asset Settings
|Name|Default Value|Allowed Values|
|-|-|-|
|`format`|`rgba8`|`r8`, `rgba8`, `bc1`, `bc3`, `bc4`, `bc5`|
|`srgb`|`false`|`true`, `false`|
|`filtering`|`linear`|`linear`, `nearest`|
|`dither`|`false`|`true`, `false`|
|`enableAnistropy`|`true`|`true`, `false`|
|`useGlobalDownscale`|`false`|`true`, `false`|
|`width`|*Optional*|Integer > 0|
|`height`|*Optional*|Integer > 0|
|`mipLevels`|*Optional*|Integer > 0|
|`cubeMap` *(only for Texture2DArray)*|`false`|`true`, `false`|
|`3d` *(only for Texture2DArray)*|`false`|`true`, `false`|

## OBJ Asset Settings
|Name|Default Value|Allowed Values|
|-|-|-|
|`access`|`gpu`|`gpu`, `cpu`, `all`|
|`removeNameSuffix`|`false`|`true`, `false`|
|`flipWinding`|`false`|`true`, `false`|

## Particle Emitter Asset Format

|Name|Default Value|Description|
|-|-|-|
|`emissionRate`               |**Required**|Number of particles to emit per second.|
|`lifeTime` *[min,max]*       |**Required**|Life time in seconds.|
|`position`                   |`(0,0,0)`   |Position generator for new particles.|
|`velocity`                   |`(0,0,0)`   |Velocity generator for new particles.|
|`rotation` *[min,max]*       |`0`         |Initial rotation angle in degrees.|
|`angularVelocity` *[min,max]*|`0`         |Angular velocity in degrees / second.|
|`opacity` *[min,max]*        |`1`         |Initial opacity, opacity <0 means transparent, >1 means opaque.|
|`endOpacity` *[min,max]*     |`1`         |Final opacity factor (the opacity of a particle at the end of its life is `opacity * endOpacity`).|
|`size` *[min,max]*           |`1`         |Initial size (radius of the particle in world space units).|
|`endSize` *[min,max]*        |`1`         |Final size factor (the size of a particle at the end of its life is `size * endSize`).|
|`blend`                      |`alpha`     |Particle blend mode. Valid values are alpha and additive.|
|`gravity`                    |`true`      |Whether or not the particle should be affected by gravity.|
