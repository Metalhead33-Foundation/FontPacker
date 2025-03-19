#version 430

layout(local_size_x = 8, local_size_y = 8) in;
layout (binding = 0, r8) readonly uniform image2D fontTexture;
layout(binding = 1, rgba32f) uniform writeonly image2D rawSdfTexture;

layout (binding = 3, std140) uniform Dimensions {
    int intendedSampleWidth;
    int intendedSampleHeight;
};

// Helper structure to store edge information
struct Edge {
    vec2 p1;
    vec2 p2;
    vec2 normal;
    float angle;
};

// Get pixel value from bitmap (1.0 for inside, 0.0 for outside)
float getBitmapValue(ivec2 pos) {
    return imageLoad(fontTexture, pos).r > 0.5 ? 1.0 : 0.0;
}

// Check if a position is inside our texture bounds
bool isInBounds(ivec2 pos) {
    ivec2 dimensions = imageSize(fontTexture);
    return pos.x >= 0 && pos.y >= 0 && pos.x < dimensions.x && pos.y < dimensions.y;
}

// Find the nearest edge to a given point
Edge findNearestEdge(ivec2 pixelPos) {
    ivec2 dimensions = imageSize(fontTexture);
    float pixelValue = getBitmapValue(pixelPos);

    // Initialize with an invalid edge
    Edge nearestEdge;
    nearestEdge.p1 = vec2(-1.0);
    nearestEdge.p2 = vec2(-1.0);
    float minDist = length(vec2(intendedSampleWidth, intendedSampleHeight));

    // Search in a square area around the pixel
    for (int y = -intendedSampleHeight; y <= intendedSampleHeight; y++) {
        for (int x = -intendedSampleWidth; x <= intendedSampleWidth; x++) {
            ivec2 checkPos = pixelPos + ivec2(x, y);

            if (!isInBounds(checkPos)) continue;

            float checkValue = getBitmapValue(checkPos);

            // If we found a pixel with different value (inside/outside transition)
            if (checkValue != pixelValue) {
                // Check all 8 neighboring pixels of this edge pixel
                for (int ny = -1; ny <= 1; ny++) {
                    for (int nx = -1; nx <= 1; nx++) {
                        if (nx == 0 && ny == 0) continue;

                        ivec2 neighborPos = checkPos + ivec2(nx, ny);
                        if (!isInBounds(neighborPos)) continue;

                        float neighborValue = getBitmapValue(neighborPos);

                        // If neighbor has different value, we found an edge
                        if (neighborValue != checkValue) {
                            // Create an edge between these two points
                            vec2 p1 = vec2(checkPos) + 0.5;
                            vec2 p2 = vec2(neighborPos) + 0.5;
                            vec2 edgeDir = normalize(p2 - p1);
                            vec2 edgeNormal = vec2(-edgeDir.y, edgeDir.x);
                            float edgeAngle = atan(edgeDir.y, edgeDir.x);

                            // Calculate distance from our original pixel to this edge
                            vec2 toPixel = vec2(pixelPos) + 0.5 - p1;
                            float dist = abs(dot(toPixel, edgeNormal));

                            // Keep the nearest edge
                            if (dist < minDist) {
                                minDist = dist;
                                nearestEdge.p1 = p1;
                                nearestEdge.p2 = p2;
                                nearestEdge.normal = edgeNormal;
                                nearestEdge.angle = edgeAngle;
                            }
                        }
                    }
                }
            }
        }
    }

    return nearestEdge;
}

// Calculate the minimum distance to an edge
float computeDistance(vec2 pixel, Edge edge) {
    vec2 lineVec = edge.p2 - edge.p1;
    vec2 pointVec = pixel - edge.p1;

    float t = clamp(dot(pointVec, lineVec) / dot(lineVec, lineVec), 0.0, 1.0);
    vec2 projection = edge.p1 + t * lineVec;

    return length(pixel - projection);
}

// Assign distance to appropriate color channel based on edge angle
vec3 assignToChannels(float distance, float angle, bool isInside) {
    float MAX_DISTANCE = length(vec2(intendedSampleWidth, intendedSampleHeight));
    // Convert angle to positive range [0, 2π]
    angle = mod(angle + 2.0 * 3.14159, 2.0 * 3.14159);

    // Normalize distance to [0, 1] range
    float normalizedDist = clamp(distance / MAX_DISTANCE, 0.0, 1.0);

    // Apply sign based on whether pixel is inside or outside
    float signedDist = isInside ? -normalizedDist : normalizedDist;

    // Map to [0, 1] for storage in texture
    float mappedDist = signedDist * 0.5 + 0.5;

    // Determine which channels to store the distance in based on edge angle
    // We use three 120° sectors:
    // - Red: angles around 0° (horizontal edges)
    // - Green: angles around 120° (diagonal edges)
    // - Blue: angles around 240° (vertical edges)
    vec3 channels = vec3(1.0);  // Initialize with max distance

    if (angle < 2.0 * 3.14159 / 3.0) {
        // First sector: primarily red
        channels.r = mappedDist;
    } else if (angle < 4.0 * 3.14159 / 3.0) {
        // Second sector: primarily green
        channels.g = mappedDist;
    } else {
        // Third sector: primarily blue
        channels.b = mappedDist;
    }

    return channels;
}

void main() {
    // Get current pixel coordinates
    ivec2 texCoord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dimensions = imageSize(fontTexture);

    // Skip if outside texture dimensions
    if (texCoord.x >= dimensions.x || texCoord.y >= dimensions.y) {
        return;
    }

    // Check if pixel is inside or outside shape
    bool isInside = getBitmapValue(texCoord) > 0.5;

    // Find nearest edge
    Edge nearestEdge = findNearestEdge(texCoord);

    if (nearestEdge.p1.x < 0) {
        // No edge found within search radius, assign maximum distance
        float value = isInside ? 0.0 : 1.0;  // 0.5 maps to 0 distance
        imageStore(rawSdfTexture, texCoord, vec4(value, value, value, 1.0));
    } else {
        // Compute exact distance to edge
        float distance = computeDistance(vec2(texCoord) + 0.5, nearestEdge);

        // Assign distance to appropriate channels based on edge angle
        vec3 msdfValues = assignToChannels(distance, nearestEdge.angle, isInside);

        // Store result
        imageStore(rawSdfTexture, texCoord, vec4(msdfValues, 1.0));
    }
}
