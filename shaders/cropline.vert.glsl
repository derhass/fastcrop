#version 450 core

layout(std140, binding=0) uniform windowStateUBO
{
	vec4 cropLineColor;
	uvec2 dims;
} windowState;

layout(std140, binding=1) uniform displayStateUBO
{
	ivec2 imgDims;
	vec2 scale;
	vec2 offset;
} displayState;

layout(std140, binding=2) uniform cropStateUBO
{
	ivec2 pos;
	ivec2 size;
} cropState;

void main()
{
	ivec2 sel[4] = ivec2[4](ivec2(0,0),ivec2(1,0),ivec2(1,1),ivec2(0,1));
	ivec2 s = sel[gl_VertexID];

	vec2 imgScale = vec2(1.0) / vec2(displayState.imgDims);

	vec2 A = vec2(cropState.pos.x * imgScale.x, (cropState.pos.x + cropState.size.x) * imgScale.x);
	vec2 B = vec2(cropState.pos.y * imgScale.y, (cropState.pos.y + cropState.size.y) * imgScale.y);

	vec2 ndc = vec2(A[s.x], B[s.y]) * displayState.scale + displayState.offset;

	// snap-in
	vec2 wd = vec2(windowState.dims);
	vec2 win = round((ndc * 0.5 + vec2(0.5)) * wd);
	win = win + (vec2(s) - vec2(0.5));
	ndc = (win / wd) * 2.0 - vec2(1.0);

	gl_Position = vec4(ndc, 0.0, 1.0);
}
