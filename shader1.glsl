#version 430 core

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Input image texture
layout (binding = 0, r8) readonly uniform image2D fontTexture;

// Output SDF texture
layout (binding = 1, r32f) writeonly uniform image2D rawSdfTexture;
layout (binding = 2, r8) writeonly uniform image2D isInsideTex;

// Texture dimensions
layout (binding = 3, std140) uniform Dimensions {
    int intendedSampleWidth;
    int intendedSampleHeight;
};

#define MAX_SAMPLES_DIM 4

// Function to calculate the distance to the nearest edge
ivec2 calculateDistance(ivec2 threadId, ivec2 texSize, float maxDistance, bool isInside) {
    float minDistance = maxDistance;
    ivec2 offsetDirection(0, 0);
    for (int offsetY = -intendedSampleWidth; offsetY <= intendedSampleWidth; ++offsetY) {
        for (int offsetX = -intendedSampleHeight; offsetX <= intendedSampleHeight; ++offsetX) {
            ivec2 offset = ivec2(offsetX, offsetY)
            ivec2 samplePos = clamp(threadId + offset, ivec2(0, 0), texSize - ivec2(1));
            float pointSample = imageLoad(fontTexture, samplePos).r;
            bool isEdge = (pointSample > 0.5) != isInside;
            if (isEdge) {
                float dist = abs(float(offsetX)) + abs(float(offsetY));
                if (dist < minDistance) {
                    minDistance = dist;
                    offsetDirection = offset;
                }
            }
        }
    }
    return offsetDirection;
}

void main() {
    ivec2 threadId = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imageDimensions = imageSize(rawSdfTexture);
    if (threadId.x >= imageDimensions.x || threadId.y >= imageDimensions.y) {
        return;
    }

    float pixel = imageLoad(fontTexture, threadId).r;
    float maxDistance = length(vec2(intendedSampleWidth, intendedSampleHeight));
    bool isInside = pixel > 0.5;
    ivec2 offsetDirection = calculateDistance(threadId, imageDimensions, maxDistance, isInside);

    // Normalize sdfValue to [-0.5, 0.5] range and then to [0, 1]

    // Write the SDF value to the output texture
    imageStore(rawSdfTexture, threadId, vec4(sdfValue, sdfValue, sdfValue, 1.0));
    imageStore(isInsideTex, threadId, vec4(float(isInside), float(isInside), float(isInside), 1.0));
}
