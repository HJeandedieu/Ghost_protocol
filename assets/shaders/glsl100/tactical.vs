#version 100
attribute vec3 vertexPosition;
attribute vec3 vertexNormal;
attribute vec4 vertexColor;
varying vec3 normal;
varying vec4 tint;
uniform mat4 mvp;
uniform mat4 matNormal;
void main() {
    normal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    tint = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
