#include "lib/sdf.glsl"

uniform vec4 color;
uniform vec2 dimensions;
uniform float paramf;
uniform float dt;

in vec2 pos;
out vec4 color_out;

void main(void) {
   vec2 uv = pos*dimensions;
   float m = 1.0 / dimensions.x;
   float d = sdBox( uv, dimensions-vec2(1.0) );

   uv.y  = abs(uv.y);
   uv.x -= dt*dimensions.y*0.5;
   uv.x  = mod(-uv.x,dimensions.y)-0.25*dimensions.y;
   float ds = -0.2*abs(uv.x-0.5*uv.y) + 2.0/3.0;
   d = max( d, ds );

   float alpha = smoothstep(-1.0, 0.0, -d);
   color_out   = color;
   color_out.a*= alpha;
   color_out.a*= smoothstep(dimensions.x, dimensions.x-paramf, length(uv));
}
