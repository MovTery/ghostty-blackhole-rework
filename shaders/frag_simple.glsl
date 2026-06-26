 #version 330
 
 uniform vec3 iResolution;
 uniform float iTime;
 uniform vec4 iDate;
 
 out vec4 fragColor;
 
 // ===== Simple black hole shader (no Cg compiler issues) =====
 
 float rand(vec2 co) {
     return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
 }
 
 vec3 starfield(vec2 uv) {
     vec3 col = vec3(0.01, 0.01, 0.04);
     // Nebula
     vec2 np = uv * 3.0 + vec2(sin(iTime * 0.02), cos(iTime * 0.015)) * 0.3;
     float neb = 0.0;
     neb += 0.3 * sin(np.x * 1.2 + np.y * 0.8 + iTime * 0.005);
     neb += 0.2 * sin(np.x * 0.7 - np.y * 1.5 + iTime * 0.008);
     neb = max(0.0, neb);
     col += vec3(0.02, 0.005, 0.04) * neb;
     // Stars
     for (int layer = 0; layer < 3; layer++) {
         float scale = 50.0 + float(layer) * 80.0;
         vec2 p = uv * scale;
         vec2 ip = floor(p);
         vec2 fp = fract(p) - 0.5;
         float r = rand(ip + float(layer) * 3.7);
         float star = 1.0 - smoothstep(0.02, 0.15 * (1.0 - r * 0.5), length(fp));
         star *= step(0.92, r);
         float twinkle = 0.7 + 0.3 * sin(iTime * (0.5 + 2.0 * r) + 40.0 * r);
         col += vec3(0.8 + r * 0.2) * star * 0.6 * twinkle;
     }
     // Milky Way
     float mw = 0.03 * max(0.0, sin((uv.x + uv.y * 0.3) * 1.5 + 0.5) * 0.5 + 0.5);
     col += vec3(0.01, 0.005, 0.015) * mw;
     return clamp(col, 0.0, 1.0);
 }
 
 void main() {
     vec2 uv = gl_FragCoord.xy / iResolution.xy;
     float aspect = iResolution.x / iResolution.y;
     vec2 p = (gl_FragCoord.xy - iResolution.xy * 0.5) / iResolution.y;
     
     // Black hole centered on screen, slowly drifting
     vec2 center = vec2(0.0, 0.0);
     float r = length(p - center);
     
     // Shadow radius (fraction of screen height)
     float shadowR = 0.08;
     // Einstein ring radius (where lensing is strongest)
     float einsteinR = 0.25;
     // Outer radius for lensing falloff
     float outerR = 0.5;
     
     vec3 col;
     if (r < shadowR) {
         // Inside the shadow — pure black
         col = vec3(0.0);
     } else {
         // Gravitational lensing
         float lensing = 0.0;
         if (r < einsteinR) {
             // Strong lensing near the shadow
             lensing = (einsteinR / r - 1.0) * 0.5;
         } else if (r < outerR) {
             // Weak lensing falloff
             lensing = smoothstep(outerR, einsteinR, r) * 0.3;
         }
         
         // Sample the background at the lensed position
         vec2 dir = normalize(p - center);
         vec2 lensedUV = uv + dir * lensing * 0.08;
         col = starfield(lensedUV);
         
         // Einstein ring glow
         float ring = 1.0 - smoothstep(0.0, 0.08, abs(r - einsteinR));
         col += vec3(0.3, 0.2, 0.1) * ring * 0.5;
         
         // Accretion disk glow (simplified — colored ring)
         float diskInner = shadowR * 1.2;
         float diskOuter = einsteinR * 1.1;
         float disk = 1.0 - smoothstep(diskInner, diskOuter, r);
         if (r > shadowR * 1.0) {
             vec3 diskColor = mix(vec3(0.8, 0.3, 0.1), vec3(0.1, 0.1, 0.8), smoothstep(diskInner, diskOuter, r));
             col += diskColor * disk * 0.15;
         }
     }
     
     fragColor = vec4(col, 1.0);
 }
