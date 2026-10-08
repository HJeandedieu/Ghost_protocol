#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float elapsedTime;
uniform float alarmPulse;
uniform float grainIntensity;
uniform vec2 texelSize;
uniform float vignetteStrength;
out vec4 finalColor;

void main() {
    vec4 color = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    // Restrained one-pixel bloom only samples already rendered, visible geometry.
    vec2 texel = texelSize;
    vec3 neighbors = texture(texture0, fragTexCoord + vec2(texel.x, 0.0)).rgb
        + texture(texture0, fragTexCoord - vec2(texel.x, 0.0)).rgb
        + texture(texture0, fragTexCoord + vec2(0.0, texel.y)).rgb
        + texture(texture0, fragTexCoord - vec2(0.0, texel.y)).rgb;
    color.rgb += max(neighbors * 0.25 - vec3(0.65), vec3(0.0)) * 0.08;
    vec2 offset = (fragTexCoord - 0.5) * 2.0;
    float vignette = 1.0 - vignetteStrength * (1.0 + alarmPulse) * smoothstep(0.2, 1.414214, length(offset));
    float grain = fract(sin(dot(fragTexCoord / texelSize, vec2(12.9898, 78.233)) + elapsedTime * 17.0) * 43758.5453);
    // Multiplicative grain keeps unrevealed darkness dark.
    finalColor = vec4(color.rgb * vignette * (1.0 + (grain - 0.5) * 2.0 * grainIntensity), color.a);
}
