#version 100
precision highp float;
varying vec2 fragTexCoord;
varying vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float elapsedTime;
uniform float alarmPulse;

void main() {
    vec4 color = texture2D(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec2 offset = (fragTexCoord - 0.5) * 2.0;
    float vignette = 1.0 - 0.35 * (1.0 + alarmPulse) * smoothstep(0.2, 1.414214, length(offset));
    float grain = fract(sin(dot(fragTexCoord * vec2(1280.0, 720.0), vec2(12.9898, 78.233)) + elapsedTime * 17.0) * 43758.5453);
    gl_FragColor = vec4(color.rgb * vignette * (1.0 + (grain - 0.5) * 0.08), color.a);
}
