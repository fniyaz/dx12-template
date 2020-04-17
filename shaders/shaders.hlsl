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
	float3 light = float3(0.,0., 0);

	float4 diff = input.color / 2.;


	float alpha = ((0 - input.origin.x) * (0 - input.origin.x) + (2- input.origin.y) * (2-input.origin.y) + input.origin.z * input.origin.z) * 10;
	

	float3 shx = cross(float3(1, 1, 0), input.norm);
	float3 shy = cross(shx, input.norm);
	float3 alt_norm = input.norm + (shx * sin(alpha) / 9.) + (shy * cos(alpha) / 9.);

	float3 to_light = light - input.origin.xyz;
	float k = abs(dot(to_light, alt_norm) / length(to_light) / length(alt_norm));
	float4 col = k * diff;


	if (input.position.z <= 0)
		return float4(0, 0, 0, 0); 
	return diff + col;
	//return float4(abs(input.norm) / 2. + float3(.5, .5, .5), 1);
}
