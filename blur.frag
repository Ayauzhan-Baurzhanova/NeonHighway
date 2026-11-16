#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D image;
uniform sampler2D bloomBlur;
uniform int passType; // 0 = Horizontal Gaussian Blur, 1 = Vertical Gaussian Blur, 2 = Final Composite
uniform int bloomEnabled;
uniform float exposure;

uniform float weight[5] = float[] (0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);

void main()
{
    if (passType == 0 || passType == 1)
    {
        vec2 tex_offset = 1.0 / textureSize(image, 0);
        vec3 result = texture(image, TexCoords).rgb * weight[0];
        if (passType == 0)
        {
            for(int i = 1; i < 5; ++i)
            {
                result += texture(image, TexCoords + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
                result += texture(image, TexCoords - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
            }
        }
        else
        {
            for(int i = 1; i < 5; ++i)
            {
                result += texture(image, TexCoords + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
                result += texture(image, TexCoords - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
            }
        }
        FragColor = vec4(result, 1.0);
    }
    else
    {
        vec3 hdrColor = texture(image, TexCoords).rgb;
        vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
        if (bloomEnabled == 1)
        {
            hdrColor += bloomColor; // Additive blending
        }
        // Exposure tone mapping & gamma correction
        vec3 result = vec3(1.0) - exp(-hdrColor * exposure);
        result = pow(result, vec3(1.0 / 2.2));
        FragColor = vec4(result, 1.0);
    }
}