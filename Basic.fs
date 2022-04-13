#version 430 core

layout(location=0) out vec4 out_color;
 
in vec4 vertexColor; // Now interpolated across face
in vec4 interPos;
in vec3 interNormal;

//Structure to hold Point Light
struct PointLight {
vec4 pos;
vec4 color;
};

uniform PointLight light;



void main() {	
	vec3 N = normalize(interNormal);
	
	/*
	This section is now irrelevant
	// Just output interpolated color
	out_color = vertexColor;
	*/

	vec3 L = vec3(light.pos - interPos);
	float d = length(L);
  	float at = 1.0 / (d * d + 1.0);

	L = normalize(L);

	float diffuse = max(0, dot(L, N));

	vec3 diffColor = diffuse * vec3(light.color * vertexColor);

	out_color = vec4(diffColor, 1.0);
}
