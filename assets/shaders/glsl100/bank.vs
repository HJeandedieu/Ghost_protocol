#version 100
attribute vec3 vertexPosition;
attribute vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
varying vec3 worldPosition;
varying vec3 worldNormal;
void main() {
    worldPosition = (matModel * vec4(vertexPosition, 1.0)).xyz;
    worldNormal = normalize((matModel * vec4(vertexNormal, 0.0)).xyz);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
