#pragma once
#include "glm_macro.h"
#include <glm/glm.hpp>

class RigidTransform3D
{
public:
    RigidTransform3D();
    ~RigidTransform3D();

    glm::mat4x4 *getHomoMat();
    void setHomoMat(glm::mat4x4 *mat);

private:
    glm::mat4x4 *m_homo_mat;
};
