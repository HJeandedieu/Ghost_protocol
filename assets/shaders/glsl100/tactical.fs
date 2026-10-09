#version 100
precision mediump float;
varying vec3 normal;
varying vec4 tint;
uniform vec3 tealRim;
uniform vec3 redRim;
uniform vec3 goldRim;
uniform float phase;
uniform float visibility;
void main() {
    if (visibility <= 0.0) discard;
    vec3 n = normalize(normal);
    float key = max(dot(n, normalize(vec3(-0.6,0.8,0.5))),0.0);
    float rim = pow(max(dot(n, normalize(vec3(0.8,0.25,-0.5))),0.0),3.0);
    float edge = pow(max(dot(n, normalize(vec3(-0.9,0.3,-0.2))),0.0),4.0);
    vec3 color = tint.rgb * (0.55 + key*0.6);
    color += rim * mix(tealRim,redRim,phase)*0.24;
    color += edge * mix(tealRim,goldRim,phase)*0.22;
    gl_FragColor = vec4(color, tint.a*visibility);
}
