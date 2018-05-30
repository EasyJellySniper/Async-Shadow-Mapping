Shader "Custom/SqStandard" 
{
		Properties
		{
			_Color("Color", Color) = (1.0,1.0,1.0,1.0)
			_MainTex("Texture", 2D) = "white" {}
		}

	SubShader
	{
		Pass
		{
			Tags{ "LightMode" = "ForwardBase" }

			CGPROGRAM
			#pragma vertex vert
			#pragma fragment frag
			#pragma target 5.0
			#pragma only_renderers d3d11
			#pragma multi_compile_instancing
			#include "UnityCG.cginc" // for UnityObjectToWorldNormal
			#include "UnityLightingCommon.cginc" // for _LightColor0
			#include "SqShadow.cginc"

			struct appdata
			{
				float4 vertex : POSITION;
				float3 normal : NORMAL;
				float2 texcoord : TEXCOORD;
				UNITY_VERTEX_INPUT_INSTANCE_ID
			};

			struct v2f
			{
				float2 uv : TEXCOORD0;
				float3 normal : NORMAL;
				float4 vertex : SV_POSITION;
				float4 shadowPos : TEXCOORD1;
				UNITY_VERTEX_INPUT_INSTANCE_ID
			};

			v2f vert(appdata v)
			{
				// instancing setup
				UNITY_SETUP_INSTANCE_ID(v);

				v2f o;
				o.vertex = UnityObjectToClipPos(v.vertex);
				o.uv = v.texcoord;

				// get vertex normal in world space
				half3 worldNormal = UnityObjectToWorldNormal(v.normal);
				o.normal = v.normal;

				float4 posW = mul(unity_ObjectToWorld, v.vertex);
				o.shadowPos = mul(_AsyncShadowMatrix, posW);

				UNITY_TRANSFER_INSTANCE_ID(v, o);

				return o;
			}

			sampler2D _MainTex;

			UNITY_INSTANCING_BUFFER_START(Props)
			UNITY_DEFINE_INSTANCED_PROP(float4, _Color)
			UNITY_INSTANCING_BUFFER_END(Props)

			fixed4 frag(v2f i) : SV_Target
			{
				UNITY_SETUP_INSTANCE_ID(i);

				// sample texture
				fixed4 col = tex2D(_MainTex, i.uv);
				col *= UNITY_ACCESS_INSTANCED_PROP(Props, _Color);

				half nl = max(0, dot(i.normal, _WorldSpaceLightPos0.xyz));
				float4 diff = nl * _LightColor0;
				diff.rgb *= CalcShadowFactor(i.shadowPos);
				col *= diff;

				return col;
			}
			ENDCG
		}
	}
	FallBack "Diffuse"
}
