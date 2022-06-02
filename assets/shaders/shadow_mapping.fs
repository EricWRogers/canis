#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
} fs_in;

struct Material {
    sampler2D specular;
    sampler2D diffuse;
    float shininess;
}; 

struct DirLight {
    vec3 direction;
	
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform sampler2D shadowMap;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightDirection;
uniform vec4 modelColor;

uniform Material material;

uniform DirLight dirLight;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // check whether current frag pos is in shadow
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightDir = normalize(lightPos - fs_in.FragPos);
    //float bias = 0.005; 
    float bias = max(0.05 * (1.0 - dot(normal, lightDirection)), 0.005);
    //float shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    // keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if(projCoords.z > 1.0)
        shadow = 0.0; 

    return shadow;
}

// calculates the color when using a directional light.
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // combine results
    vec3 ambient = light.ambient * (vec3(modelColor) * vec3(texture(material.diffuse, fs_in.TexCoords)));
    vec3 diffuse = light.diffuse * diff * (vec3(modelColor) * vec3(texture(material.diffuse, fs_in.TexCoords)));
    vec3 specular = light.specular * spec * vec3(texture(material.specular, fs_in.TexCoords));
    
    // // calculate shadow
    float shadow = ShadowCalculation(fs_in.FragPosLightSpace);                      
    return (ambient + (1.0 - shadow) * (diffuse + specular)) * vec3(modelColor);
}

void main()
{           
    vec3 normal = normalize(fs_in.Normal);
    vec3 viewDir = normalize(viewPos - fs_in.FragPos);
    
    vec3 result = CalcDirLight(dirLight, normal, viewDir); 
    
    FragColor = vec4(result, modelColor.a);
}