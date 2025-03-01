#version 430 core
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
// Define the layout of the input image
layout(r8, binding = 0) writeonly uniform image2D outputImage;

void main() {
    // Get the dimensions of the image
    //ivec2 imageDimensions = imageSize(outputImage);

    ivec2 imageDimensions = ivec2(1024,1024);

    // Get the current pixel's coordinates
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

    // Normalize the pixel coordinates to the range [0.0, 1.0]
    vec2 normalizedCoord = vec2(pixelCoord) / vec2(imageDimensions);

    // Compute the greyscale value based on the x-coordinate
    float greyscaleValue = normalizedCoord.x;

    // Write the greyscale value to the texture
    imageStore(outputImage, pixelCoord, vec4(greyscaleValue, greyscaleValue, greyscaleValue, 1.0));
}
