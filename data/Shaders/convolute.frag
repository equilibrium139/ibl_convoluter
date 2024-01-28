out vec4 fragColor;
in vec3 dir;

uniform samplerCube environmentMap;

// TODO: implement as define
const float PI = 3.14159265359;

// Compute the irradiance from the hemisphere of directions centered at dir
void main()
{		
    vec3 normal = normalize(dir);
  
    vec3 irradiance = vec3(0.0);
  
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float sampleDelta = 0.025;
    float nrSamples = 0.0; 
    float nSamples = ((2.0 * PI) / sampleDelta) * ((0.5 * PI) / sampleDelta);
    for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal; 

            irradiance += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta) * (1.0 / float(nSamples));
            nrSamples++;
        }
    }
    irradiance = PI * irradiance;
  
    fragColor = vec4(irradiance, 1.0);
}