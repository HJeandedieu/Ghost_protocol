#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexColor;
out vec4 surfaceTint;
uniform mat4 mvp;
uniform mat4 matModel;
out vec3 worldPosition;
out vec3 worldNormal;
void main() {
    surfaceTint = vertexColor;
    worldPosition = (matModel * vec4(vertexPosition, 1.0)).xyz;
    worldNormal = normalize((matModel * vec4(vertexNormal, 0.0)).xyz);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
