#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexColor;
out vec3 normal;
out vec4 tint;
uniform mat4 mvp;
uniform mat4 matNormal;
void main() {
    normal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    tint = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
