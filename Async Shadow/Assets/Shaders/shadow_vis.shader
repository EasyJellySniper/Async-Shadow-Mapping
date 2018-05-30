// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

Shader "AsyncShadow/shadow_vis"
{
	SubShader
	{
		Pass
		{
			ZTest Always
			Cull Off
			ZWrite Off

			CGPROGRAM
				#pragma target 5.0
				#pragma only_renderers d3d11

				#pragma vertex MainVs
				#pragma fragment MainPs

				#include "UnityCG.cginc"

				struct VertexInput
				{
					float4 vPositionOs : POSITION;
					float2 vTexCoord : TEXCOORD0;
				};

				struct VertexOutput
				{
					float4 vPositionPs : SV_POSITION;
					float2 vTexCoord : TEXCOORD0;
				};

				VertexOutput MainVs( VertexInput i )
				{
					VertexOutput o;
					o.vPositionPs.xyzw = UnityObjectToClipPos( i.vPositionOs.xyzw );
					o.vTexCoord.xy = float2( i.vTexCoord.x, 1.0 - i.vTexCoord.y );
					return o;
				}

				Texture2D _AsyncShadow;
				SamplerComparisonState sampler_AsyncShadow;

				float4 MainPs( VertexOutput i ) : SV_Target
				{
					float depth = 0.0f;

					#define SAMPLENUM 128.0
					for (int j = 0; j < SAMPLENUM; j++)
					{
						float shadow = (1.0 / SAMPLENUM) * _AsyncShadow.SampleCmpLevelZero(sampler_AsyncShadow,
							i.vTexCoord.xy, j / SAMPLENUM).r;


						depth += shadow;
					}

					depth = pow(depth, 2.0);

					#if defined(UNITY_REVERSED_Z)
						depth = 1.0f - depth;
					#endif

					return float4(depth, depth, depth, 1.0f);
				}
			ENDCG
		}
	}
}
