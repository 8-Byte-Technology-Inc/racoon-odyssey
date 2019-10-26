/////////////
// GLOBALS //
/////////////
cbuffer VSConstants0
{
	matrix viewMatrix;
	matrix projectionMatrix;
};

cbuffer VSConstants1
{
	matrix worldMatrix;
	matrix worldNormalMatrix;
};

cbuffer VSConstants2
{
	int animIndex;
};

cbuffer VSConstants3
{
	matrix jointMatrix[16];
	matrix jointNormalMatrix[16];
};

Texture2D<float> g_boneTexture;

//////////////
// TYPEDEFS //
//////////////
struct VertexInputType
{
	float3 position : POSITION0;
	float3 normal : NORMAL;
	float4 color : COLOR;
	int4 bones : BONES;
	float4 weights : WEIGHTS;
};

struct PixelInputType
{
	float4 normalWC : TEXCOORD0;
	float4 color : TEXCOORD1;
	float4 position : SV_POSITION;
};

matrix GetBoneMatrix(uint animIndex, uint boneIndex, bool normal)
{
	uint xBase = 1 + (animIndex * 8) + (normal ? 4 : 0);
	uint yBase = 1 + (boneIndex * 4);

	matrix val;
	val._m00 = g_boneTexture.Load(uint3(xBase + 0, yBase + 0, 0));
	val._m01 = g_boneTexture.Load(uint3(xBase + 0, yBase + 1, 0));
	val._m02 = g_boneTexture.Load(uint3(xBase + 0, yBase + 2, 0));
	val._m03 = g_boneTexture.Load(uint3(xBase + 0, yBase + 3, 0));

	val._m10 = g_boneTexture.Load(uint3(xBase + 1, yBase + 0, 0));
	val._m11 = g_boneTexture.Load(uint3(xBase + 1, yBase + 1, 0));
	val._m12 = g_boneTexture.Load(uint3(xBase + 1, yBase + 2, 0));
	val._m13 = g_boneTexture.Load(uint3(xBase + 1, yBase + 3, 0));

	val._m20 = g_boneTexture.Load(uint3(xBase + 2, yBase + 0, 0));
	val._m21 = g_boneTexture.Load(uint3(xBase + 2, yBase + 1, 0));
	val._m22 = g_boneTexture.Load(uint3(xBase + 2, yBase + 2, 0));
	val._m23 = g_boneTexture.Load(uint3(xBase + 2, yBase + 3, 0));

	val._m30 = g_boneTexture.Load(uint3(xBase + 3, yBase + 0, 0));
	val._m31 = g_boneTexture.Load(uint3(xBase + 3, yBase + 1, 0));
	val._m32 = g_boneTexture.Load(uint3(xBase + 3, yBase + 2, 0));
	val._m33 = g_boneTexture.Load(uint3(xBase + 3, yBase + 3, 0));

	return val;
}

////////////////////////////////////////////////////////////////////////////////
// Vertex Shader
////////////////////////////////////////////////////////////////////////////////
PixelInputType GenericVertexShader(VertexInputType input)
{
	PixelInputType output;

	// Change the position vector to be 4 units for proper matrix calculations.
	float4 inputPosition;
	inputPosition.x = input.position.x;
	inputPosition.y = input.position.y;
	inputPosition.z = input.position.z;
	inputPosition.w = 1.0f;

	float4 inputNormal;
	inputNormal.x = input.normal.x;
	inputNormal.y = input.normal.y;
	inputNormal.z = input.normal.z;
	inputNormal.w = 1.0f;

	if (input.bones.x < 0)
	{
		output.position = inputPosition;
		output.normalWC = inputNormal;
	}
	else if (animIndex < 0)
	{
		output.position = mul(inputPosition, jointMatrix[input.bones.x]) * input.weights.x;
		output.normalWC = mul(inputNormal, jointNormalMatrix[input.bones.x]) * input.weights.x;

		if (input.bones.y >= 0)
		{
			output.position += mul(inputPosition, jointMatrix[input.bones.y]) * input.weights.y;
			output.normalWC += mul(inputNormal, jointNormalMatrix[input.bones.y]) * input.weights.y;

			if (input.bones.z >= 0)
			{
				output.position += mul(inputPosition, jointMatrix[input.bones.z]) * input.weights.z;
				output.normalWC += mul(inputNormal, jointNormalMatrix[input.bones.z]) * input.weights.z;

				if (input.bones.w >= 0)
				{
					output.position += mul(inputPosition, jointMatrix[input.bones.w]) * input.weights.w;
					output.normalWC += mul(inputNormal, jointNormalMatrix[input.bones.w]) * input.weights.w;
				}
			}

			output.normalWC = normalize(output.normalWC);
		}
	}
	else
	{
		matrix boneMatrix1 = GetBoneMatrix(animIndex, input.bones.x, false);
		output.position = mul(inputPosition, boneMatrix1) * input.weights.x;

		matrix boneNormalMatrix1 = GetBoneMatrix(animIndex, input.bones.x, true);
		output.normalWC = mul(inputNormal, boneNormalMatrix1) * input.weights.x;

		if (input.bones.y >= 0)
		{
			matrix boneMatrix2 = GetBoneMatrix(animIndex, input.bones.y, false);
			output.position += mul(inputPosition, boneMatrix2) * input.weights.y;

			matrix boneNormalMatrix2 = GetBoneMatrix(animIndex, input.bones.y, true);
			output.normalWC += mul(inputNormal, boneNormalMatrix2) * input.weights.y;

			if (input.bones.z >= 0)
			{
				matrix boneMatrix3 = GetBoneMatrix(animIndex, input.bones.z, false);
				output.position += mul(inputPosition, boneMatrix3) * input.weights.z;

				matrix boneNormalMatrix3 = GetBoneMatrix(animIndex, input.bones.z, true);
				output.normalWC += mul(inputNormal, boneNormalMatrix3) * input.weights.z;

				if (input.bones.w >= 0)
				{
					matrix boneMatrix4 = GetBoneMatrix(animIndex, input.bones.w, false);
					output.position += mul(inputPosition, boneMatrix4) * input.weights.w;

					matrix boneNormalMatrix4 = GetBoneMatrix(animIndex, input.bones.w, true);
					output.normalWC += mul(inputNormal, boneNormalMatrix4) * input.weights.w;
				}
			}

			output.normalWC = normalize(output.normalWC);
		}
	}

	// Calculate the position of the vertex against the world, view, and projection matrices.
	output.position = mul(output.position, worldMatrix);
	output.position = mul(output.position, viewMatrix);
	output.position = mul(output.position, projectionMatrix);

	output.normalWC = mul(output.normalWC, worldNormalMatrix);

	output.color = input.color;

	return output;
}

