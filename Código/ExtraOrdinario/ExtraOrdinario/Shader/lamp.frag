#version 330 core
out vec4 FragColor;

// Color del cubo que marca cada spotlight (gris si esta apagada)
uniform vec3 lampColor;

void main()
{
     FragColor = vec4(lampColor, 1.0f);
}
