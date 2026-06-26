param($WorkDir)

$header = @"
#version 460 core

layout(location = 0) uniform vec3 iResolution;
layout(location = 1) uniform float iTime;
layout(location = 2) uniform vec4 iDate;
layout(location = 3) uniform vec4 iCurrentCursorColor;
layout(location = 4) uniform vec4 iPreviousCursorColor;

layout(location = 0) out vec4 fragColor;

float rand(vec2 co) { return fract(sin(dot(co,vec2(12.9898,78.233)))*43758.5453); }
vec3 sky(vec2 uv) {
    vec3 c=vec3(.01,.01,.04); vec2 np=uv*3.+vec2(sin(iTime*.02),cos(iTime*.015))*.3;
    float nb=max(0.,.3*sin(np.x*1.2+np.y*.8+iTime*.005)+.2*sin(np.x*.7-np.y*1.5+iTime*.008));
    c+=vec3(.02,.005,.04)*nb;
    for(int l=0;l<3;l++){float sc=50.+float(l)*80.;vec2 p=uv*sc,ip=floor(p),fp=fract(p)-.5;
    float r=rand(ip+float(l)*3.7);float st=step(.92,r)*(1.-smoothstep(.02,.15*(1.-r*.5),length(fp)));
    c+=vec3(.8+r*.2)*st*.6*(.7+.3*sin(iTime*(.5+2.*r)+40.*r));}
    c+=vec3(.01,.005,.015)*.03*max(0.,sin((uv.x+uv.y*.3)*1.5+.5)*.5+.5);
    return clamp(c,0.,1.);
}
"@

$body = Get-Content "$WorkDir\blackhole.glsl" -Encoding UTF8 -Raw
$body = $body -replace 'void mainImage\(out vec4 \w+, in vec2 \w+\) \{\s*', 'void main() {'
$body = $body -replace '(?<![.\w])fragCoord(?![.\w])', 'gl_FragCoord.xy'
$body = $body -replace 'SIZE_MODE MODE_TOKENS', 'SIZE_MODE MODE_DEMO'
$body = $body -replace 'iTimeCursorChange', 'iTime'
$body = $body -replace 'texture\(iChannel0, suv\)', 'sky(suv)'
$body = $body -replace 'texture\(iChannel0, uv\)', 'vec4(sky(uv), 1.0)'

$combined = $header + "`n" + $body
$combinedPath = "$WorkDir\build\combined_shader.glsl"
$spvPath = "$WorkDir\build\blackhole_frag.spv"
Set-Content $combinedPath -Encoding UTF8 -Value $combined

$glslang = "C:\msys64\ucrt64\bin\glslangValidator.exe"
$proc = Start-Process -NoNewWindow -FilePath $glslang -ArgumentList "-G -S frag -o ""$spvPath"" ""$combinedPath""" -Wait -PassThru
if ($proc.ExitCode -eq 0 -and (Test-Path $spvPath)) {
    $len = (Get-Item $spvPath).Length
    Write-Output "SPIRV_OK:$len"
} else {
    Write-Output "SPIRV_FAIL"
}
