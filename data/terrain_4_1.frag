#version 140

out vec4 fragColor;

uniform mat4 view;
uniform vec4 clearColor;

uniform sampler2D normals;
uniform sampler2D diffuse;
uniform sampler2D detail;
uniform sampler2D detailn;

in vec2 v_texc;
in vec3 v_eye;
in float v_z;

// you can use this light, e.g., as directional light
const vec3 light = normalize(vec3(2.0, 2.0, 0.0));

void main()
{
	vec3 e = normalize(v_eye);
	vec3 l = light;
	vec3 n = normalize(texture(normals, v_texc).xyz * 2.0 - 1.0);
	vec3 d = texture(diffuse, v_texc).rgb;
	vec3 detail = texture(detail, v_texc*100).rgb;
    vec3 detailn = texture(detailn, v_texc*100).rgb;

    // Task_4_2 - ToDo Begin

		// Make your terrain look nice ;)

		// 1P for having Normal Mapping using the terrains
		// Normal and the details maps normals.

		// 1P for having a detail map that blends in based
		// on the fragments distance.

		// 1P for having atmospheric scattering (fake or real)
		// -> use e.g., the gl_FragCoord.z for this

	// Task_4_2 - ToDo End
	vec4 norm = mix(vec4(n, 1.0), vec4(detailn, 1.f), clamp(1/(pow(3, v_z)), 0.f, 0.5f));
	fragColor = vec4(d, 1.0);

	//details
	fragColor = mix(fragColor, vec4(detail, 1.f), clamp(1/(pow(3, v_z)) - 0.4, 0.f, 0.3f));

	//normals and light
	fragColor *= max(dot(vec4(light, 1.0f), norm), 0.0);

	//normal highlights
    fragColor += vec4(max(dFdy(n).r, dFdx(norm).r));

    //fog of war
    fragColor = mix(fragColor, clearColor, 1 - clamp(3.5/pow(2, v_z), 0.f, 1.f));
}
