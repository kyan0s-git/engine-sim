#version 420

layout(binding = 0) uniform sampler2D diffuseTex;

out vec4 out_Color;

in vec4 ex_Pos;
in vec2 ex_Tex;
in vec3 ex_Normal;
in vec2 ex_Local;

layout (binding = 0) uniform ScreenVariables {
	mat4 CameraView;
	mat4 Projection;
	vec4 CameraEye;
	
	vec4 FogColor;
	float FogNear;
	float FogFar;
};

layout (binding = 1) uniform ObjectVariables {
	mat4 Transform;
	vec2 TexOffset;
	vec2 TexScale;
	vec4 Scale;

	vec4 BaseColor;
	vec4 Emission;
	float SpecularMix;
	float DiffuseMix;
	float Metallic;
	float DiffuseRoughness;
	float SpecularPower;
	float IncidentSpecular;

	int ColorReplace;
	int Lit;
};

struct Light {
	vec4 Position;
	vec4 Color;
	vec4 Direction;
	float Attenuation0;
	float Attenuation1;
	int FalloffEnabled;
	int Active;
};

layout (binding = 3) uniform Lighting {
	Light Lights[32];
	vec4 AmbientLighting;
};

float pow5(float v) {
	return (v * v) * (v * v) * v;
}

float f_diffuse(vec3 i, vec3 o, vec3 h, vec3 normal, float power, float roughness) {
	float h_dot_i = dot(h, i);
	float h_dot_i_2 = h_dot_i * h_dot_i;
	float f_d90 = 0.5 + 2 * h_dot_i_2 * roughness;

	float cos_theta_i = dot(i, normal);
	float cos_theta_o = dot(o, normal);

	float f_d = (1 + (f_d90 - 1) * pow5(1 - cos_theta_i)) * (1 + (f_d90 - 1) * pow5(1 - cos_theta_o));
	return clamp(f_d * power * cos_theta_i, 0.0, 1.0);
}

float f_specular(vec3 i, vec3 o, vec3 h, vec3 normal, float F0, float power, float specularPower) {
	vec3 reflected = -reflect(i, normal);
	float intensity = dot(reflected, o);

	if (intensity < 0.0) return 0.0;

	// Fresnel approximation
	float F0_scaled = 0.08 * F0;
	float o_dot_h = dot(o, h);
	float s = pow5(1 - o_dot_h);
	float F = F0_scaled + s * (1 - F0_scaled);

	return clamp(pow(intensity, specularPower) * F * power, 0.0, 1.0);
}

float f_specular_ambient(vec3 o, vec3 normal, float F0, float power) {
	// Fresnel approximation
	float F0_scaled = 0.08 * F0;
	float o_dot_n = dot(o, normal);
	float s = pow5(1 - o_dot_n);
	float F = F0_scaled + s * (1 - F0_scaled);

	return clamp(F * power, 0.0, 1.0);
}

float linearToSrgb(float u) {
	const float MinSrgbPower = 0.0031308;

	if (u < MinSrgbPower) {
		return 12.92 * u;
	}
	else {
		return 1.055 * pow(u, 1 / 2.4) - 0.055;
	}
}

float srgbToLinear(float u) {
	const float MinSrgbPower = 0.04045;

    if (u < MinSrgbPower) {
        return u / 12.92;
    }
    else {
        return pow((u + 0.055) / 1.055, 2.4);
    }
}

vec3 linearToSrgb(vec3 v) {
	return vec3(
		linearToSrgb(v.r),
		linearToSrgb(v.g),
		linearToSrgb(v.b)
	);
}

vec3 srgbToLinear(vec3 v) {
	return vec3(
		srgbToLinear(v.r),
		srgbToLinear(v.g),
		srgbToLinear(v.b)
	);
}

// Shading of the flat 2D engine parts (Lit >= 2). Each part is given a
// coordinate t that runs across it; the screen-space derivative of t gives
// the direction across the part, so the surface normal of a round bar or
// pin can be reconstructed at any rotation without extra geometry.
//   2: cylinder across the model's local x (half width in DiffuseRoughness)
//   3: round bar across a generated line's width (texture v)
//   4: dome / journal (radial texture coordinates)
//   5: plate with bevelled edges across a line's width
//   6: flat metal
//   7: turned (machined) disc, radius in DiffuseRoughness
//   8: honed cylinder liner across a line's width
// SpecularMix: highlight strength, SpecularPower: shininess,
// Metallic: tints the highlight, DiffuseMix: grain, IncidentSpecular: grain
// cells per local unit.
float hash12(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

vec3 shadePart(vec3 base) {
	float t = 0.0;
	bool curved = true;
	if (Lit == 2) t = clamp(ex_Local.x / max(DiffuseRoughness, 1e-4), -1.0, 1.0);
	else if (Lit == 3 || Lit == 5) t = clamp(2.0 * ex_Tex.y - 1.0, -1.0, 1.0);
	else if (Lit == 4) t = clamp(length(2.0 * ex_Tex - 1.0), 0.0, 1.0);
	else curved = false;

	vec3 n = vec3(0.0, 0.0, 1.0);
	if (curved && Lit != 5) {
		vec2 g = vec2(dFdx(t), dFdy(t));
		float gl = length(g);
		vec2 dir = (gl > 1e-6) ? g / gl : vec2(0.0);
		n = normalize(vec3(dir * t, sqrt(max(1.0 - t * t, 0.0)) + 0.15));
	}

	vec3 l = normalize(vec3(-0.45, 0.6, 0.65));
	vec3 h = normalize(l + vec3(0.0, 0.0, 1.0));
	float diffuse = max(dot(n, l), 0.0);
	float highlight = pow(max(dot(n, h), 0.0), max(SpecularPower, 1.0)) * SpecularMix;

	vec3 color = base * (0.42 + 0.72 * diffuse);
	color += highlight * mix(vec3(1.0), base * 1.6, Metallic);

	// Bevelled plates darken towards their edges
	if (Lit == 5) color *= 1.0 - 0.35 * pow(abs(t), 6.0);

	// Machining rings, darker towards the rim
	if (Lit == 7) {
		float r = length(ex_Local) / max(DiffuseRoughness, 1e-4);
		color *= (1.0 + 0.025 * sin(r * 110.0)) * (1.0 - 0.18 * r * r);
	}

	// Concave bore with a cross-hatched hone pattern
	if (Lit == 8) {
		float u = clamp(2.0 * ex_Tex.y - 1.0, -1.0, 1.0);
		vec2 p = ex_Local * 900.0;
		float a = abs(fract(p.y + 0.55 * p.x) - 0.5);
		float b = abs(fract(p.y - 0.55 * p.x) - 0.5);
		float hatch = mix(0.86, 1.0, smoothstep(0.0, 0.12, min(a, b)));
		color = base * (0.55 + 0.45 * u * u) * hatch;
	}

	// Fine grain fixed to the part
	float grain = hash12(floor(ex_Local * IncidentSpecular)) - 0.5;
	color *= 1.0 + DiffuseMix * grain;

	return max(color, vec3(0.0)) + Emission.rgb;
}

void main(void) {
	const float FullSpecular = 1 / 0.08;

	vec3 totalLighting = vec3(1.0, 1.0, 1.0);
	vec3 normal = normalize(ex_Normal.xyz);

	vec4 baseColor;
	float roughness = 0.5;
	float power = 1.0;

	if (ColorReplace == 0) {
		vec4 diffuse = texture(diffuseTex, ex_Tex).rgba;
		baseColor = vec4(srgbToLinear(diffuse.rgb), diffuse.a) * BaseColor;
	}
	else {
		baseColor = BaseColor;
	}

	totalLighting = baseColor.rgb;

	if (Lit >= 2) {
		totalLighting = shadePart(baseColor.rgb);
	}
	else if (Lit == 1) {
		vec3 o = normalize(CameraEye.xyz - ex_Pos.xyz);
		float cos_theta_o = dot(o, normal);

		vec3 ambientSpecular = f_specular_ambient(o, normal, IncidentSpecular, SpecularMix) * AmbientLighting.rgb;
		vec3 ambientDiffuse = f_diffuse(o, o, o, normal, DiffuseMix, DiffuseRoughness) * AmbientLighting.rgb * baseColor.rgb;
		vec3 ambientMetallic = f_specular_ambient(o, normal, FullSpecular, 1.0) * AmbientLighting.rgb * baseColor.rgb;

		totalLighting = mix(
			ambientSpecular + ambientDiffuse,
			ambientMetallic,
			Metallic);
		totalLighting += Emission.rgb;

		for (int li = 0; li < 32; ++li) {
			if (Lights[li].Active == 0) continue;

			vec3 i = Lights[li].Position.xyz - ex_Pos.xyz;
			float inv_dist = 1.0 / length(i);
			i *= inv_dist;

			float cos_theta_i = dot(i, normal);

			if (cos_theta_i < 0) continue;
			if (cos_theta_o < 0) continue;

			vec3 h = normalize(i + o);
			vec3 diffuse = f_diffuse(i, o, h, normal, DiffuseMix, DiffuseRoughness) * baseColor.rgb * Lights[li].Color.rgb;
			vec3 specular = f_specular(i, o, h, normal, IncidentSpecular, SpecularMix, SpecularPower) * Lights[li].Color.rgb;
			vec3 metallic = vec3(0.0, 0.0, 0.0);

			if (Metallic > 0) {
				metallic = f_specular(i, o, h, normal, FullSpecular, 1, SpecularPower) * Lights[li].Color.rgb * baseColor.rgb;
			}

			// Spotlight calculation
			float spotCoherence = -dot(i, Lights[li].Direction.xyz);
			float spotAttenuation = 1.0;
			if (spotCoherence > Lights[li].Attenuation0) spotAttenuation = 1.0;
			else if (spotCoherence < Lights[li].Attenuation1) spotAttenuation = 0.0;
			else {
				float t = Lights[li].Attenuation0 - Lights[li].Attenuation1;
				if (t == 0) spotAttenuation = 1.0;
				else spotAttenuation = (spotCoherence - Lights[li].Attenuation1) / t;
			}

			float falloff = 1.0;
			if (Lights[li].FalloffEnabled == 1) {
				falloff = (inv_dist * inv_dist);
			}

			vec3 bsdf = mix(
				diffuse + specular,
				metallic,
				Metallic);

			totalLighting += falloff * bsdf * spotAttenuation * spotAttenuation * spotAttenuation;
		}
	}

	float distanceToCamera = length(CameraEye.xyz - ex_Pos.xyz);
	float fogAttenuation = (clamp(distanceToCamera, FogNear, FogFar) - FogNear) / (FogFar - FogNear);

	out_Color = vec4(
		linearToSrgb(
			mix(
				totalLighting.rgb,
				FogColor.rgb,
				fogAttenuation)), baseColor.a);
}
