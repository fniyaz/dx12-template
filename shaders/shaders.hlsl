cbuffer ConstantBuffer: register(b0)
{
	float4x4 mvpMatrix;
}

struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
	float3 norm : NORMAL;
	float3 origin : POSITION;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR, float3 norm : NORMAL)
{
	PSInput result;

	result.position = mul(mvpMatrix, position);
	result.color = color;
	result.norm = norm;
	result.origin = position.xyz / position.w;

	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	float3 light = float3(1.,1.,0.);

	float4 diff = input.color / 2.;
	float4 col = abs(dot((light - input.origin.xyz), input.norm)) * diff;


	if (input.position.z <= 0)
		return float4(0, 0, 0, 0); 
	return diff + col;
}
