#version 450 core

layout(location = 0) out vec4 color;

layout(std140, binding=0) uniform windowStateUBO
{
	vec4 cropLineColor;
	uvec2 dims;
} windowState;

void main()
{
	color = windowState.cropLineColor;
}
