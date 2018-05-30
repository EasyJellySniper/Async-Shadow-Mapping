cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
	uint gTexIndex;
};

cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
};

#define MAXTEXTURE 16
Texture2D cutoutMaps[MAXTEXTURE] : register(t0);
SamplerState samAnisoWrap : register(s0);

struct VIn
{
	float3 vertex    : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

struct VOut
{
	float4 vertex    : SV_POSITION;
	float2 uv : TEXCOORD;
};

VOut VS(VIn i)
{
	VOut o = (VOut)0.0f;

    // Transform to world space.
    o.vertex = mul(float4(i.vertex, 1.0f), gWorld);
	o.vertex = mul(o.vertex, gViewProj);
	
	o.uv = i.uv;
	
    return o;
}

void PS(VOut i) 
{
	if(gTexIndex != -1)
	{
		float alpha = cutoutMaps[gTexIndex].Sample(samAnisoWrap, i.uv);
		clip(alpha - 0.5f);
	}
}