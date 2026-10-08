#version 100
precision highp float;
varying vec2 fragTexCoord;
varying vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float elapsedTime;
uniform float alarmPulse;
uniform float grainIntensity;
uniform float vignetteStrength;

void main() {
    vec4 color = texture2D(texture0, fragTexCoord) * colDiffuse * fragColor;
    // Restrained one-pixel bloom only samples already rendered, visible geometry.
    vec2 texel = vec2(1.0 / 1280.0, 1.0 / 720.0);
    vec3 neighbors = texture2D(texture0, fragTexCoord + vec2(texel.x, 0.0)).rgb
        + texture2D(texture0, fragTexCoord - vec2(texel.x, 0.0)).rgb
        + texture2D(texture0, fragTexCoord + vec2(0.0, texel.y)).rgb
        + texture2D(texture0, fragTexCoord - vec2(0.0, texel.y)).rgb;
    color.rgb += max(neighbors * 0.25 - vec3(0.65), vec3(0.0)) * 0.08;
    vec2 offset = (fragTexCoord - 0.5) * 2.0;
    float vignette = 1.0 - vignetteStrength * (1.0 + alarmPulse) * smoothstep(0.2, 1.414214, length(offset));
    float grain = fract(sin(dot(fragTexCoord * vec2(1280.0, 720.0), vec2(12.9898, 78.233)) + elapsedTime * 17.0) * 43758.5453);
    gl_FragColor = vec4(color.rgb * vignette * (1.0 + (grain - 0.5) * 2.0 * grainIntensity), color.a);
}
