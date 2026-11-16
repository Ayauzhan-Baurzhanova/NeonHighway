#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D fontAtlas;
uniform vec4 textColor;
uniform int isPanel;

void main()
{
    if (isPanel == 1)
    {
        FragColor = textColor;
    }
    else
    {
        float alpha = texture(fontAtlas, TexCoords).r;
        if (alpha < 0.1)
            discard;
        FragColor = vec4(textColor.rgb, textColor.a * alpha);
    }
}