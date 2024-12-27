#version 450 core

in vec2 texCoord;

layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D tex;

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
	color = texture(tex, texCoord);
	if (cropState.size.x > 0) {
		vec2 imgCoord = texCoord * vec2(displayState.imgDims);
	        ivec2 imgCoordi = ivec2(imgCoord);
		if (any(lessThan(imgCoordi, cropState.pos)) || any(greaterThanEqual(imgCoordi,cropState.pos + cropState.size))) {
			color.rgb = 0.25 * color.rgb;
		}
	}
}
