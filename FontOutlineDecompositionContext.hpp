/**
 * @file FontOutlineDecompositionContext.hpp
 * @brief Structures and functions for decomposing font outlines into edge segments.
 * 
 * Provides functionality for converting FreeType font outlines and SVG paths
 * into edge segments (lines, quadratic and cubic Bezier curves) for SDF generation.
 */

#ifndef FONTOUTLINEDECOMPOSITIONCONTEXT_HPP
#define FONTOUTLINEDECOMPOSITIONCONTEXT_HPP
#include <functional>
#include <glm/glm.hpp>
#include <cstdint>
#include <array>
#include <vector>
#include <span>
#include <map>

/**
 * @brief Type of edge segment.
 * @enum EdgeType
 */
enum class EdgeType : int32_t {
	LINEAR,    ///< A line segment. Only first two points are relevant.
	QUADRATIC, ///< A quadratic Bezier curve. Only first three points are relevant.
	CUBIC      ///< A cubic Bezier curve. All four points are relevant.
};

/**
 * @brief Point indices for linear edge segments.
 * @enum LinePart
 */
enum LinePart {
	LINE_P1 = 0,  ///< First point (start)
	LINE_P2 = 1   ///< Second point (end)
};

/**
 * @brief Point indices for quadratic Bezier edge segments.
 * @enum QuadraticPart
 */
enum QuadraticPart {
	QUADRATIC_P1 = 0,      ///< First point (start)
	QUADRATIC_CONTROL = 1, ///< Control point
	QUADRATIC_P2 = 2      ///< Second point (end)
};

/**
 * @brief Point indices for cubic Bezier edge segments.
 * @enum CubicPart
 */
enum CubicPart {
	CUBIC_P1 = 0,        ///< First point (start)
	CUBIC_CONTROL1 = 1, ///< First control point
	CUBIC_CONTROL2 = 2, ///< Second control point
	CUBIC_P2 = 3        ///< Second point (end)
};

/**
 * @brief Edge color for MSDF generation.
 * @enum EdgeColor
 * Colors are used in multi-channel signed distance fields to identify edge directions.
 */
enum class EdgeColor : uint32_t {
	BLACK = 0x000000,   ///< Black channel
	RED = 0xFF0000,     ///< Red channel
	GREEN = 0x00FF00,   ///< Green channel
	YELLOW = 0xFFFF00,  ///< Yellow channel (red + green)
	BLUE = 0x0000FF,    ///< Blue channel
	MAGENTA = 0xFF00FF, ///< Magenta channel (red + blue)
	CYAN = 0x00FFFF,   ///< Cyan channel (green + blue)
	WHITE = 0xFFFFFF   ///< White channel (all colors)
};

/**
 * @brief Orientation of a contour or edge.
 * @enum Orientation
 */
enum class Orientation {
	CW = 0,      ///< Clockwise
	CCW = 1,     ///< Counterclockwise
	COLINEAR = 2 ///< Colinear (degenerate case)
};

/**
 * @brief Check orientation of two points (always colinear).
 * @param A First point.
 * @param B Second point.
 * @return Always returns COLINEAR.
 */
Orientation checkOrientation(const glm::fvec2& A, const glm::fvec2& B);

/**
 * @brief Check orientation of three points (triangle winding).
 * @param A First point.
 * @param B Second point.
 * @param C Third point.
 * @return Orientation (CW, CCW, or COLINEAR).
 */
Orientation checkOrientation(const glm::fvec2& A, const glm::fvec2& B, const glm::fvec2& C);

/**
 * @brief Check orientation of a quadratic Bezier curve.
 * @param p1 Start point.
 * @param control Control point.
 * @param p2 End point.
 * @return Orientation of the curve.
 */
Orientation checkQuadraticOrientation(const glm::fvec2& p1, const glm::fvec2& control, const glm::fvec2& p2);

/**
 * @brief Check orientation of a cubic Bezier curve.
 * @param p1 Start point.
 * @param c1 First control point.
 * @param c2 Second control point.
 * @param p2 End point.
 * @return Orientation of the curve.
 */
Orientation checkCubicOrientation(const glm::fvec2& p1, const glm::fvec2& c1,const glm::fvec2& c2, const glm::fvec2& p2);

/**
 * @brief Represents a single edge segment (line, quadratic, or cubic Bezier).
 * @struct EdgeSegment
 */
struct EdgeSegment {
	EdgeType type;                      ///< Type of edge (LINEAR, QUADRATIC, or CUBIC)
	int32_t contourId;                  ///< ID of the contour this edge belongs to
	uint32_t clr;                       ///< RRGGBBXX encoded edge color for MSDF
	int32_t padding;                    ///< Padding for std430 alignment in OpenGL SSBOs
	std::array<glm::fvec2,4> points;    ///< Control points (up to 4, depending on type)
	
	/**
	 * @brief Get minimum X coordinate of the edge's bounding box.
	 * @return Minimum X value.
	 */
	float getMinX() const;
	
	/**
	 * @brief Get minimum Y coordinate of the edge's bounding box.
	 * @return Minimum Y value.
	 */
	float getMinY() const;
	
	/**
	 * @brief Get maximum X coordinate of the edge's bounding box.
	 * @return Maximum X value.
	 */
	float getMaxX() const;
	
	/**
	 * @brief Get maximum Y coordinate of the edge's bounding box.
	 * @return Maximum Y value.
	 */
	float getMaxY() const;
	
	/**
	 * @brief Invert the edge direction (swap start and end points).
	 */
	void invert();
	
	/**
	 * @brief Evaluate the edge at a parameter value.
	 * @param param Parameter value (0.0 = start, 1.0 = end).
	 * @return Point on the edge at the given parameter.
	 */
	glm::fvec2 point(float param=0.0f) const;
	
	/**
	 * @brief Get the direction vector at a parameter value.
	 * @param param Parameter value.
	 * @return Direction vector (tangent to the curve).
	 */
	glm::fvec2 direction(float param=0.0f) const;
	
	/**
	 * @brief Get the direction change (second derivative) at a parameter value.
	 * @param param Parameter value.
	 * @return Direction change vector.
	 */
	glm::fvec2 directionChange(float param=0.0f) const;
	
	/**
	 * @brief Split the edge into three equal segments.
	 * @param a First segment (output).
	 * @param b Second segment (output).
	 * @param c Third segment (output).
	 */
	void splitIntoThree(EdgeSegment& a, EdgeSegment& b, EdgeSegment& c) const;
	
	/**
	 * @brief Check the orientation of this edge.
	 * @return Orientation (CW, CCW, or COLINEAR).
	 */
	Orientation checkOrientation() const;
	
	/**
	 * @brief Deconverge the edge by adjusting a control point.
	 * @param param Parameter index of the point to adjust.
	 * @param vector Adjustment vector.
	 */
	void deconverge(int param, const glm::fvec2& vector);
	
	/**
	 * @brief Find scanline intersections with a horizontal line.
	 * @param x Array to store X coordinates of intersections (output).
	 * @param dy Array to store direction indicators (output).
	 * @param y Y coordinate of the scanline.
	 * @return Number of intersections found.
	 */
	int scanlineIntersections(double x[], int dy[3], double y) const;
};

/**
 * @brief Compute the winding number of a contour.
 * @param contour Span of edge segments forming the contour.
 * @param samplesPerCurve Number of samples per curve for evaluation (default: 10).
 * @return Orientation of the contour.
 */
Orientation computeWinding(const std::span<const EdgeSegment>& contour, int samplesPerCurve = 10);

/**
 * @brief Check if a point is inside a contour using ray casting.
 * @param point Point to test.
 * @param edges All edge segments.
 * @param contourId ID of the contour to test against.
 * @return True if point is inside the contour.
 */
bool isPointInContour(const glm::fvec2& point, const std::vector<EdgeSegment>& edges, int32_t contourId);

/**
 * @brief Check if one contour is inside another.
 * @param innerContour Inner contour to test.
 * @param allEdges All edge segments.
 * @param outerContourId ID of the outer contour.
 * @param samplesPerEdge Number of sample points per edge (default: 5).
 * @return True if inner contour is inside outer contour.
 */
bool isContourInContour(const std::span<const EdgeSegment>& innerContour, const std::vector<EdgeSegment>& allEdges, int32_t outerContourId, int samplesPerEdge = 5);

/**
 * @brief Bounding box structure.
 * @struct BoundingBox
 */
struct BoundingBox {
	float top, bottom, left, right;  ///< Bounding box boundaries
};

/**
 * @brief Information about a contour.
 * @struct ContourInfo
 */
struct ContourInfo {
	BoundingBox bb;      ///< Bounding box of the contour
	int32_t contourId;   ///< Unique ID of the contour
};

/**
 * @brief Context for decomposing font outlines into edge segments.
 * @struct FontOutlineDecompositionContext
 * 
 * Maintains state for building edge segments from FreeType outlines or SVG paths.
 * Provides methods for path construction, contour management, and edge coloring.
 */
struct FontOutlineDecompositionContext {
	typedef std::vector<std::vector<size_t>> IdShapeMap;              ///< Map of shape IDs to edge indices
	typedef std::function<void(const IdShapeMap&)> IdShapeMapIterator; ///< Iterator function type
	typedef std::pair<int32_t,int32_t> ContourDefinition;              ///< Contour definition (start, end indices)
	typedef std::map<int32_t,ContourDefinition> ContourMap;            ///< Map of contour IDs to definitions
	typedef std::vector<ContourDefinition> ContourVector;               ///< Vector of contour definitions
	
	std::vector<ContourInfo> contourInfo;        ///< Information about each contour
	glm::fvec2 curPos = glm::fvec2(0.0f, 0.0f); ///< Current pen position
	glm::fvec2 firstPointInContour = glm::fvec2(0.0f, 0.0f); ///< First point in current contour
	std::vector<EdgeSegment> edges;              ///< All edge segments
	std::vector<EdgeSegment> stagingEdges;      ///< Temporary staging area for edges
	int32_t curShapeId = 0;                     ///< Current shape ID
	
	/**
	 * @brief Close the current shape/contour.
	 * @param checkWinding Whether to check and correct winding order.
	 */
	void closeShape(bool checkWinding=false);
	
	/**
	 * @brief Move to a new position (start new contour).
	 * @param to Target position.
	 * @param checkWinding Whether to check winding order.
	 * @return 0 on success.
	 */
	int moveTo(const glm::fvec2& to, bool checkWinding=false);
	
	/**
	 * @brief Draw a line to a position.
	 * @param to Target position.
	 * @return 0 on success.
	 */
	int lineTo(const glm::fvec2& to);
	
	/**
	 * @brief Draw a quadratic Bezier curve.
	 * @param control Control point.
	 * @param to End point.
	 * @return 0 on success.
	 */
	int conicTo(const glm::fvec2& control, const glm::fvec2&  to);
	
	/**
	 * @brief Draw a cubic Bezier curve.
	 * @param control1 First control point.
	 * @param control2 Second control point.
	 * @param to End point.
	 * @return 0 on success.
	 */
	int cubicTo(const glm::fvec2& control1, const glm::fvec2& control2, const glm::fvec2& to);
	
	/**
	 * @brief Translate and scale edges to a new size.
	 * @param nWidth New width.
	 * @param nHeight New height.
	 * @param paddingX Horizontal padding.
	 * @param paddingY Vertical padding.
	 * @param invertY Whether to invert Y coordinates (default: true).
	 */
	void translateToNewSize(unsigned nWidth, unsigned nHeight, unsigned paddingX, unsigned paddingY, bool invertY = true);
	
	/**
	 * @brief Translate and scale edges to a new size with metrics.
	 * @param nWidth New width.
	 * @param nHeight New height.
	 * @param paddingX Horizontal padding.
	 * @param paddingY Vertical padding.
	 * @param metricWidth Font metric width.
	 * @param metricHeight Font metric height.
	 * @param horiBearingX Horizontal bearing X.
	 * @param horiBearingY Horizontal bearing Y.
	 * @param invertY Whether to invert Y coordinates (default: true).
	 */
	void translateToNewSize(unsigned nWidth, unsigned nHeight, unsigned paddingX, unsigned paddingY, double metricWidth, double metricHeight, double horiBearingX, double horiBearingY, bool invertY = true);
	
	/**
	 * @brief Check if edges are within a bounding box.
	 * @param xOffset X offset of the bounding box.
	 * @param yOffset Y offset of the bounding box.
	 * @param width Width of the bounding box.
	 * @param height Height of the bounding box.
	 * @return True if edges are within the box.
	 */
	bool isWithinBoundingBox(unsigned xOffset, unsigned yOffset, unsigned width, unsigned height);
	
	/**
	 * @brief Iterate over contours grouped by shape.
	 * @param shapeIterator Function to call for each shape's contour map.
	 */
	void iterateOverContours(const IdShapeMapIterator& shapeIterator) const;
	
	/**
	 * @brief Compute signed area of a contour.
	 * @param contourEdges Edge segments forming the contour.
	 * @param subdivisions Number of subdivisions for curve evaluation (default: 20).
	 * @return Signed area (positive for CCW, negative for CW).
	 */
	static float computeSignedArea(const std::span<const EdgeSegment>& contourEdges, int subdivisions = 20);
	
	/**
	 * @brief Normalize a contour (ensure consistent orientation).
	 * @param contour Contour edges to normalize (modified in place).
	 */
	static void normalizeContour(std::vector<EdgeSegment>& contour);
	
	/**
	 * @brief Produce a map of contour IDs to contour definitions.
	 * @return Contour map.
	 */
	ContourMap produceContourMap() const;
	
	/**
	 * @brief Produce a vector of contour definitions.
	 * @return Contour vector.
	 */
	ContourVector produceContourVecetor() const;
	
	/**
	 * @brief Get edge segments for a contour (non-const).
	 * @param contour Contour definition.
	 * @return Span of edge segments.
	 */
	std::span<EdgeSegment> getEdgeSegmentsForContour(const ContourDefinition& contour);
	
	/**
	 * @brief Get edge segments for a contour (const).
	 * @param contour Contour definition.
	 * @return Span of edge segments.
	 */
	std::span<const EdgeSegment> getEdgeSegmentsForContour(const ContourDefinition& contour) const;
	
	/**
	 * @brief Orient all contours consistently (outer CCW, inner CW).
	 */
	void orientContours();
	
	/**
	 * @brief Make shape IDs signed (positive for outer, negative for inner).
	 * @param flip Whether to flip the sign convention.
	 */
	void makeShapeIdsSigend(bool flip = false);
	
	/**
	 * @brief Assign colors to edges for MSDF generation (simple algorithm).
	 */
	void assignColours();
	
	/**
	 * @brief Assign colors to edges using msdfgen-style algorithm.
	 * @param angleThreshold Angle threshold in degrees for color assignment (default: 3.0).
	 * @param seed Random seed for color assignment (default: 1942).
	 */
	void assignColoursMsdfgen(double angleThreshold = 3.0, unsigned long long seed = 1942);
	
	/**
	 * @brief Clear all data from the context.
	 */
	void clear();
};

#endif // FONTOUTLINEDECOMPOSITIONCONTEXT_HPP
