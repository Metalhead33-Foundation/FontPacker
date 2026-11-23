/**
 * @file screen.vert.glsl
 * @brief Vertex shader for drawing a full-screen textured quad.
 * @version 330 core
 */

#version 330 core
/** @brief Vertex position in clip space. */
layout (location = 0) in vec2 aPos;
/** @brief Vertex texture coordinate. */
layout (location = 1) in vec2 aTexCoords;

/** @brief Interpolated texture coordinate passed to the fragment shader. */
out vec2 TexCoords;

/** @brief Write the clip-space position and texture coordinate. */
void main()
{
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); 
}
