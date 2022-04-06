#version 430 core

layout(location=0) in vec3 position;
layout(location=1) in vec4 color;
layout(location=2) in vec3 normal;

out vec3 interNormal;
out vec4 vertexColor;
out vec4 interPos;

uniform mat3 normMat;
uniform mat4 modelMat;
uniform mat4 viewMat;
uniform mat4 projMat;


void main()
{

	// Get position of vertex (object space)
	vec4 objPos = vec4(position, 1.0);

	// calculate position after model and view transformations
	interPos = viewMat * modelMat * objPos;

	//calculate normal position after normal transformation
	interNormal = normMat * normal;

	// For now, just pass along vertex position (no transformations)
	gl_Position = projMat * viewMat * modelMat * objPos;

	// Output per-vertex color
	vertexColor = color;
}
