/**
 * @file shader1.glsl
 * @brief OpenGL compute shader for generating single-channel SDF from bitmap images.
 * 
 * This shader computes signed distance fields from rasterized bitmap images.
 * It samples the input texture in a neighborhood around each pixel to find
 * the nearest edge and calculates the distance using either Manhattan or
 * Euclidean distance metrics.
 * 
 * @version 430 core
 * @requires OpenGL 4.3+ with compute shader support
 */

#version 430 core

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

/**
 * @brief Input bitmap texture (8-bit grayscale).
 * @binding 0
 */
layout (binding = 0, r8) readonly uniform image2D fontTexture;

/**
 * @brief Output SDF texture (32-bit float, single channel).
 * @binding 1
 */
layout (binding = 1, r32f) writeonly uniform image2D rawSdfTexture;

/**
 * @brief Output inside/outside mask texture (8-bit).
 * @binding 2
 */
layout (binding = 2, r8) writeonly uniform image2D isInsideTex;

/**
 * @brief Uniform buffer containing texture dimensions.
 * @binding 3
 * @struct Dimensions
 */
layout (binding = 3, std140) uniform Dimensions {
    int intendedSampleWidth;   ///< Sample search width in pixels
    int intendedSampleHeight; ///< Sample search height in pixels
};

/**
 * @brief Distance function macro.
 * 
 * Selects between Manhattan (L1) and Euclidean (L2) distance based on
 * USE_MANHATTAN_DISTANCE preprocessor define.
 */
#ifdef USE_MANHATTAN_DISTANCE
    #define DISTANCE_FUNC(p1, p2) (abs((p1).x - (p2).x) + abs((p1).y - (p2).y))
#else
    #define DISTANCE_FUNC(p1, p2) length((p1) - (p2))
#endif

/**
 * @brief Calculate the distance to the nearest edge from a pixel position.
 * 
 * Searches in a neighborhood defined by intendedSampleWidth/Height to find
 * the nearest edge pixel and computes the distance.
 * 
 * @param threadId Current pixel position (integer coordinates).
 * @param texSize Texture dimensions.
 * @param maxDistance Maximum search distance.
 * @param isInside Whether the current pixel is inside the shape.
 * @return Normalized distance (0.0 to 1.0).
 */
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

/**
 * @brief Main compute shader entry point.
 * 
 * For each pixel in the output texture:
 * 1. Determines if the pixel is inside or outside the shape
 * 2. Calculates the distance to the nearest edge
 * 3. Writes the SDF value and inside/outside flag to output textures
 * 
 * Note: SDF values are not normalized here; normalization is performed
 * in a separate CPU pass.
 */
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
