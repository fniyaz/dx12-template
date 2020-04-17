cbuffer ConstantBuffer: register(b0)
{
	float4x4 mvpMatrix;
}

struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
	PSInput result;

	result.position = mul(mvpMatrix, position);
	result.color = color;

	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	if (input.position.z <= 0)
		return float4(0, 0, 0, 0); 
	return input.color;
}
