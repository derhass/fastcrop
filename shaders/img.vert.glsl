#version 450 core

out vec2 texCoord;

layout(std140, binding=1) uniform displayStateUBO
{
	ivec2 imgDims;
	vec2 scale;
	vec2 offset;
} displayState;

void main()
{
	vec2 quad[6] = vec2[6](
			vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
			vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0));
	vec2 pos = quad[gl_VertexID];

	texCoord = vec2(pos.x, 1.0 - pos.y);
	gl_Position = vec4(displayState.scale * pos + displayState.offset, 0.0, 1.0);
}
