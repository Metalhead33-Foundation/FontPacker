#version 430 core

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Input image texture
layout (binding = 0, r8) readonly uniform image2D fontTexture;

// Output SDF texture
layout (binding = 1, r8) writeonly uniform image2D sdfTexture;

// Texture dimensions
layout (binding = 2, std140) uniform Dimensions {
    int intendedSampleWidth;
    int intendedSampleHeight;
};

#define MAX_SAMPLES_DIM 4

// Function to calculate the distance to the nearest edge
float calculateDistance(ivec2 threadId, ivec2 texSize, float maxDistance, bool isInside) {
    float minDistance = maxDistance;
    for (int offsetY = -intendedSampleWidth; offsetY <= intendedSampleWidth; ++offsetY) {
	for (int offsetX = -intendedSampleHeight; offsetX <= intendedSampleHeight; ++offsetX) {
	    ivec2 samplePos = clamp(threadId + ivec2(offsetX, offsetY), ivec2(0, 0), texSize - ivec2(1));
	    float pointSample = imageLoad(fontTexture, samplePos).r;
	    bool isEdge = (pointSample > 0.5) != isInside;
	    if (isEdge) {
		float dist = length(vec2(offsetX, offsetY));
		if (dist < minDistance) {
		    minDistance = dist;
		}
	    }
	}
    }
    return minDistance / maxDistance;
}

void main() {
    ivec2 threadId = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imageDimensions = imageSize(sdfTexture);
    if (threadId.x >= imageDimensions.x || threadId.y >= imageDimensions.y) {
	return;
    }

    float pixel = imageLoad(fontTexture, threadId).r;
    float maxDistance = length(vec2(intendedSampleWidth, intendedSampleHeight));
    bool isInside = pixel > 0.5;
    float sdfValue = calculateDistance(threadId, imageDimensions, maxDistance, isInside);

    // Normalize sdfValue to [-0.5, 0.5] range and then to [0, 1]
    sdfValue = 1.0 - (isInside ? 0.5 - (sdfValue / 2.0) : 0.5 + (sdfValue / 2.0));

    // Write the SDF value to the output texture
    imageStore(sdfTexture, threadId, vec4(sdfValue, sdfValue, sdfValue, 1.0));
}
