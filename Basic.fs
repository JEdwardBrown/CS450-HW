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
uniform float metallic;
uniform float roughness;

const float pi = 3.14159265359;

vec3 getFresnelAtAngleZero(vec3 albedo, float metallic) {
	vec3 f0 = vec3(0.04);
	f0 = mix(f0, albedo, metallic);
	return f0;
}

vec3 getFresnel(vec3 F0, vec3 L, vec3 H) {
	float cosAngle = max(0, dot(L, H));
	F0 = F0 + (1 - F0) * pow((1 - max(0, cosAngle)), 5.0);
	return F0;
}

 float getNDF(vec3 H, vec3 N, float roughness) {
	float NDF;
	NDF = (pow(roughness, 4.0)) / (pi * pow((pow(dot(N, H), 2.0) * (pow(roughness, 4.0) - 1) + 1), 2.0));
	return NDF;
 }

 float getSchlickGeo(vec3 B, vec3 N, float roughness) {
	 float k, SG;
	 k = ((pow(roughness + 1, 2.0)) / 8);
	 SG = (dot(N, B)) / ((dot(N, B)*(1 - k) + k));
	 return SG;
 }

float getGF(vec3 L, vec3 V, vec3 N, float roughness) {
	float GL, GF, GV;
	GL = getSchlickGeo(L, N, roughness);
	GV = getSchlickGeo(V, N, roughness);
	GF = GL * GV;
	return GF;
}

void main() {	
	vec3 N = normalize(interNormal);
	
	/*
	This section is now irrelevant
	// Just output interpolated color
	out_color = vertexColor;
	*/

    vec3 V = normalize(-vec3(interPos));
	vec3 f0 = getFresnelAtAngleZero(vec3(vertexColor), metallic);
	vec3 L = vec3(light.pos - interPos);
	float d = length(L);
  	float at = 1.0 / (d * d + 1.0);

	L = normalize(L);
	vec3 H = normalize(L + V);
	vec3 F = getFresnel(f0, L, H);
	vec3 kS = F;
	//Calculate Diffuse Color
	vec3 kD = 1.0 - kS;
	vec3 diffColor = (kD * (1.0 - metallic) * vec3(vertexColor)) / pi;
	float diffuse = max(0, dot(L, N));
	//Calculate Complete Specular Reflection
	float NDF = getNDF(H, N, roughness);
	float G = getGF(L, V, N, roughness);
	kS = kS * NDF * G;
	vec3 compSpecRef = kS / ((4.0 * max(0, dot(N , L)) * max(0, dot(N , V))) + 0.0001);
 	vec3 finalColor = (kD + kS)*vec3(light.color)*max(0, dot(N,L));
	 
	out_color = vec4(finalColor, 1.0);
}
