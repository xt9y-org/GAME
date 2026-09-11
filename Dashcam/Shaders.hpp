#ifndef GAME_DASHCAM_SHADERS_HPP
#define GAME_DASHCAM_SHADERS_HPP

namespace Dashcam::Shaders {

inline constexpr const char *vertex = R"GLSL(
#version 120
varying vec2 vUv;
void main()
{
    gl_Position = vec4(gl_Vertex.xy, 0.0, 1.0);
    vUv = gl_Vertex.xy * 0.5 + 0.5;
}
)GLSL";

inline constexpr const char *lens_gl = R"GLSL(
#version 120
uniform sampler2D uSource;
uniform vec4 uP0;
uniform vec4 uP1;
uniform vec4 uP2;
uniform vec4 uP3;
varying vec2 vUv;

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}

void main()
{
    vec2 centered = vUv * 2.0 - 1.0;
    float radius2 = dot(centered, centered);
    centered *= 1.0 + uP0.x * radius2;
    vec2 uv = centered * 0.5 + 0.5;

    float scan = uv.y - 0.5;
    uv.x += scan * uP0.w * uP1.z;
    float jello = sin(uv.y * 33.0 + uP2.x * 41.0) * uP0.w * uP1.w * 0.025;
    uv.x += jello;

    vec2 radial = normalize(centered + vec2(1.0e-5));
    vec2 chroma = radial * uP0.y * (0.25 + radius2);
    vec2 safe_uv = clamp(uv, vec2(0.001), vec2(0.999));
    float r = texture2D(uSource, clamp(safe_uv + chroma, vec2(0.001), vec2(0.999))).r;
    float g = texture2D(uSource, safe_uv).g;
    float b = texture2D(uSource, clamp(safe_uv - chroma, vec2(0.001), vec2(0.999))).b;
    vec3 color = vec3(r, g, b);

    float edge = smoothstep(0.32, 1.16, radius2);
    color *= 1.0 - edge * uP0.z;

    float dust = hash21(floor(vUv * vec2(91.0, 57.0)));
    float speck = smoothstep(0.965, 0.995, dust);
    float streak = pow(max(0.0, sin(vUv.x * 24.0 + sin(vUv.y * 7.0))), 18.0);
    float brightness = max(max(color.r, color.g), color.b);
    color *= 1.0 - (speck * 0.55 + streak * 0.12) * uP1.x * smoothstep(0.35, 1.1, brightness);

    float dash = smoothstep(0.68, 1.0, vUv.y) * (0.5 + 0.5 * sin(vUv.x * 31.0));
    float glass = smoothstep(0.58, 0.98, vUv.y) * 0.45 + dash * 0.20;
    color += vec3(0.055, 0.06, 0.05) * glass * uP1.y;

    float outside = step(0.0, -uv.x) + step(1.0, uv.x) + step(0.0, -uv.y) + step(1.0, uv.y);
    color *= 1.0 - clamp(outside, 0.0, 1.0);
    gl_FragColor = vec4(max(color, vec3(0.0)), 1.0);
}
)GLSL";

inline constexpr const char *sensor_gl = R"GLSL(
#version 120
uniform sampler2D uSource;
uniform vec4 uP0;
uniform vec4 uP1;
uniform vec4 uP2;
uniform vec4 uP3;
varying vec2 vUv;

float hash21(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    vec2 texel = max(uP2.yz, vec2(1.0e-6));
    vec3 center = max(texture2D(uSource, vUv).rgb, vec3(0.0));
    vec3 left = max(texture2D(uSource, clamp(vUv - vec2(texel.x * 2.0, 0.0), vec2(0.0), vec2(1.0))).rgb, vec3(0.0));
    vec3 right = max(texture2D(uSource, clamp(vUv + vec2(texel.x * 2.0, 0.0), vec2(0.0), vec2(1.0))).rgb, vec3(0.0));
    vec3 bleed = vec3(left.r, center.g, right.b);
    vec3 color = mix(center, bleed, clamp(uP1.w, 0.0, 1.0));

    color *= max(uP0.x, 0.0);
    color = max(color - vec3(max(uP0.y, 0.0)), vec3(0.0));
    float clip_level = max(uP0.z, 0.05);
    color = min(color, vec3(clip_level));
    color /= clip_level;
    color = smoothstep(vec3(0.0), vec3(1.0), color);

    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    color = mix(color, vec3(luma), clamp(uP0.w, 0.0, 1.0));
    color.g += uP1.x;
    color.r += uP1.y;
    color.g += uP1.y * 0.7;

    float dark = 1.0 - clamp(luma, 0.0, 1.0);
    float grain = hash21(gl_FragCoord.xy + vec2(uP2.x * 173.0, uP2.x * 91.0)) - 0.5;
    color += vec3(grain) * uP1.z * (0.30 + dark * 1.70);

    color = pow(clamp(color, vec3(0.0), vec3(1.0)), vec3(1.0 / 2.15));
    gl_FragColor = vec4(color, 1.0);
}
)GLSL";

inline constexpr const char *compression_gl = R"GLSL(
#version 120
uniform sampler2D uSource;
uniform vec4 uP0;
uniform vec4 uP1;
uniform vec4 uP2;
uniform vec4 uP3;
varying vec2 vUv;

float hash11(float p)
{
    return fract(sin(p * 127.1) * 43758.5453);
}

void main()
{
    float virtual_h = max(uP0.x, 120.0);
    float virtual_w = virtual_h * max(uP1.w, 0.25);
    vec2 pixel = vec2(1.0 / virtual_w, 1.0 / virtual_h);
    float motion = clamp(uP1.z * 0.025 + abs(uP2.z) * 0.015, 0.0, 1.0);
    float block = mix(1.0, 10.0, clamp(uP0.y + motion * 0.35, 0.0, 1.0));
    vec2 uv = (floor(vUv / (pixel * block)) + 0.5) * pixel * block;

    float force = max(uP1.y - 0.65, 0.0);
    float glitch = clamp(force * uP0.w, 0.0, 0.10);
    float band = floor(uv.y * 18.0 + uP1.x * 7.0);
    float jump = (hash11(band + floor(uP1.x * 12.0)) - 0.5) * glitch;
    if (hash11(band * 2.17 + floor(uP1.x * 9.0)) > 0.78) uv.x += jump;
    uv = clamp(uv, vec2(0.001), vec2(0.999));

    vec3 color = texture2D(uSource, uv).rgb;
    float levels = mix(255.0, 34.0, clamp(uP0.y + motion * 0.25, 0.0, 1.0));
    color = floor(color * levels + 0.5) / levels;

    float line = mod(floor(vUv.y * virtual_h), 2.0);
    color *= 1.0 - line * clamp(uP0.z, 0.0, 0.35);

    float smear = clamp(uP0.y * motion * 0.20, 0.0, 0.18);
    vec3 next = texture2D(uSource, clamp(uv + vec2(pixel.x * 2.0, 0.0), vec2(0.001), vec2(0.999))).rgb;
    color = mix(color, next, smear);
    gl_FragColor = vec4(clamp(color, vec3(0.0), vec3(1.0)), 1.0);
}
)GLSL";

inline constexpr const char *copy_gl = R"GLSL(
#version 120
uniform sampler2D uSource;
uniform vec4 uP0;
uniform vec4 uP1;
uniform vec4 uP2;
uniform vec4 uP3;
varying vec2 vUv;
void main() { gl_FragColor = texture2D(uSource, vUv); }
)GLSL";

inline constexpr const char *lens_metal = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Params { float4 p0; float4 p1; float4 p2; float4 p3; };
float hash21(float2 p) { p = fract(p * float2(123.34f,345.45f)); p += dot(p,p+34.345f); return fract(p.x*p.y); }
kernel void dashcam_stage(constant Params& p [[buffer(0)]], texture2d<float, access::sample> source [[texture(0)]], texture2d<float, access::write> target [[texture(1)]], sampler s [[sampler(0)]], uint2 id [[thread_position_in_grid]])
{
    uint2 size = uint2(target.get_width(), target.get_height()); if (id.x >= size.x || id.y >= size.y) return;
    float2 base = (float2(id)+0.5f)/float2(size); float2 centered = base*2.0f-1.0f; float r2=dot(centered,centered); centered*=1.0f+p.p0.x*r2; float2 uv=centered*0.5f+0.5f;
    uv.x += (uv.y-0.5f)*p.p0.w*p.p1.z; uv.x += sin(uv.y*33.0f+p.p2.x*41.0f)*p.p0.w*p.p1.w*0.025f;
    float2 radial=normalize(centered+float2(1.0e-5f)); float2 chroma=radial*p.p0.y*(0.25f+r2); float2 suv=clamp(uv,float2(0.001f),float2(0.999f));
    float r=source.sample(s,clamp(suv+chroma,float2(0.001f),float2(0.999f))).r; float g=source.sample(s,suv).g; float b=source.sample(s,clamp(suv-chroma,float2(0.001f),float2(0.999f))).b; float3 color=float3(r,g,b);
    float edge=smoothstep(0.32f,1.16f,r2); color*=1.0f-edge*p.p0.z; float dust=hash21(floor(base*float2(91.0f,57.0f))); float speck=smoothstep(0.965f,0.995f,dust); float streak=pow(max(0.0f,sin(base.x*24.0f+sin(base.y*7.0f))),18.0f); float bright=max(max(color.r,color.g),color.b); color*=1.0f-(speck*0.55f+streak*0.12f)*p.p1.x*smoothstep(0.35f,1.1f,bright);
    float dash=smoothstep(0.68f,1.0f,base.y)*(0.5f+0.5f*sin(base.x*31.0f)); float glass=smoothstep(0.58f,0.98f,base.y)*0.45f+dash*0.20f; color+=float3(0.055f,0.06f,0.05f)*glass*p.p1.y;
    bool outside=uv.x<0.0f||uv.x>1.0f||uv.y<0.0f||uv.y>1.0f; target.write(float4(outside?float3(0.0f):max(color,float3(0.0f)),1.0f),id);
}
)MSL";

inline constexpr const char *sensor_metal = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Params { float4 p0; float4 p1; float4 p2; float4 p3; };
float hash21(float2 q) { return fract(sin(dot(q,float2(12.9898f,78.233f)))*43758.5453f); }
kernel void dashcam_stage(constant Params& p [[buffer(0)]], texture2d<float, access::sample> source [[texture(0)]], texture2d<float, access::write> target [[texture(1)]], sampler s [[sampler(0)]], uint2 id [[thread_position_in_grid]])
{
    uint2 size=uint2(target.get_width(),target.get_height()); if(id.x>=size.x||id.y>=size.y)return; float2 uv=(float2(id)+0.5f)/float2(size); float2 texel=max(p.p2.yz,float2(1.0e-6f));
    float3 center=max(source.sample(s,uv).rgb,float3(0.0f)); float3 left=max(source.sample(s,clamp(uv-float2(texel.x*2.0f,0.0f),float2(0.0f),float2(1.0f))).rgb,float3(0.0f)); float3 right=max(source.sample(s,clamp(uv+float2(texel.x*2.0f,0.0f),float2(0.0f),float2(1.0f))).rgb,float3(0.0f)); float3 color=mix(center,float3(left.r,center.g,right.b),clamp(p.p1.w,0.0f,1.0f));
    color*=max(p.p0.x,0.0f); color=max(color-float3(max(p.p0.y,0.0f)),float3(0.0f)); float clip=max(p.p0.z,0.05f); color=min(color,float3(clip))/clip; color=smoothstep(float3(0.0f),float3(1.0f),color); float luma=dot(color,float3(0.299f,0.587f,0.114f)); color=mix(color,float3(luma),clamp(p.p0.w,0.0f,1.0f)); color.g+=p.p1.x; color.r+=p.p1.y; color.g+=p.p1.y*0.7f;
    float grain=hash21(float2(id)+float2(p.p2.x*173.0f,p.p2.x*91.0f))-0.5f; color+=float3(grain)*p.p1.z*(0.30f+(1.0f-clamp(luma,0.0f,1.0f))*1.70f); color=pow(clamp(color,float3(0.0f),float3(1.0f)),float3(1.0f/2.15f)); target.write(float4(color,1.0f),id);
}
)MSL";

inline constexpr const char *compression_metal = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Params { float4 p0; float4 p1; float4 p2; float4 p3; };
float hash11(float q){return fract(sin(q*127.1f)*43758.5453f);}
kernel void dashcam_stage(constant Params& p [[buffer(0)]], texture2d<float, access::sample> source [[texture(0)]], texture2d<float, access::write> target [[texture(1)]], sampler s [[sampler(0)]], uint2 id [[thread_position_in_grid]])
{
    uint2 size=uint2(target.get_width(),target.get_height()); if(id.x>=size.x||id.y>=size.y)return; float2 base=(float2(id)+0.5f)/float2(size); float vh=max(p.p0.x,120.0f), vw=vh*max(p.p1.w,0.25f); float2 pixel=float2(1.0f/vw,1.0f/vh); float motion=clamp(p.p1.z*0.025f+abs(p.p2.z)*0.015f,0.0f,1.0f); float block=mix(1.0f,10.0f,clamp(p.p0.y+motion*0.35f,0.0f,1.0f)); float2 uv=(floor(base/(pixel*block))+0.5f)*pixel*block;
    float force=max(p.p1.y-0.65f,0.0f), glitch=clamp(force*p.p0.w,0.0f,0.10f), band=floor(uv.y*18.0f+p.p1.x*7.0f), jump=(hash11(band+floor(p.p1.x*12.0f))-0.5f)*glitch; if(hash11(band*2.17f+floor(p.p1.x*9.0f))>0.78f)uv.x+=jump; uv=clamp(uv,float2(0.001f),float2(0.999f));
    float3 color=source.sample(s,uv).rgb; float levels=mix(255.0f,34.0f,clamp(p.p0.y+motion*0.25f,0.0f,1.0f)); color=floor(color*levels+0.5f)/levels; float line=fmod(floor(base.y*vh),2.0f); color*=1.0f-line*clamp(p.p0.z,0.0f,0.35f); float smear=clamp(p.p0.y*motion*0.20f,0.0f,0.18f); float3 next=source.sample(s,clamp(uv+float2(pixel.x*2.0f,0.0f),float2(0.001f),float2(0.999f))).rgb; color=mix(color,next,smear); target.write(float4(clamp(color,float3(0.0f),float3(1.0f)),1.0f),id);
}
)MSL";

inline constexpr const char *copy_metal = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Params { float4 p0; float4 p1; float4 p2; float4 p3; };
kernel void dashcam_stage(constant Params& p [[buffer(0)]], texture2d<float, access::sample> source [[texture(0)]], texture2d<float, access::write> target [[texture(1)]], sampler s [[sampler(0)]], uint2 id [[thread_position_in_grid]])
{
    uint2 size=uint2(target.get_width(),target.get_height()); if(id.x>=size.x||id.y>=size.y)return; float2 uv=(float2(id)+0.5f)/float2(size); target.write(source.sample(s,uv),id);
}
)MSL";

} // namespace Dashcam::Shaders

#endif
