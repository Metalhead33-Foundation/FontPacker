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
// Distance function macro
#ifdef USE_MANHATTAN_DISTANCE
    #define DISTANCE_FUNC(p1, p2) (abs((p1).x - (p2).x) + abs((p1).y - (p2).y))
#else
    #define DISTANCE_FUNC(p1, p2) length((p1) - (p2))
#endif

// Function to calculate the distance to the nearest edge
float calculateDistance(ivec2 threadId, ivec2 texSize, float maxDistance, bool isInside) {
    float minDistance = maxDistance;
    for (int offsetY = -intendedSampleHeight; offsetY <= intendedSampleHeight; ++offsetY) {
        for (int offsetX = -intendedSampleWidth; offsetX <= intendedSampleWidth; ++offsetX) {
            ivec2 samplePos = clamp(threadId + ivec2(offsetX, offsetY), ivec2(0, 0), texSize - ivec2(1));
            float pointSample = imageLoad(fontTexture, samplePos).r;
            bool isEdge = (pointSample > 0.5) != isInside;
            if (isEdge) {
                #ifdef USE_MANHATTAN_DISTANCE
                float dist = abs(float(offsetX)) + abs(float(offsetY));
                #else
                float dist = length(vec2(offsetX, offsetY));
                #endif
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
    ivec2 imageDimensions = imageSize(rawSdfTexture);
    if (threadId.x >= imageDimensions.x || threadId.y >= imageDimensions.y) {
        return;
    }

    float pixel = imageLoad(fontTexture, threadId).r;
    #ifdef USE_MANHATTAN_DISTANCE
    float maxDistance = abs(float(intendedSampleWidth)) + abs(float(intendedSampleHeight));
    #else
    float maxDistance = length(vec2(intendedSampleWidth, intendedSampleHeight));
    #endif
    bool isInside = pixel > 0.5;
    float sdfValue = calculateDistance(threadId, imageDimensions, maxDistance, isInside);

    // We do not normalize sdfValue to [-0.5, 0.5] or [0, 1] here - we do it in a separate pass on the CPU.

    // Write the SDF value to the output texture
    imageStore(rawSdfTexture, threadId, vec4(sdfValue, sdfValue, sdfValue, 1.0));
    imageStore(isInsideTex, threadId, vec4(float(isInside), float(isInside), float(isInside), 1.0));
}
