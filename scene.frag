#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform vec3 objectColor;
uniform float emissionStrength;
uniform int useTexture;
uniform sampler2D diffuseTexture;

void main()
{
    vec3 baseColor = objectColor;
    if (useTexture == 1) {
        baseColor *= texture(diffuseTexture, TexCoords).rgb;
    }

    // 1. Ambient Lighting (Phong)
    float ambientStrength = 0.20;
    vec3 ambient = ambientStrength * baseColor;

    // 2. Diffuse Lighting (Phong)
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * baseColor;

    // 3. Specular Lighting (Phong)
    float specularStrength = 0.7;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * lightColor;

    // Distance Attenuation + Emissive Neon Glow
    float distance = length(lightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.015 * distance + 0.0007 * (distance * distance));

    vec3 lighting = ambient + (diffuse + specular) * attenuation;
    vec3 emissive = baseColor * emissionStrength;
    vec3 result = lighting + emissive;

    // Output 0: Normal Scene Color
    FragColor = vec4(result, 1.0);

    // Output 1: Bright Pixel Extraction for Bloom (Luminance Threshold)
    float brightness = dot(result, vec3(0.2126, 0.7152, 0.0722));
    if (brightness > 1.0)
        BrightColor = vec4(result, 1.0);
    else
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
}