#version 430 core
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
// RGBA
// RGB = the 3 channels of the MSDF
// A = just a regular SDF
layout (binding = 1, rgba32f) readonly uniform image2D sdf_input;
layout (binding = 2, rgba32f) writeonly uniform image2D sdf_output;

float threshold = 1.0; // Tune this based on actual data; usually around 1.0 works well

bool isFalseEdge(vec3 a, vec3 b) {
    int jumpCount = 0;
    if (abs(a.r - b.r) > threshold && sign(a.r) != sign(b.r)) jumpCount++;
    if (abs(a.g - b.g) > threshold && sign(a.g) != sign(b.g)) jumpCount++;
    if (abs(a.b - b.b) > threshold && sign(a.b) != sign(b.b)) jumpCount++;
    return jumpCount >= 2;
}

float median3(float a, float b, float c) {
    return max(min(a, b), min(max(a, b), c));
}

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
