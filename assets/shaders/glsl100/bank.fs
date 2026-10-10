#version 100
precision highp float;
varying vec3 worldPosition;
varying vec3 worldNormal;
varying vec4 surfaceTint;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 mapSize;
uniform vec2 playerPosition;
uniform float tileSize;
uniform float haloRadius;
uniform float phase;
uniform float surfaceMode;
void main() {
    vec3 values = texture2D(texture0, clamp(worldPosition.xz / mapSize, vec2(0.0), vec2(0.99999))).rgb;
    float localHalo = (1.0-smoothstep(haloRadius*0.72, haloRadius, distance(worldPosition.xz,playerPosition))) * values.b;
    float visibility = surfaceMode < 0.5 ? values.r : values.g;
    visibility = mix(max(visibility, localHalo), 1.0, phase);
    float shade = 0.46 + 0.54 * max(dot(normalize(worldNormal), normalize(vec3(-0.4,0.8,-0.3))),0.0);
    vec3 base;
    if (surfaceMode < 0.5) {
        base = mix(vec3(0.10,0.24,0.25),vec3(0.26,0.045,0.065),phase);
        vec2 grid = abs(fract(worldPosition.xz/(tileSize*0.5))-0.5);
        float seam = smoothstep(0.485,0.497,max(grid.x,grid.y));
        base *= 1.0-seam*0.45;
        base *= 0.88+0.12*mod(floor(worldPosition.x/tileSize)+floor(worldPosition.z/tileSize),2.0);
    } else if (surfaceMode < 1.5) {
        base = mix(vec3(0.15,0.29,0.30),vec3(0.35,0.035,0.055),phase);
        float course = abs(fract(worldPosition.y/(tileSize*0.33))-0.5);
        base *= 1.0-smoothstep(0.47,0.499,course)*0.25;
        float baseboard = 1.0-smoothstep(2.0,5.0,worldPosition.y);
        base = mix(base,vec3(0.06,0.065,0.08),baseboard*0.7);
        base *= colDiffuse.rgb;
    } else if (surfaceMode > 2.5) {
        base = colDiffuse.rgb * surfaceTint.rgb;
        base *= mix(vec3(0.55,0.85,0.88),vec3(1.0,0.58,0.5),phase);
        visibility = mix(max(values.r,localHalo),1.0,phase);
    } else {
        base = colDiffuse.rgb;
        visibility = 1.0;
    }
    // Broad red architectural light, restrained edge highlights and attached floor variation.
    float ceilingStrip = pow(max(dot(normalize(worldNormal),vec3(0.0,-1.0,0.0)),0.0),2.0);
    float upper = smoothstep(tileSize*0.85,tileSize*1.35,worldPosition.y);
    base += mix(vec3(0.005,0.022,0.022),vec3(0.09,0.002,0.012),phase)*upper;
    base *= 1.0-ceilingStrip*0.45;
    vec2 fixture = mod(worldPosition.xz/tileSize,4.0)-vec2(0.5);
    fixture = min(abs(fixture),4.0-abs(fixture));
    float pool = exp(-dot(fixture,fixture)*0.7);
    base += mix(vec3(0.01,0.025,0.025),vec3(0.12,0.015,0.005),phase)*pool;
    if(surfaceMode>2.5 && worldPosition.y>tileSize*1.2 && surfaceTint.r>0.8)
        base = mix(vec3(0.42,0.65,0.63),vec3(1.0,0.44,0.23),phase);
    vec3 lit = mix(vec3(0.028,0.031,0.04),base*shade,visibility);
    gl_FragColor = vec4(lit,colDiffuse.a);
}
