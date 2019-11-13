/////////////
// GLOBALS //
/////////////
cbuffer PSConstants
{
	float4 lightVectorWorld;
};

Texture2D shaderTexture;
SamplerState SampleType;

//////////////
// TYPEDEFS //
//////////////
struct PixelInputType
{
	float4 normalWC : TEXCOORD0;
	float4 color : TEXCOORD1;
	//float4 position : SV_POSITION;
};

////////////////////////////////////////////////////////////////////////////////
// Pixel Shader
////////////////////////////////////////////////////////////////////////////////
float4 GenericPixelShader(PixelInputType input) : SV_TARGET
{
	float4 outColor = input.color;

	if (input.color.w < 0.00001)
	{
		float2 uv;
		uv.x = input.color.x;
		uv.y = input.color.y;
		outColor = shaderTexture.Sample(SampleType, uv);
	}

	// determine lighting.
	float NdotL = max(0, dot(input.normalWC, lightVectorWorld));

	// determine final color.
	float4 final = outColor * (0.8 + (NdotL * .4));

	// don't mess with alpha.
	final.w = outColor.w;
	return final;
}

