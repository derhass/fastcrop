#version 450

in vec2 texCoord;

layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D tex;

layout(std140, binding=1) uniform displayStateUBO
{
	uvec2 imgDims;
	vec2 scale;
	vec2 offset;
} displayState;

layout(std140, binding=2) uniform cropStateUBO
{
	uvec2 pos;
	uvec2 size;
} cropState;

void main()
{
	color = texture(tex, texCoord);
	if (cropState.size.x > 0) {
		vec2 imgCoord = uvec2(texCoord * vec2(displayState.imgDims));
	        uvec2 imgCoordu = uvec2(imgCoord);
		if (any(lessThan(imgCoordu, cropState.pos)) || any(greaterThanEqual(imgCoordu,cropState.pos + cropState.size))) {
			color.rgb = 0.25 * color.rgb;
		}
	}
}
