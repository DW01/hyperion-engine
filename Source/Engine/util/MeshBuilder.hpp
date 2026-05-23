/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/containers/Array.hpp>

#include <Core/reflection/Handle.hpp>

#include <Core/math/BoundingBox.hpp>
#include <Core/math/Vector3.hpp>
#include <Core/math/Vector4.hpp>

#include <rendering/Vertex.hpp>

namespace Hyperion {

class Mesh;
class VoxelOctree;

namespace MeshBuilder
{
/*! \brief Build a quad mesh in the XY plane with size 1x1. */
HYP_API Handle<Mesh> Quad();

/*! \brief Build a double-sided quad mesh in the XY plane with size 1x1.
    *  The back face has mirrored UVs so that rendered content (e.g. text)
    *  appears legible from both sides. */
HYP_API Handle<Mesh> DoubleSidedQuad();

/*! \brief Build a cube mesh.
    *  \param originOnBottom If true, the cube's origin will be at the bottom face center. Otherwise, it will be at the center of the cube.
    */
HYP_API Handle<Mesh> Cube(bool originOnBottom = false);

/*! \brief Build a UV sphere mesh with \p numSlices slices and \p numStacks stacks.
    *  \param numSlices Number of slices (longitude divisions).
    *  \param numStacks Number of stacks (latitude divisions).
    */
HYP_API Handle<Mesh> NormalizedCubeSphere(uint32 numDivisions);

HYP_API Handle<Mesh> ApplyTransform(const Mesh* mesh, const Transform& transform);
HYP_API Handle<Mesh> Merge(const Mesh* a, const Mesh* b, const Transform& aTransform, const Transform& bTransform);
HYP_API Handle<Mesh> Merge(const Mesh* a, const Mesh* b);

HYP_API Handle<Mesh> BuildVoxelMesh(const VoxelOctree& voxelOctree);

HYP_API Handle<Mesh> Cylinder(float radius, float height, uint32 numSegments);

HYP_API Handle<Mesh> Cone(float radius, float height, uint32 numSegments);

HYP_API Handle<Mesh> Torus(float majorRadius, float minorRadius, uint32 majorSegments, uint32 minorSegments);

} // namespace MeshBuilder
} // namespace Hyperion
