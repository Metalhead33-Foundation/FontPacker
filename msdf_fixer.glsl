/**
 * @file msdf_fixer.glsl
 * @brief OpenGL compute shader for fixing false edges in MSDF textures.
 * 
 * This shader detects and fixes false edges in multi-channel signed distance fields.
 * False edges occur when multiple MSDF channels cross zero at the same location,
 * creating artifacts. The shader detects these cases and replaces the RGB channels
 * with the median value or the alpha channel (regular SDF).
 * 
 * @version 430 core
 * @requires OpenGL 4.3+ with compute shader support
 */

#version 430 core
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

/**
 * @brief Input MSDF texture (RGBA32F).
 * RGB = MSDF channels, A = regular SDF.
 * @binding 1
 */
layout (binding = 1, rgba32f) readonly uniform image2D sdf_input;

/**
 * @brief Output fixed MSDF texture (RGBA32F).
 * @binding 2
 */
layout (binding = 2, rgba32f) writeonly uniform image2D sdf_output;

/**
 * @brief Threshold for detecting false edges.
 * 
 * If two or more channels cross zero (change sign) with a difference
 * greater than this threshold, it's considered a false edge.
 * 
 * @note Tune this based on actual data; usually around 1.0 works well.
 */
float threshold = 1.0;

/**
 * @brief Check if a transition between two MSDF values is a false edge.
 * 
 * A false edge is detected when 2 or more channels cross zero simultaneously.
 * 
 * @param a First MSDF value (RGB channels).
 * @param b Second MSDF value (RGB channels).
 * @return True if this is a false edge.
 */
bool isFalseEdge(vec3 a, vec3 b) {
    int jumpCount = 0;
    if (abs(a.r - b.r) >= threshold && sign(a.r) != sign(b.r)) jumpCount++;
    if (abs(a.g - b.g) >= threshold && sign(a.g) != sign(b.g)) jumpCount++;
    if (abs(a.b - b.b) >= threshold && sign(a.b) != sign(b.b)) jumpCount++;
    return jumpCount >= 2;
}

/**
 * @brief Calculate median of three values.
 * @param a First value.
 * @param b Second value.
 * @param c Third value.
 * @return Median value.
 */
float median3(float a, float b, float c) {
    return max(min(a, b), min(max(a, b), c));
}

/**
 * @brief Main compute shader entry point.
 * 
 * For each pixel:
 * 1. Samples 4-neighborhood (up, down, left, right)
 * 2. Detects false edges by checking for simultaneous channel sign changes
 * 3. If false edge detected, replaces RGB with alpha channel (regular SDF)
 * 4. Otherwise, passes through the original MSDF values
 */
void main()
{
    ivec2 threadId = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dims = imageSize(sdf_input);
    if (threadId.x >= dims.x || threadId.y >= dims.y) return;

    vec4 center = imageLoad(sdf_input, threadId);
    vec3 centerRGB = center.rgb;

    bool falseEdgeDetected = false;

    // Offsets to sample 4 direct neighbors (up, down, left, right)
    ivec2 offsets[4] = ivec2[](
        ivec2(-1, 0), ivec2(1, 0),
        ivec2(0, -1), ivec2(0, 1)
    );

    for (int i = 0; i < 4; ++i) {
        ivec2 neighborCoord = threadId + offsets[i];
        if (neighborCoord.x < 0 || neighborCoord.x >= dims.x ||
            neighborCoord.y < 0 || neighborCoord.y >= dims.y)
            continue;

        vec3 neighborRGB = imageLoad(sdf_input, neighborCoord).rgb;
        if (isFalseEdge(centerRGB, neighborRGB)) {
            falseEdgeDetected = true;
            break;
        }
    }

    vec4 outputColor;

    if (falseEdgeDetected) {
        float med = median3(centerRGB.r, centerRGB.g, centerRGB.b);
        //outputColor = vec4(vec3(med), center.a); // Use median for RGB, keep alpha as-is
        outputColor = vec4(center.a); // Use median for RGB, keep alpha as-is
    } else {
        outputColor = center;
    }

    imageStore(sdf_output, threadId, outputColor);
}
