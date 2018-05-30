#pragma once

float4x4 _AsyncShadowMatrix;
Texture2D _AsyncShadow;
SamplerComparisonState sampler_AsyncShadow;
float _ShadowBias;

float CalcShadowFactor(float4 shadowPosH)
{
	shadowPosH.xyz /= shadowPosH.w;
	float2 vShadowTexCoord = 0.5f * shadowPosH.xy + 0.5f;
	vShadowTexCoord.y = 1.0f - vShadowTexCoord.y;

	[branch]
	if (!(saturate(vShadowTexCoord.x) == vShadowTexCoord.x) ||
		!(saturate(vShadowTexCoord.y) == vShadowTexCoord.y))
	{
		return 1.0f;
	}
	else
	{
		float depth = shadowPosH.z - _ShadowBias;

		// 3x3 PCF
		uint width, height, numMips;
		_AsyncShadow.GetDimensions(0, width, height, numMips);
		float dx = 1.0f / (float)width;

		float percentLit = 0.0f;
		const float2 offsets[9] =
		{
			float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
			float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
			float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
		};

		[unroll]
		for (int i = 0; i < 9; ++i)
		{
			float shadow = _AsyncShadow.SampleCmpLevelZero(sampler_AsyncShadow,
				vShadowTexCoord.xy + offsets[i], depth).r;

#if defined(UNITY_REVERSED_Z)
			shadow = 1.0f - shadow;
#endif

			percentLit += shadow;
		}

		return percentLit / 9.0f;
	}
}