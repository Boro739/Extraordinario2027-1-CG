#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoords;

// --- Nuevos atributos para skeletal animation ---
layout (location = 3) in ivec4 boneIds;
layout (location = 4) in vec4  weights;

out vec3 Normal;
out vec3 FragPos;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// --- Nuevos uniforms para skeletal animation ---
uniform int  useBones;                // 1 = animacion activa, 0 = modelo estatico
uniform mat4 finalBonesMatrices[100]; // transformaciones finales de cada hueso

// --- Animacion procedural de la bandera ---
uniform int   ondear;      // 1 = desplazar la tela con ondas
uniform float tiempo;      // segundos
uniform float ondaInicio;  // X (modelo) donde la tela se une al asta
uniform float ondaLargo;   // largo de la tela en X (con signo)
uniform float ondaAlturaMin; // Y minima de la tela (el asta y la base no se mueven)

void main()
{
    vec4 skinnedPosition = vec4(0.0);
    vec3 skinnedNormal   = vec3(0.0);

    if (useBones == 1)
    {
        for (int i = 0; i < 4; i++)
        {
            if (boneIds[i] == -1) continue;

            if (boneIds[i] >= 100)
            {
                skinnedPosition = vec4(position, 1.0);
                skinnedNormal   = normal;
                break;
            }

            skinnedPosition += finalBonesMatrices[boneIds[i]] * vec4(position, 1.0) * weights[i];
            skinnedNormal   += mat3(finalBonesMatrices[boneIds[i]]) * normal * weights[i];
        }
    }
    else
    {
        // Comportamiento original sin animacion
        skinnedPosition = vec4(position, 1.0);
        skinnedNormal   = normal;
    }

    if (ondear == 1 && skinnedPosition.y >= ondaAlturaMin)
    {
        // d = 0 junto al asta (no se mueve) y 1 en la punta de la tela; suma de dos senos viajando hacia la punta
        float d = clamp((skinnedPosition.x - ondaInicio) / ondaLargo, 0.0, 1.0);
        float onda = sin(6.0 * d - 5.0 * tiempo) + 0.4 * sin(11.0 * d - 8.0 * tiempo + 1.3);
        skinnedPosition.z += 0.12 * abs(ondaLargo) * d * onda;
        skinnedPosition.y += 0.03 * abs(ondaLargo) * d * sin(4.0 * d - 3.0 * tiempo);
    }

    gl_Position = projection * view * model * skinnedPosition;
    FragPos     = vec3(model * skinnedPosition);
    Normal      = mat3(transpose(inverse(model))) * skinnedNormal;
    TexCoords   = texCoords;
}
