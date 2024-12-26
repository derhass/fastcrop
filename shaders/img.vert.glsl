#version 450

out vec2 texCoord;

void main()
{
	vec2 quad[6] = vec2[6](
			vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
			vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0));
	vec2 pos = quad[gl_VertexID];

	texCoord = pos;
	gl_Position = vec4(pos, 0.0, 1.0);
}
