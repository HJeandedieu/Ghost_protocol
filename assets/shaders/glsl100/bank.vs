#version 100
attribute vec3 vertexPosition;
attribute vec3 vertexNormal;
attribute vec4 vertexColor;
varying vec4 surfaceTint;
uniform mat4 mvp;
uniform mat4 matModel;
varying vec3 worldPosition;
varying vec3 worldNormal;
void main() {
    surfaceTint = vertexColor;
    worldPosition = (matModel * vec4(vertexPosition, 1.0)).xyz;
    worldNormal = normalize((matModel * vec4(vertexNormal, 0.0)).xyz);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
