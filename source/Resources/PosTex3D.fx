//------------------------------------------------
// Globals
//------------------------------------------------
float gPI = 3.14159265359f;
float gLightIntensity = 7.0f;
float gShininess = 25.0f;

float3 gLightDirection = normalize(float3(0.577f, -0.577f, 0.577f));

float4x4 gWorldViewProj : WorldViewProjection;
float4x4 gWorld : World;
float4x4 gViewInverse : ViewInverse;

Texture2D gDiffuseMap : DiffuseMap;
Texture2D gNormalMap : NormalMap;
Texture2D gSpecularMap : SpecularMap;
Texture2D gGlossinessMap : GlossinessMap;

SamplerState gSamState : SampleState
{
	Filter = MIN_MAG_MIP_POINT;
	AddressU = Wrap; // or Mirror, Clamp, Border
	AddressV = Wrap; // or Mirror, Clamp, Border
};

RasterizerState gRasterizerState
{
	CullMode = back;
	FrontCounterClockwise = false; // default
};

BlendState gBlendState
{
	BlendEnable[0] = false;
	RenderTargetWriteMask[0] = 0x0F;
};

DepthStencilState gDepthStencilState
{
	DepthEnable = true;
	DepthWriteMask = 1;
	DepthFunc = less;
	StencilEnable = false;
};

//------------------------------------------------
// Input/Output Struct
//------------------------------------------------
struct VS_INPUT
{
	float3 Position	: POSITION;
	float3 Normal	: NORMAL;
	float3 Tangent	: TANGENT;
	float2 UV		: TEXCOORD;
};

struct VS_OUTPUT
{
	float4 Position			: SV_POSITION;
	float4 WorldPosition	: W_POSITION;
	float3 Normal			: NORMAL;
	float3 Tangent			: TANGENT;
	float2 UV				: TEXCOORD;
};

//------------------------------------------------
// Vertex Shader
//------------------------------------------------
VS_OUTPUT VS(VS_INPUT input)
{
	VS_OUTPUT output = (VS_OUTPUT)0;
	output.Position = mul(float4(input.Position, 1.0f), gWorldViewProj);
	output.WorldPosition = mul(float4(input.Position, 1.0f), gWorld);
	output.Tangent = mul(normalize(input.Tangent), (float3x3)gWorld);
	output.Normal = mul(normalize(input.Normal), (float3x3)gWorld);
	output.UV = input.UV;
	return output;
}

//------------------------------------------------
// BRDF Calculation
//------------------------------------------------
float4 CalculateLambert(float kd, float4 cd)
{
	return cd * kd / gPI;
}

float CalculatePhong(float ks, float exp, float3 l, float3 v, float3 n)
{
	float3 reflectedLightVector = reflect(l,n);
	float reflectedViewDot = saturate(dot(reflectedLightVector, v));
	float phong = ks * pow(reflectedViewDot, exp);

	return phong;
}

//------------------------------------------------
// Pixel Shader
//------------------------------------------------
float4 PS(VS_OUTPUT input) : SV_TARGET
{
	float3 binormal = cross(input.Normal, input.Tangent);
	float4x4 tangentSpaceAxis = float4x4(float4(input.Tangent, 0.0f), float4(binormal, 0.0f), float4(input.Normal, 0.0), float4(0.0f, 0.0f, 0.0f, 1.0f));
	float3 currentNormalMap = 2.0f * gNormalMap.Sample(gSamState, input.UV).rgb - float3(1.0f, 1.0f, 1.0f);
	float3 normal = mul(float4(currentNormalMap, 0.0f), tangentSpaceAxis);

	float3 viewDirection = normalize(input.WorldPosition.xyz - gViewInverse[3].xyz);

	float observedArea = saturate(dot(normal, -gLightDirection));

	float4 lambert = CalculateLambert(1.0f, gDiffuseMap.Sample(gSamState, input.UV));

	float specularExp = gShininess * gGlossinessMap.Sample(gSamState, input.UV).r;
	float4 specular = gSpecularMap.Sample(gSamState, input.UV) * CalculatePhong(1.0f, specularExp, -gLightDirection, viewDirection, input.Normal);

	return (gLightIntensity * lambert + specular) * observedArea;
}

//------------------------------------------------
// Technique
//------------------------------------------------
technique11 DefaultTechnique
{
	pass P0
	{
		SetRasterizerState(gRasterizerState);
		SetDepthStencilState(gDepthStencilState, 0);
		SetBlendState(gBlendState, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
		SetVertexShader(CompileShader(vs_5_0, VS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, PS()));
	}
}