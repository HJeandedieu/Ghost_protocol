#version 330
in vec3 worldPosition;
in vec3 worldNormal;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 mapSize;
uniform vec2 playerPosition;
uniform float tileSize;
uniform float haloRadius;
uniform float phase;
uniform float surfaceMode;
out vec4 finalColor;
void main() {
    vec3 values = texture(texture0, clamp(worldPosition.xz / mapSize, vec2(0.0), vec2(0.99999))).rgb;
    float localHalo = (1.0-smoothstep(haloRadius*0.72, haloRadius, distance(worldPosition.xz,playerPosition))) * values.b;
    float visibility = surfaceMode < 0.5 ? values.r : values.g;
    visibility = mix(max(visibility, localHalo), 1.0, phase);
    float shade = 0.46 + 0.54 * max(dot(normalize(worldNormal), normalize(vec3(-0.4,0.8,-0.3))),0.0);
    vec3 base;
    if (surfaceMode < 0.5) {
        base = mix(vec3(0.10,0.24,0.25),vec3(0.26,0.045,0.065),phase);
        vec2 grid = abs(fract(worldPosition.xz/tileSize)-0.5);
        float seam = smoothstep(0.485,0.497,max(grid.x,grid.y));
        base *= 1.0-seam*0.45;
        base *= 0.88+0.12*mod(floor(worldPosition.x/tileSize)+floor(worldPosition.z/tileSize),2.0);
    } else if (surfaceMode < 1.5) {
        base = mix(vec3(0.15,0.29,0.30),vec3(0.48,0.055,0.085),phase);
        float course = abs(fract(worldPosition.y/(tileSize*0.33))-0.5);
        base *= 1.0-smoothstep(0.47,0.499,course)*0.25;
        float baseboard = 1.0-smoothstep(2.0,5.0,worldPosition.y);
        base = mix(base,vec3(0.06,0.065,0.08),baseboard*0.7);
        base *= colDiffuse.rgb;
    } else {
        base = colDiffuse.rgb;
        visibility = 1.0;
    }
    vec3 lit = mix(vec3(0.028,0.031,0.04),base*shade,visibility);
    finalColor = vec4(lit,colDiffuse.a);
}
