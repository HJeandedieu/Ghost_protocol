#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
out vec3 worldPosition;
out vec3 worldNormal;
void main() {
    worldPosition = (matModel * vec4(vertexPosition, 1.0)).xyz;
    worldNormal = normalize((matModel * vec4(vertexNormal, 0.0)).xyz);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
