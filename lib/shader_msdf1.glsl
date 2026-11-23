/**
 * @file shader_msdf1.glsl
 * @brief OpenGL compute shader for generating multi-channel SDF (MSDF) from bitmap images.
 * 
 * This shader computes a simplified multi-channel signed distance field from
 * rasterized bitmap images. Each channel (R, G, B) tracks the distance to the
 * nearest edge in a different direction (horizontal, vertical, diagonal).
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
 * @brief Output MSDF texture (32-bit float, RGB channels).
 * @binding 1
 */
layout (binding = 1, r32f) writeonly uniform image2D rawSdfTexture;

/**
 * @brief Output inside/outside mask texture (RGBA8).
 * @binding 2
 */
layout (binding = 2, rgba8) writeonly uniform image2D isInsideTex;

/**
 * @brief Uniform buffer containing texture dimensions.
 * @binding 3
 * @struct Dimensions
 */
layout (binding = 3, std140) uniform Dimensions {
    int intendedSampleWidth;   ///< Sample search width in pixels
    int intendedSampleHeight; ///< Sample search height in pixels
};

#define MAX_SAMPLES_DIM 4

/**
 * @brief Calculate multi-channel distance to nearest edges.
 * 
 * Searches in three directions:
 * - Red channel: horizontal (X-axis) direction
 * - Green channel: vertical (Y-axis) direction
 * - Blue channel: diagonal direction
 * 
 * @param threadId Current pixel position.
 * @param texSize Texture dimensions.
 * @param maxDistance Maximum search distance.
 * @param isInside Whether the current pixel is inside the shape.
 * @return vec3 containing distances for R, G, B channels (normalized 0.0-1.0).
 */
vec3 calculateDistance(ivec2 threadId, ivec2 texSize, float maxDistance, bool isInside) {
    vec3 minDistance = vec3(maxDistance,maxDistance,maxDistance);
    // Calculate red channel - the closest edge in the horizontal (X-axis) direction.
    for (int offsetX = -intendedSampleWidth; offsetX <= intendedSampleWidth; ++offsetX) {
        ivec2 samplePos = clamp(threadId + ivec2(offsetX, 0), ivec2(0, 0), texSize - ivec2(1));
        float pointSample = imageLoad(fontTexture, samplePos).r;
        bool isEdge = (pointSample > 0.5) != isInside;
        if (isEdge) {
            float dist = length(vec2(offsetX, 0));
            if (dist < minDistance.r) {
                minDistance.r = dist;
            }
        }
    }
    // Calculate green channel - the closest edge in the vertical (Y-axis) direction.
    for (int offsetY = -intendedSampleHeight; offsetY <= intendedSampleHeight; ++offsetY) {
        ivec2 samplePos = clamp(threadId + ivec2(0, offsetY), ivec2(0, 0), texSize - ivec2(1));
        float pointSample = imageLoad(fontTexture, samplePos).r;
        bool isEdge = (pointSample > 0.5) != isInside;
        if (isEdge) {
            float dist = length(vec2(0, offsetY));
            if (dist < minDistance.g) {
                minDistance.g = dist;
            }
        }
    }
    // Calculate blue channel - the closest edge in a diagonal direction
    int maxStep = min(intendedSampleWidth,intendedSampleHeight);
    for (int offset = -maxStep; offset <= maxStep; ++offset) {
        ivec2 samplePos = clamp(threadId + ivec2(offset, offset), ivec2(0, 0), texSize - ivec2(1));
        float pointSample = imageLoad(fontTexture, samplePos).r;
        bool isEdge = (pointSample > 0.5) != isInside;
        if (isEdge) {
            float dist = length(vec2(offset, offset));
            if (dist < minDistance.b) {
                minDistance.b = dist;
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
    float maxDistance = length(vec2(intendedSampleWidth, intendedSampleHeight));
    bool isInside = pixel > 0.5;
    vec3 sdfValue = calculateDistance(threadId, imageDimensions, maxDistance, isInside);

    // Normalize sdfValue to [-0.5, 0.5] range and then to [0, 1] later, in a separate pass on the CPU

    // Write the SDF value to the output texture
    imageStore(rawSdfTexture, threadId, vec4(sdfValue, 1.0));
    imageStore(isInsideTex, threadId, vec4(float(isInside), float(isInside), float(isInside), 1.0));
}
