#version 430 core

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Output SDF texture
layout (binding = 1, r32f) writeonly uniform image2D rawSdfTexture;
layout (binding = 2, r8) writeonly uniform image2D isInsideTex;

struct EdgeSegment {
    int type;
    vec2 points[4];
};

layout(std430, binding = 3) buffer EdgeBuffer {
    EdgeSegment edges[];
};


void main(void)
{
    ivec2 threadId = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imageDimensions = imageSize(rawSdfTexture);
    if (threadId.x >= imageDimensions.x || threadId.y >= imageDimensions.y) {
	return;
    }
    vec2 pos = vec2(threadId) + vec2(0.5);
    int i = 0;
    float minLength = 999999.0;
    for(i = 0; i < edges.length(); ++i) {
	// TODO: actually do the separate types of edge segment types.
	// Placeholder: calculating distance to first point.
	float dist = length(pos - edges[i].points[0]);
	if(dist <= minLength) {
	    minLength = dist;
	}
    }
    float sdfValue = minLength;
    // Placeholder - we will find a way to measure this later
    bool isInside = true;
    //imageStore(rawSdfTexture, threadId, vec4(pos.x, pos.x, pos.x, 1.0));
    imageStore(rawSdfTexture, threadId, vec4(sdfValue, sdfValue, sdfValue, 1.0));
    imageStore(isInsideTex, threadId, vec4(float(isInside), float(isInside), float(isInside), 1.0));
}
