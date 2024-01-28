uniform float farPlane;
uniform vec3 lightPosWS;

in vec4 surfacePosWS;

void main()
{
    // float distance = length(surfacePosWS.xyz - lightPosWS);
    vec3 lightToSurfaceWS = surfacePosWS.xyz - lightPosWS;
    float depth = max(max(abs(lightToSurfaceWS.x), abs(lightToSurfaceWS.y)), abs(lightToSurfaceWS.z));
    const float nearPlane = 0.001;
    gl_FragDepth = (depth - nearPlane) / (farPlane - nearPlane);
    // float depth = abs(surfacePosWS.z - lightPosWS.z);
    // gl_FragDepth = depth / farPlane;
    // gl_FragDepth = distance / farPlane;
}