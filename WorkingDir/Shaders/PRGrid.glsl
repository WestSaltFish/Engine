#ifdef GRID_SHADER

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main()
{
	vTexCoord = aTexCoord;
	gl_Position = vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) ///////////////////////////////////////////////

in vec2 vTexCoord;

uniform float left;
uniform float right;
uniform float bottom;
uniform float top;
uniform float znear;

uniform float worldMatrix;
uniform float viewMatrix;

layout(location = 0) out vec4 oColor;

void main()
{
	vec4 finalColor = vec4(1.0f);

    vec3 eyeDirEyeSpace;
    eyeDirEyeSpace.x = left + vTexCoord.x + (right - left);
    eyeDirEyeSpace.y = bottom + vTexCoord.y + (top - bottom);
    eyeDirEyeSpace.z = -znear;
    vec3 eyeDirWorldSpace = normalize(mat3(worldMatrix) * eyeDirEyeSpace);

    vec3 eyeDirEyeSpace = vec3(0.0);
    vec3 eyePosWorldSpace = vec3(worldMatrix * vec4(eyeDirEyeSpace, 1.0));

    vec3 planeNormalWorldSpace = vec3(0.0,1.0,0.0);
    vec3 planePointWorldSpace = vec3(0.0);

    float numerator = dot(planePointWorldSpace - eyePosWorldSpace, planeNormalWorldSpace);
    float denominator = dot(eyeDirWorldSpace, planeNormalWorldSpace);
    float t = numerator / denominator;

    if(t > 0.0)
    {
        vec3 hitWorldSpace = eyePosWorldSpace + eyeDirWorldSpace * t;
        oColor = vec4(grid(hitWorldSpace, 1.0));
    }
    else
    {
        gl_FragDepth = 0.0;
        discard;
    }

	oColor = finalColor;
}

#endif
#endif